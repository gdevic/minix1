/* This file contains the main program of the File System.  It consists of
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are
 *   main:	main program of the File System
 *   reply:	send a reply to a process after the requested work is done
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"
#include "super.h"

#define M64K     0xFFFF0000L	/* 16 bit mask for DMA check */
#define INFO               2	/* where in data_org is info from build */
#define MAX_RAM          512	/* maxium RAM disk size in blocks */

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC main()
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int error;
  extern int (*call_vector[NCALLS])();

  fs_init();

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	get_work();		/* sets who and fs_call */

	fp = &fproc[who];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */
	dont_reply = FALSE;	/* in other words, do reply is default */

	/* Call the internal function that does the work. */
	if (fs_call < 0 || fs_call >= NCALLS)
		error = E_BAD_CALL;
	else
		error = (*call_vector[fs_call])();

	/* Copy the results back to the user and send reply. */
	if (dont_reply) continue;
	reply(who, error);
	if (rdahed_inode != NIL_INODE) read_ahead(); /* do block read ahead */
  }
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE get_work()
{  
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */

  register struct fproc *rp;

  if (reviving != 0) {
	/* Revive a suspended process. */
	for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) 
		if (rp->fp_revived == REVIVING) {
			who = rp - fproc;
			fs_call = rp->fp_fd & BYTE;
			fd = (rp->fp_fd >>8) & BYTE;
			buffer = rp->fp_buffer;
			nbytes = rp->fp_nbytes;
			rp->fp_suspended = NOT_SUSPENDED; /* no longer hanging*/
			rp->fp_revived = NOT_REVIVING;
			reviving--;
			return;
		}
	panic("get_work couldn't revive anyone", NO_NUM);
  }

  /* Normal case.  No one to revive. */
  if (receive(ANY, &m) != OK) panic("fs receive error", NO_NUM);

  who = m.m_source;
  fs_call = m.m_type;
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC reply(whom, result)
int whom;			/* process to reply to */
int result;			/* result of the call (usually OK or error #) */
{
/* Send a reply to a user process. It may fail (if the process has just
 * been killed by a signal, so don't check the return code.  If the send
 * fails, just ignore it.
 */

  reply_type = result;
  send(whom, &m1);
}

/*===========================================================================*
 *				fs_init					     *
 *===========================================================================*/
PRIVATE fs_init()
{
/* Initialize global variables, tables, etc. */

  register struct inode *rip;
  int i;
  extern struct inode *get_inode();

  buf_pool();			/* initialize buffer pool */
  load_ram();			/* Load RAM disk from root diskette. */
  load_super();			/* Load super block for root device */

  /* Initialize the 'fproc' fields for process 0 and process 2. */
  for (i = 0; i < 3; i+= 2) {
	fp = &fproc[i];
	rip = get_inode(ROOT_DEV, ROOT_INODE);
	fp->fp_rootdir = rip;
	dup_inode(rip);
	fp->fp_workdir = rip;
	fp->fp_realuid = (uid) SYS_UID;
	fp->fp_effuid = (uid) SYS_UID;
	fp->fp_realgid = (gid) SYS_GID;
	fp->fp_effgid = (gid) SYS_GID;
	fp->fp_umask = ~0;
  }

  /* Certain relations must hold for the file system to work at all. */
  if (ZONE_NUM_SIZE != 2) panic("ZONE_NUM_SIZE != 2", NO_NUM);
  if (SUPER_SIZE > BLOCK_SIZE) panic("SUPER_SIZE > BLOCK_SIZE", NO_NUM);
  if(BLOCK_SIZE % INODE_SIZE != 0)panic("BLOCK_SIZE % INODE_SIZE != 0", NO_NUM);
  if (NR_FDS > 127) panic("NR_FDS > 127", NO_NUM);
  if (NR_BUFS < 6) panic("NR_BUFS < 6", NO_NUM);
  if (sizeof(d_inode) != 32) panic("inode size != 32", NO_NUM);
}

/*===========================================================================*
 *				buf_pool				     *
 *===========================================================================*/
PRIVATE buf_pool()
{
/* Initialize the buffer pool.  On the IBM PC, the hardware DMA chip is
 * not able to cross 64K boundaries, so any buffer that happens to lie
 * across such a boundary is not used.  This is not very elegant, but all
 * the alternative solutions are as bad, if not worse.  The fault lies with
 * the PC hardware.
 */
  register struct buf *bp;
  vir_bytes low_off, high_off;
  phys_bytes org;
  extern phys_clicks get_base();

  bufs_in_use = 0;
  front = &buf[0];
  rear = &buf[NR_BUFS - 1];

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	bp->b_blocknr = NO_BLOCK;
	bp->b_dev = NO_DEV;
	bp->b_next = bp + 1;
	bp->b_prev = bp - 1;
  }
  buf[0].b_prev = NIL_BUF;
  buf[NR_BUFS - 1].b_next = NIL_BUF;

  /* Delete any buffers that span a 64K boundary. */
#ifdef i8088
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	org = get_base() << CLICK_SHIFT;	/* phys addr where FS is */
	low_off = (vir_bytes) bp->b_data;
	high_off = low_off + BLOCK_SIZE - 1;
	if (((org + low_off) & M64K) != ((org + high_off) & M64K)) {
		if (bp == &buf[0]) {
			front = &buf[1];
			buf[1].b_prev = NIL_BUF;
		} else if (bp == &buf[NR_BUFS - 1]) {
			rear = &buf[NR_BUFS - 2];
			buf[NR_BUFS - 2].b_next = NIL_BUF;
		} else {
			/* Delete a buffer in the middle. */
			bp->b_prev->b_next = bp + 1;
			bp->b_next->b_prev = bp - 1;
		}
	}
  }
#endif

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) bp->b_hash = bp->b_next;
  buf_hash[NO_BLOCK & (NR_BUF_HASH - 1)] = front;
}


/*===========================================================================*
 *				load_ram				     *
 *===========================================================================*/
PRIVATE load_ram()
{
/* The root diskette contains a block-by-block image of the root file system
 * starting at 0.  Go get it and copy it to the RAM disk. 
 */

  register struct buf *bp, *bp1;
  int count;
  long k_loaded;
  struct super_block *sp;
  block_nr i;
  phys_clicks ram_clicks, init_org, init_text_clicks, init_data_clicks;
  extern phys_clicks data_org[INFO + 2];
  extern struct buf *get_block();

  /* Get size of INIT by reading block on diskette where 'build' put it. */
  init_org = data_org[INFO];
  init_text_clicks = data_org[INFO + 1];
  init_data_clicks = data_org[INFO + 2];

  /* Get size of RAM disk by reading root file system's super block */
  bp = get_block(BOOT_DEV, SUPER_BLOCK, NORMAL);  /* get RAM super block */
  copy(super_block, bp->b_data, sizeof(struct super_block));
  sp = &super_block[0];
  if (sp->s_magic != SUPER_MAGIC)
	panic("Diskette in drive 0 is not root file system", NO_NUM);
  count = sp->s_nzones << sp->s_log_zone_size;	/* # blocks on root dev */
  if (count > MAX_RAM) panic("RAM disk is too big. # blocks = ", count);
  ram_clicks = count * (BLOCK_SIZE/CLICK_SIZE);
  put_block(bp, FULL_DATA_BLOCK);

  /* Tell MM the origin and size of INIT, and the amount of memory used for the
   * system plus RAM disk combined, so it can remove all of it from the map.
   */
  m1.m_type = BRK2;
  m1.m1_i1 = init_text_clicks;
  m1.m1_i2 = init_data_clicks;
  m1.m1_i3 = init_org + init_text_clicks + init_data_clicks + ram_clicks;
  m1.m1_p1 = (char *) init_org;
  if (sendrec(MM_PROC_NR, &m1) != OK) panic("FS Can't report to MM", NO_NUM);

  /* Tell RAM driver where RAM disk is and how big it is. */
  m1.m_type = DISK_IOCTL;
  m1.DEVICE = RAM_DEV;
  m1.POSITION = (long) init_org + (long) init_text_clicks + init_data_clicks;
  m1.POSITION = m1.POSITION << CLICK_SHIFT;
  m1.COUNT = count;
  if (sendrec(MEM, &m1) != OK) panic("Can't report size to MEM", NO_NUM);

  /* Copy the blocks one at a time from the root diskette to the RAM */
  printf("Loading RAM disk from root diskette.      Loaded:   0K ");
  for (i = 0; i < count; i++) {
	bp = get_block(BOOT_DEV, (block_nr) i, NORMAL);
	bp1 = get_block(ROOT_DEV, i, NO_READ);
	copy(bp1->b_data, bp->b_data, BLOCK_SIZE);
	bp1->b_dirt = DIRTY;
	put_block(bp, I_MAP_BLOCK);
	put_block(bp1, I_MAP_BLOCK);
	k_loaded = ( (long) i * BLOCK_SIZE)/1024L;	/* K loaded so far */
	if (k_loaded % 5 == 0) printf("\b\b\b\b\b%3DK %c", k_loaded, 0);
  }

  printf("\rRAM disk loaded.  Please remove root diskette.           \n\n");
}


/*===========================================================================*
 *				load_super				     *
 *===========================================================================*/
PRIVATE load_super()
{
  register struct super_block *sp;
  register struct inode *rip;
  extern struct inode *get_inode();

/* Initialize the super_block table. */

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  	sp->s_dev = NO_DEV;

  /* Read in super_block for the root file system. */
  sp = &super_block[0];
  sp->s_dev = ROOT_DEV;
  rw_super(sp,READING);
  rip = get_inode(ROOT_DEV, ROOT_INODE);	/* inode for root dir */

  /* Check super_block for consistency (is it the right diskette?). */
  if ( (rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3 ||
						sp->s_magic != SUPER_MAGIC)
	panic("Root file system corrupted.  Possibly wrong diskette.", NO_NUM);

  sp->s_imount = rip;
  dup_inode(rip);
  sp->s_isup = rip;
  sp->s_rd_only = 0;
  if (load_bit_maps(ROOT_DEV) != OK)
	panic("init: can't load root bit maps", NO_NUM);
}

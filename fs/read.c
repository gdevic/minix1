/* This file contains the heart of the mechanism used to read (and write)
 * files.  Read and write requests are split up into chunks that do not cross
 * block boundaries.  Each chunk is then processed in turn.  Reads on special
 * files are also detected and handled.
 *
 * The entry points into this file are
 *   do_read:	 perform the READ system call by calling read_write
 *   read_write: actually do the work of READ and WRITE
 *   read_map:	 given an inode and file position, lookup its zone number
 *   rw_user:	 call the kernel to read and write user space
 *   read_ahead: manage the block read ahead business
 */

#include "../h/const.h"
#include "../h/type.h"
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

#define FD_MASK          077	/* max file descriptor is 63 */

PRIVATE message umess;		/* message for asking SYSTASK for user copy */
PUBLIC int rdwt_err;		/* set to EIO if disk error occurs */

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
PUBLIC int do_read()
{
  return(read_write(READING));
}



/*===========================================================================*
 *				read_write				     *
 *===========================================================================*/
PUBLIC int read_write(rw_flag)
int rw_flag;			/* READING or WRITING */
{
/* Perform read(fd, buffer, nbytes) or write(fd, buffer, nbytes) call. */

  register struct inode *rip;
  register struct filp *f;
  register file_pos bytes_left, f_size;
  register unsigned off, cum_io;
  file_pos position;
  int r, chunk, virg, mode_word, usr, seg;
  struct filp *wf;
  extern struct super_block *get_super();
  extern struct filp *find_filp(), *get_filp();
  extern real_time clock_time();

  /* MM loads segments by putting funny things in upper 10 bits of 'fd'. */
  if (who == MM_PROC_NR && (fd & (~BYTE)) ) {
	usr = (fd >> 8) & BYTE;
	seg = (fd >> 6) & 03;
	fd &= FD_MASK;		/* get rid of user and segment bits */
  } else {
	usr = who;		/* normal case */
	seg = D;
  }

  /* If the file descriptor is valid, get the inode, size and mode. */
  if (nbytes == 0) return(0);	/* so char special files need not check for 0*/
  if (who != MM_PROC_NR && nbytes < 0) return(EINVAL);	/* only MM > 32K */
  if ( (f = get_filp(fd)) == NIL_FILP) return(err_code);
  if ( ((f->filp_mode) & (rw_flag == READING ? R_BIT : W_BIT)) == 0)
	return(EBADF);
  position = f->filp_pos;
  if (position < (file_pos) 0) return(EINVAL);
  rip = f->filp_ino;
  f_size = rip->i_size;
  r = OK;
  cum_io = 0;
  virg = TRUE;
  mode_word = rip->i_mode & I_TYPE;
  if (mode_word == I_BLOCK_SPECIAL && f_size == 0) f_size = MAX_P_LONG;
  rdwt_err = OK;		/* set to EIO if disk error occurs */

  /* Check for character special files. */
  if (mode_word == I_CHAR_SPECIAL) {
	if ((r = dev_io(rw_flag, (dev_nr) rip->i_zone[0], (long) position,
						nbytes, who, buffer)) >= 0) {
		cum_io = r;
		position += r;
		r = OK;
	}
  } else {
	if (rw_flag == WRITING && mode_word != I_BLOCK_SPECIAL) {
		/* Check in advance to see if file will grow too big. */
		if (position > get_super(rip->i_dev)->s_max_size - nbytes )
			return(EFBIG);

		/* Clear the zone containing present EOF if hole about
		 * to be created.  This is necessary because all unwritten
		 * blocks prior to the EOF must read as zeros.
		 */
		if (position > f_size) clear_zone(rip, f_size, 0);
	}

	/* Pipes are a little different.  Check. */
	if (rip->i_pipe && (r = pipe_check(rip, rw_flag, virg,
				nbytes, &position)) <= 0) return(r);

	/* Split the transfer into chunks that don't span two blocks. */
	while (nbytes != 0) {
		off = position % BLOCK_SIZE;	/* offset within a block */
		chunk = MIN(nbytes, BLOCK_SIZE - off);
		if (chunk < 0) chunk = BLOCK_SIZE - off;

		if (rw_flag == READING) {
			if ((bytes_left = f_size - position) <= 0)
				break;
			else
				if (chunk > bytes_left) chunk = bytes_left;
		}

		/* Read or write 'chunk' bytes. */
		r=rw_chunk(rip, position, off, chunk, rw_flag, buffer, seg,usr);
		if (r != OK) break;	/* EOF reached */
		if (rdwt_err < 0) break;

		/* Update counters and pointers. */
		buffer += chunk;	/* user buffer address */
		nbytes -= chunk;	/* bytes yet to be read */
		cum_io += chunk;	/* bytes read so far */
		position += chunk;	/* position within the file */
		virg = FALSE; /* tells pipe_check() that data has been copied */
	}
  }

  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	if (mode_word != I_CHAR_SPECIAL && mode_word != I_BLOCK_SPECIAL && 
							position > f_size)
		rip->i_size = position;
	rip->i_modtime = clock_time();
	rip->i_dirt = DIRTY;
  } else {
	if (rip->i_pipe && position >= rip->i_size) {
		/* Reset pipe pointers. */
		rip->i_size = 0;	/* no data left */
		position = 0;		/* reset reader(s) */
		if ( (wf = find_filp(rip, W_BIT)) != NIL_FILP) wf->filp_pos = 0;
	}
  }
  f->filp_pos = position;

  /* Check to see if read-ahead is called for, and if so, set it up. */
  if (rw_flag == READING && rip->i_seek == NO_SEEK && position % BLOCK_SIZE == 0
		&& (mode_word == I_REGULAR || mode_word == I_DIRECTORY)) {
	rdahed_inode = rip;
	rdahedpos = position;
  }
  if (mode_word == I_REGULAR) rip->i_seek = NO_SEEK;

  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == EOF) r = cum_io;
  return(r == OK ? cum_io : r);
}



/*===========================================================================*
 *				rw_chunk				     *
 *===========================================================================*/
PRIVATE int rw_chunk(rip, position, off, chunk, rw_flag, buff, seg, usr)
register struct inode *rip;	/* pointer to inode for file to be rd/wr */
file_pos position;		/* position within file to read or write */
unsigned off;			/* off within the current block */
int chunk;			/* number of bytes to read or write */
int rw_flag;			/* READING or WRITING */
char *buff;			/* virtual address of the user buffer */
int seg;			/* T or D segment in user space */
int usr;			/* which user process */
{
/* Read or write (part of) a block. */

  register struct buf *bp;
  register int r;
  int dir, n, block_spec;
  block_nr b;
  dev_nr dev;
  extern struct buf *get_block(), *new_block();
  extern block_nr read_map();

  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec) {
	b = position/BLOCK_SIZE;
	dev = (dev_nr) rip->i_zone[0];
  } else {
	b = read_map(rip, position);
	dev = rip->i_dev;
  }

  if (!block_spec && b == NO_BLOCK) {
	if (rw_flag == READING) {
		/* Reading from a nonexistent block.  Must read as all zeros. */
		bp = get_block(NO_DEV, NO_BLOCK, NORMAL);     /* get a buffer */
		zero_block(bp);
	} else {
		/* Writing to a nonexistent block. Create and enter in inode. */
		if ((bp = new_block(rip, position)) == NIL_BUF)return(err_code);
	}
  } else {
	/* Normally an existing block to be partially overwritten is first read
	 * in.  However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (rw_flag == WRITING && chunk == BLOCK_SIZE ? NO_READ : NORMAL);
	if(rw_flag == WRITING && off == 0 && position >= rip->i_size) n=NO_READ;
	bp = get_block(dev, b, n);
  }

  /* In all cases, bp now points to a valid buffer. */
  if (rw_flag == WRITING && chunk != BLOCK_SIZE && !block_spec &&
					position >= rip->i_size && off == 0)
	zero_block(bp);
  dir = (rw_flag == READING ? TO_USER : FROM_USER);
  r = rw_user(seg, usr, (vir_bytes)buff, (vir_bytes)chunk, bp->b_data+off, dir);
  if (rw_flag == WRITING) bp->b_dirt = DIRTY;
  n = (off + chunk == BLOCK_SIZE ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
  put_block(bp, n);
  return(r);
}



/*===========================================================================*
 *				read_map				     *
 *===========================================================================*/
PUBLIC block_nr read_map(rip, position)
register struct inode *rip;	/* ptr to inode to map from */
file_pos position;		/* position in file whose blk wanted */
{
/* Given an inode and a position within the corresponding file, locate the
 * block (not zone) number in which that position is to be found and return it.
 */

  register struct buf *bp;
  register zone_nr z;
  register block_nr b;
  register long excess, zone, block_pos;
  register int scale, boff;
  extern struct buf *get_block();

  scale = scale_factor(rip);	/* for block-zone conversion */
  block_pos = position/BLOCK_SIZE;	/* relative blk # in file */
  zone = block_pos >> scale;	/* position's zone */
  boff = block_pos - (zone << scale);	/* relative blk # within zone */

  /* Is 'position' to be found in the inode itself? */
  if (zone < NR_DZONE_NUM) {
	if ( (z = rip->i_zone[zone]) == NO_ZONE) return(NO_BLOCK);
	b = ((block_nr) z << scale) + boff;
	return(b);
  }

  /* It is not in the inode, so it must be single or double indirect. */
  excess = zone - NR_DZONE_NUM;	/* first NR_DZONE_NUM don't count */

  if (excess < NR_INDIRECTS) {
	/* 'position' can be located via the single indirect block. */
	z = rip->i_zone[NR_DZONE_NUM];
  } else {
	/* 'position' can be located via the double indirect block. */
	if ( (z = rip->i_zone[NR_DZONE_NUM+1]) == NO_ZONE) return(NO_BLOCK);
	excess -= NR_INDIRECTS;			/* single indir doesn't count */
	b = (block_nr) z << scale;
	bp = get_block(rip->i_dev, b, NORMAL);	/* get double indirect block */
	z = bp->b_ind[excess/NR_INDIRECTS];	/* z is zone # for single ind */
	put_block(bp, INDIRECT_BLOCK);		/* release double ind block */
	excess = excess % NR_INDIRECTS;		/* index into single ind blk */
  }

  /* 'z' is zone number for single indirect block; 'excess' is index into it. */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = (block_nr) z << scale;
  bp = get_block(rip->i_dev, b, NORMAL);	/* get single indirect block */
  z = bp->b_ind[excess];
  put_block(bp, INDIRECT_BLOCK);		/* release single indirect blk */
  if (z == NO_ZONE) return(NO_BLOCK);
  b = ((block_nr) z << scale) + boff;
  return(b);
}

/*===========================================================================*
 *				rw_user					     *
 *===========================================================================*/
PUBLIC int rw_user(s, u, vir, bytes, buff, direction)
int s;				/* D or T space (stack is also D) */
int u;				/* process number to r/w (usually = 'who') */
vir_bytes vir;			/* virtual address to move to/from */
vir_bytes bytes;		/* how many bytes to move */
char *buff;			/* pointer to FS space */
int direction;			/* TO_USER or FROM_USER */
{
/* Transfer a block of data.  Two options exist, depending on 'direction':
 *     TO_USER:     Move from FS space to user virtual space
 *     FROM_USER:   Move from user virtual space to FS space
 */

  if (direction == TO_USER ) {
	/* Write from FS space to user space. */
	umess.SRC_SPACE  = D;
	umess.SRC_PROC_NR = FS_PROC_NR;
	umess.SRC_BUFFER = (long) buff;
	umess.DST_SPACE  = s;
	umess.DST_PROC_NR = u;
	umess.DST_BUFFER = (long) vir;
  } else {
	/* Read from user space to FS space. */
	umess.SRC_SPACE  = s;
	umess.SRC_PROC_NR = u;
	umess.SRC_BUFFER = (long) vir;
	umess.DST_SPACE  = D;
	umess.DST_PROC_NR = FS_PROC_NR;
	umess.DST_BUFFER = (long) buff;
  }

  umess.COPY_BYTES = (long) bytes;
  sys_copy(&umess);
  return(umess.m_type);
}


/*===========================================================================*
 *				read_ahead				     *
 *===========================================================================*/
PUBLIC read_ahead()
{
/* Read a block into the cache before it is needed. */

  register struct inode *rip;
  struct buf *bp;
  block_nr b;
  extern struct buf *get_block();

  rip = rdahed_inode;		/* pointer to inode to read ahead from */
  rdahed_inode = NIL_INODE;	/* turn off read ahead */
  if ( (b = read_map(rip, rdahedpos)) == NO_BLOCK) return;	/* at EOF */
  bp = get_block(rip->i_dev, b, NORMAL);
  put_block(bp, PARTIAL_DATA_BLOCK);
}

/* This file manages the inode table.  There are procedures to allocate and
 * deallocate inodes, acquire, erase, and release them, and read and write
 * them from the disk.
 *
 * The entry points into this file are
 *   get_inode:	  search inode table for a given inode; if not there, read it
 *   put_inode:	  indicate that an inode is no longer needed in memory
 *   alloc_inode: allocate a new, unused inode
 *   wipe_inode:  erase some fields of a newly allocated inode
 *   free_inode:  mark an inode as available for a new file
 *   rw_inode:	  read a disk block and extract an inode, or corresp. write
 *   dup_inode:	  indicate that someone else is using an inode table entry
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "super.h"

/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
PUBLIC struct inode *get_inode(dev, numb)
dev_nr dev;			/* device on which inode resides */
inode_nr numb;			/* inode number */
{
/* Find a slot in the inode table, load the specified inode into it, and
 * return a pointer to the slot.  If 'dev' == NO_DEV, just return a free slot.
 */

  register struct inode *rip, *xp;

  /* Search the inode table both for (dev, numb) and a free slot. */
  xp = NIL_INODE;
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++) {
	if (rip->i_count > 0) { /* only check used slots for (dev, numb) */
		if (rip->i_dev == dev && rip->i_num == numb) {
			/* This is the inode that we are looking for. */
			rip->i_count++;
			return(rip);	/* (dev, numb) found */
		}
	} else
		xp = rip;	/* remember this free slot for later */
  }

  /* Inode we want is not currently in use.  Did we find a free slot? */
  if (xp == NIL_INODE) {	/* inode table completely full */
	err_code = ENFILE;
	return(NIL_INODE);
  }

  /* A free inode slot has been located.  Load the inode into it. */
  xp->i_dev = dev;
  xp->i_num = numb;
  xp->i_count = 1;
  if (dev != NO_DEV) rw_inode(xp, READING);	/* get inode from disk */

  return(xp);
}


/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
PUBLIC put_inode(rip)
register struct inode *rip;	/* pointer to inode to be released */
{
/* The caller is no longer using this inode.  If no one else is using it either
 * write it back to the disk immediately.  If it has no links, truncate it and
 * return it to the pool of available inodes.
 */

  if (rip == NIL_INODE) return;	/* checking here is easier than in caller */
  if (--rip->i_count == 0) {	/* i_count == 0 means no one is using it now */
	if ((rip->i_nlinks & BYTE) == 0) {
		/* i_nlinks == 0 means free the inode. */
		truncate(rip);	/* return all the disk blocks */
		rip->i_mode = I_NOT_ALLOC;	/* clear I_TYPE field */
		rip->i_pipe = NO_PIPE;
		free_inode(rip->i_dev, rip->i_num);
	}

	if (rip->i_dirt == DIRTY) rw_inode(rip, WRITING);
  }
}

/*===========================================================================*
 *				alloc_inode				     *
 *===========================================================================*/
PUBLIC struct inode *alloc_inode(dev, bits)
dev_nr dev;			/* device on which to allocate the inode */
mask_bits bits;			/* mode of the inode */
{
/* Allocate a free inode on 'dev', and return a pointer to it. */

  register struct inode *rip;
  register struct super_block *sp;
  int major, minor;
  inode_nr numb;
  bit_nr b;
  extern bit_nr alloc_bit();
  extern struct inode *get_inode();
  extern struct super_block *get_super();

  /* Acquire an inode from the bit map. */
  sp = get_super(dev);		/* get pointer to super_block */
  b=alloc_bit(sp->s_imap, (bit_nr)sp->s_ninodes+1, sp->s_imap_blocks,(bit_nr)0);
  if (b == NO_BIT) {
	err_code = ENFILE;
	major = (int) (sp->s_dev >> MAJOR) & BYTE;
	minor = (int) (sp->s_dev >> MINOR) & BYTE;
	if (sp->s_dev == ROOT_DEV)
		printf("Out of i-nodes on root device (RAM disk)\n");
	else
		printf("Out of i-nodes on device %d/%d\n", major, minor);
	return(NIL_INODE);
  }
  numb = (inode_nr) b;

  /* Try to acquire a slot in the inode table. */
  if ( (rip = get_inode(NO_DEV, numb)) == NIL_INODE) {
	/* No inode table slots available.  Free the inode just allocated. */
	free_bit(sp->s_imap, b);
  } else {
	/* An inode slot is available.  Put the inode just allocated into it. */
	rip->i_mode = bits;
	rip->i_nlinks = (links) 0;
	rip->i_uid = fp->fp_effuid;
	rip->i_gid = fp->fp_effgid;
	rip->i_dev = dev;	/* was provisionally set to NO_DEV */

	/* The fields not cleared already are cleared in wipe_inode().  They have
	 * been put there because truncate() needs to clear the same fields if
	 * the file happens to be open while being truncated.  It saves space
	 * not to repeat the code twice.
	 */
	wipe_inode(rip);
  }

  return(rip);
}

/*===========================================================================*
 *				wipe_inode				     *
 *===========================================================================*/
PUBLIC wipe_inode(rip)
register struct inode *rip;	/* The inode to be erased. */
{
/* Erase some fields in the inode.  This function is called from alloc_inode()
 * when a new inode is to be allocated, and from truncate(), when an existing
 * inode is to be truncated.
 */

  register int i;
  extern real_time clock_time();

  rip->i_size = 0;
  rip->i_modtime = clock_time();
  rip->i_dirt = DIRTY;
  for (i = 0; i < NR_ZONE_NUMS; i++)
	rip->i_zone[i] = NO_ZONE;
}


/*===========================================================================*
 *				free_inode				     *
 *===========================================================================*/
PUBLIC free_inode(dev, numb)
dev_nr dev;			/* on which device is the inode */
inode_nr numb;			/* number of inode to be freed */
{
/* Return an inode to the pool of unallocated inodes. */

  register struct super_block *sp;
  extern struct super_block *get_super();

  /* Locate the appropriate super_block. */
  sp = get_super(dev);
  free_bit(sp->s_imap, (bit_nr) numb);
}


/*===========================================================================*
 *				rw_inode				     *
 *===========================================================================*/
PUBLIC rw_inode(rip, rw_flag)
register struct inode *rip;	/* pointer to inode to be read/written */
int rw_flag;			/* READING or WRITING */
{
/* An entry in the inode table is to be copied to or from the disk. */

  register struct buf *bp;
  register d_inode *dip;
  register struct super_block *sp;
  block_nr b;
  extern struct buf *get_block();
  extern struct super_block *get_super();

  /* Get the block where the inode resides. */
  sp = get_super(rip->i_dev);
  b = (block_nr) (rip->i_num - 1)/INODES_PER_BLOCK +
				sp->s_imap_blocks + sp->s_zmap_blocks + 2;
  bp = get_block(rip->i_dev, b, NORMAL);
  dip = bp->b_inode + (rip->i_num - 1) % INODES_PER_BLOCK;

  /* Do the read or write. */
  if (rw_flag == READING) {
	copy((char *)rip, (char *) dip, INODE_SIZE); /* copy from blk to inode */
  } else {
	copy((char *)dip, (char *) rip, INODE_SIZE); /* copy from inode to blk */
	bp->b_dirt = DIRTY;
  }

  put_block(bp, INODE_BLOCK);
  rip->i_dirt = CLEAN;
}


/*===========================================================================*
 *				dup_inode				     *
 *===========================================================================*/
PUBLIC dup_inode(ip)
struct inode *ip;		/* The inode to be duplicated. */
{
/* This routine is a simplified form of get_inode() for the case where
 * the inode pointer is already known.
 */

  ip->i_count++;
}

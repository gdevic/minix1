/* This file is the counterpart of "read.c".  It contains the code for writing
 * insofar as this is not contained in read_write().
 *
 * The entry points into this file are
 *   do_write:     call read_write to perform the WRITE system call
 *   write_map:    add a new zone to an inode
 *   clear_zone:   erase a zone in the middle of a file
 *   new_block:    acquire a new block
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
 *				do_write				     *
 *===========================================================================*/
PUBLIC int do_write()
{
/* Perform the write(fd, buffer, nbytes) system call. */
  return(read_write(WRITING));
}


/*===========================================================================*
 *				write_map				     *
 *===========================================================================*/
PRIVATE int write_map(rip, position, new_zone)
register struct inode *rip;	/* pointer to inode to be changed */
file_pos position;		/* file address to be mapped */
zone_nr new_zone;		/* zone # to be inserted */
{
/* Write a new zone into an inode. */
  int scale;
  zone_nr z, *zp;
  register block_nr b;
  long excess, zone;
  int index;
  struct buf *bp;
  int new_ind, new_dbl;

  extern zone_nr alloc_zone();
  extern struct buf *get_block();
  extern real_time clock_time();

  rip->i_dirt = DIRTY;		/* inode will be changed */
  bp = NIL_BUF;
  scale = scale_factor(rip);	/* for zone-block conversion */
  zone = (position/BLOCK_SIZE) >> scale;	/* relative zone # to insert */

  /* Is 'position' to be found in the inode itself? */
  if (zone < NR_DZONE_NUM) {
	rip->i_zone[zone] = new_zone;
	rip->i_modtime = clock_time();
	return(OK);
  }

  /* It is not in the inode, so it must be single or double indirect. */
  excess = zone - NR_DZONE_NUM;	/* first NR_DZONE_NUM don't count */
  new_ind = FALSE;
  new_dbl = FALSE;

  if (excess < NR_INDIRECTS) {
	/* 'position' can be located via the single indirect block. */
	zp = &rip->i_zone[NR_DZONE_NUM];
  } else {
	/* 'position' can be located via the double indirect block. */
	if ( (z = rip->i_zone[NR_DZONE_NUM+1]) == NO_ZONE) {
		/* Create the double indirect block. */
		if ( (z = alloc_zone(rip->i_dev, rip->i_zone[0])) == NO_ZONE)
			return(err_code);
		rip->i_zone[NR_DZONE_NUM+1] = z;
		new_dbl = TRUE;	/* set flag for later */
	}

	/* Either way, 'z' is zone number for double indirect block. */
	excess -= NR_INDIRECTS;	/* single indirect doesn't count */
	index = excess / NR_INDIRECTS;
	excess = excess % NR_INDIRECTS;
	if (index >= NR_INDIRECTS) return(EFBIG);
	b = (block_nr) z << scale;
	bp = get_block(rip->i_dev, b, (new_dbl ? NO_READ : NORMAL));
	if (new_dbl) zero_block(bp);
	zp= &bp->b_ind[index];
  }

  /* 'zp' now points to place where indirect zone # goes; 'excess' is index. */
  if (*zp == NO_ZONE) {
	/* Create indirect block. */
	*zp = alloc_zone(rip->i_dev, rip->i_zone[0]);
	new_ind = TRUE;
	if (bp != NIL_BUF) bp->b_dirt = DIRTY;	/* if double ind, it is dirty */
	if (*zp == NO_ZONE) {
		put_block(bp, INDIRECT_BLOCK);	/* release dbl indirect blk */
		return(err_code);	/* couldn't create single ind */
	}
  }
  put_block(bp, INDIRECT_BLOCK);	/* release double indirect blk */

  /* 'zp' now points to indirect block's zone number. */
  b = (block_nr) *zp << scale;
  bp = get_block(rip->i_dev, b, (new_ind ? NO_READ : NORMAL) );
  if (new_ind) zero_block(bp);
  bp->b_ind[excess] = new_zone;
  rip->i_modtime = clock_time();
  bp->b_dirt = DIRTY;
  put_block(bp, INDIRECT_BLOCK);

  return(OK);
}

/*===========================================================================*
 *				clear_zone				     *
 *===========================================================================*/
PUBLIC clear_zone(rip, pos, flag)
register struct inode *rip;	/* inode to clear */
file_pos pos;			/* points to block to clear */
int flag;			/* 0 if called by read_write, 1 by new_block */
{
/* Zero a zone, possibly starting in the middle.  The parameter 'pos' gives
 * a byte in the first block to be zeroed.  Clearzone() is called from 
 * read_write and new_block().
 */

  register struct buf *bp;
  register block_nr b, blo, bhi;
  register file_pos next;
  register int scale;
  register zone_type zone_size;
  extern struct buf *get_block();
  extern block_nr read_map();

  /* If the block size and zone size are the same, clear_zone() not needed. */
  if ( (scale = scale_factor(rip)) == 0) return;


  zone_size = (zone_type) BLOCK_SIZE << scale;
  if (flag == 1) pos = (pos/zone_size) * zone_size;
  next = pos + BLOCK_SIZE - 1;

  /* If 'pos' is in the last block of a zone, do not clear the zone. */
  if (next/zone_size != pos/zone_size) return;
  if ( (blo = read_map(rip, next)) == NO_BLOCK) return;
  bhi = (  ((blo>>scale)+1) << scale)   - 1;

  /* Clear all the blocks between 'blo' and 'bhi'. */
  for (b = blo; b <= bhi; b++) {
	bp = get_block(rip->i_dev, b, NO_READ);
	zero_block(bp);
	put_block(bp, FULL_DATA_BLOCK);
  }
}


/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
PUBLIC struct buf *new_block(rip, position)
register struct inode *rip;	/* pointer to inode */
file_pos position;		/* file pointer */
{
/* Acquire a new block and return a pointer to it.  Doing so may require
 * allocating a complete zone, and then returning the initial block.
 * On the other hand, the current zone may still have some unused blocks.
 */

  register struct buf *bp;
  block_nr b, base_block;
  zone_nr z;
  zone_type zone_size;
  int scale, r;
  struct super_block *sp;
  extern struct buf *get_block();
  extern struct super_block *get_super();
  extern block_nr read_map();
  extern zone_nr alloc_zone();

  /* Is another block available in the current zone? */
  if ( (b = read_map(rip, position)) == NO_BLOCK) {
	/* Choose first zone if need be. */
	if (rip->i_size == 0) {
		sp = get_super(rip->i_dev);
		z = sp->s_firstdatazone;
	} else {
		z = rip->i_zone[0];
	}
	if ( (z = alloc_zone(rip->i_dev, z)) == NO_ZONE) return(NIL_BUF);
	if ( (r = write_map(rip, position, z)) != OK) {
		free_zone(rip->i_dev, z);
		err_code = r;
		return(NIL_BUF);
	}

	/* If we are not writing at EOF, clear the zone, just to be safe. */
	if ( position != rip->i_size) clear_zone(rip, position, 1);
	scale = scale_factor(rip);
	base_block = (block_nr) z << scale;
	zone_size = (zone_type) BLOCK_SIZE << scale;
	b = base_block + (block_nr)((position % zone_size)/BLOCK_SIZE);
  }

  bp = get_block(rip->i_dev, b, NO_READ);
  zero_block(bp);
  return(bp);
}


/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
PUBLIC zero_block(bp)
register struct buf *bp;	/* pointer to buffer to zero */
{
/* Zero a block. */

  register int n;
  register int *zip;

  n = INTS_PER_BLOCK;		/* number of integers in a block */
  zip = bp->b_int;		/* where to start clearing */

  do { *zip++ = 0;}  while (--n);
  bp->b_dirt = DIRTY;
}

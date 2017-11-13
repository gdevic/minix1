/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file manages the
 * cache.
 *
 * The entry points into this file are:
 *   get_block:	  request to fetch a block for reading or writing from cache
 *   put_block:	  return a block previously requested with get_block
 *   alloc_zone:  allocate a new zone (to increase the length of a file)
 *   free_zone:	  release a zone (when a file is removed)
 *   rw_block:	  read or write a block from the disk itself
 *   invalidate:  remove all the cache blocks on some device
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
 *				get_block				     *
 *===========================================================================*/
PUBLIC struct buf *get_block(dev, block, only_search)
register dev_nr dev;		/* on which device is the block? */
register block_nr block;	/* which block is wanted? */
int only_search;		/* if NO_READ, don't read, else act normal */
{
/* Check to see if the requested block is in the block cache.  If so, return
 * a pointer to it.  If not, evict some other block and fetch it (unless
 * 'only_search' is 1).  All blocks in the cache, whether in use or not,
 * are linked together in a chain, with 'front' pointing to the least recently
 * used block and 'rear' to the most recently used block.  If 'only_search' is
 * 1, the block being requested will be overwritten in its entirety, so it is
 * only necessary to see if it is in the cache; if it is not, any free buffer
 * will do.  It is not necessary to actually read the block in from disk.
 * In addition to the LRU chain, there is also a hash chain to link together
 * blocks whose block numbers end with the same bit strings, for fast lookup.
 */

  register struct buf *bp, *prev_ptr;

  /* Search the list of blocks not currently in use for (dev, block). */
  bp = buf_hash[block & (NR_BUF_HASH - 1)];	/* search the hash chain */
  if (dev != NO_DEV) {
	while (bp != NIL_BUF) {
		if (bp->b_blocknr == block && bp->b_dev == dev) {
			/* Block needed has been found. */
			if (bp->b_count == 0) bufs_in_use++;
			bp->b_count++;	/* record that block is in use */
			return(bp);
		} else {
			/* This block is not the one sought. */
			bp = bp->b_hash; /* move to next block on hash chain */
		}
	}
  }

  /* Desired block is not on available chain.  Take oldest block ('front').
   * However, a block that is aready in use (b_count > 0) may not be taken.
   */
  if (bufs_in_use == NR_BUFS) panic("All buffers in use", NR_BUFS);
  bufs_in_use++;		/* one more buffer in use now */
  bp = front;
  while (bp->b_count > 0 && bp->b_next != NIL_BUF) bp = bp->b_next;
  if (bp == NIL_BUF || bp->b_count > 0) panic("No free buffer", NO_NUM);

  /* Remove the block that was just taken from its hash chain. */
  prev_ptr = buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)];
  if (prev_ptr == bp) {
	buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)] = bp->b_hash;
  } else {
	/* The block just taken is not on the front of its hash chain. */
	while (prev_ptr->b_hash != NIL_BUF)
		if (prev_ptr->b_hash == bp) {
			prev_ptr->b_hash = bp->b_hash;	/* found it */
			break;
		} else {
			prev_ptr = prev_ptr->b_hash;	/* keep looking */
		}
  }

  /* If the  block taken is dirty, make it clean by rewriting it to disk. */
  if (bp->b_dirt == DIRTY && bp->b_dev != NO_DEV) rw_block(bp, WRITING);

  /* Fill in block's parameters and add it to the hash chain where it goes. */
  bp->b_dev = dev;		/* fill in device number */
  bp->b_blocknr = block;	/* fill in block number */
  bp->b_count++;		/* record that block is being used */
  bp->b_hash = buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)];
  buf_hash[bp->b_blocknr & (NR_BUF_HASH - 1)] = bp;	/* add to hash list */

  /* Go get the requested block, unless only_search = NO_READ. */
  if (dev != NO_DEV && only_search == NORMAL) rw_block(bp, READING);
  return(bp);			/* return the newly acquired block */
}


/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
PUBLIC put_block(bp, block_type)
register struct buf *bp;	/* pointer to the buffer to be released */
int block_type;			/* INODE_BLOCK, DIRECTORY_BLOCK, or whatever */
{
/* Return a block to the list of available blocks.   Depending on 'block_type'
 * it may be put on the front or rear of the LRU chain.  Blocks that are
 * expected to be needed again shortly (e.g., partially full data blocks)
 * go on the rear; blocks that are unlikely to be needed again shortly
 * (e.g., full data blocks) go on the front.  Blocks whose loss can hurt
 * the integrity of the file system (e.g., inode blocks) are written to
 * disk immediately if they are dirty.  
 */

  register struct buf *next_ptr, *prev_ptr;

  if (bp == NIL_BUF) return;	/* it is easier to check here than in caller */

  /* If block is no longer in use, first remove it from LRU chain. */
  bp->b_count--;		/* there is one use fewer now */
  if (bp->b_count > 0) return;	/* block is still in use */

  bufs_in_use--;		/* one fewer block buffers in use */
  next_ptr = bp->b_next;	/* successor on LRU chain */
  prev_ptr = bp->b_prev;	/* predecessor on LRU chain */
  if (prev_ptr != NIL_BUF)
	prev_ptr->b_next = next_ptr;
  else
	front = next_ptr;	/* this block was at front of chain */

  if (next_ptr != NIL_BUF)
	next_ptr->b_prev = prev_ptr;
  else
	rear = prev_ptr;	/* this block was at rear of chain */

  /* Put this block back on the LRU chain.  If the ONE_SHOT bit is set in
   * 'block_type', the block is not likely to be needed again shortly, so put
   * it on the front of the LRU chain where it will be the first one to be
   * taken when a free buffer is needed later.
   */
  if (block_type & ONE_SHOT) {
	/* Block probably won't be needed quickly. Put it on front of chain.
  	 * It will be the next block to be evicted from the cache.
  	 */
	bp->b_prev = NIL_BUF;
	bp->b_next = front;
	if (front == NIL_BUF)
		rear = bp;	/* LRU chain was empty */
	else
		front->b_prev = bp;
	front = bp;
  } else {
	/* Block probably will be needed quickly.  Put it on rear of chain.
  	 * It will not be evicted from the cache for a long time.
  	 */
	bp->b_prev = rear;
	bp->b_next = NIL_BUF;
	if (rear == NIL_BUF)
		front = bp;
	else
		rear->b_next = bp;
	rear = bp;
  }

  /* Some blocks are so important (e.g., inodes, indirect blocks) that they
   * should be written to the disk immediately to avoid messing up the file
   * system in the event of a crash.
   */
  if ((block_type & WRITE_IMMED) && bp->b_dirt==DIRTY && bp->b_dev != NO_DEV)
	rw_block(bp, WRITING);

  /* Super blocks must not be cached, lest mount use cached block. */
  if (block_type == ZUPER_BLOCK) bp->b_dev = NO_DEV;
}


/*===========================================================================*
 *				alloc_zone				     *
 *===========================================================================*/
PUBLIC zone_nr alloc_zone(dev, z)
dev_nr dev;			/* device where zone wanted */
zone_nr z;			/* try to allocate new zone near this one */
{
/* Allocate a new zone on the indicated device and return its number. */

  bit_nr b, bit;
  struct super_block *sp;
  int major, minor;
  extern bit_nr alloc_bit();
  extern struct super_block *get_super();

  /* Note that the routine alloc_bit() returns 1 for the lowest possible
   * zone, which corresponds to sp->s_firstdatazone.  To convert a value
   * between the bit number, 'b', used by alloc_bit() and the zone number, 'z',
   * stored in the inode, use the formula:
   *     z = b + sp->s_firstdatazone - 1
   * Alloc_bit() never returns 0, since this is used for NO_BIT (failure).
   */
  sp = get_super(dev);		/* find the super_block for this device */
  bit = (bit_nr) z - (sp->s_firstdatazone - 1);
  b = alloc_bit(sp->s_zmap, (bit_nr) sp->s_nzones - sp->s_firstdatazone + 1,
						sp->s_zmap_blocks, bit);
  if (b == NO_BIT) {
	err_code = ENOSPC;
	major = (int) (sp->s_dev >> MAJOR) & BYTE;
	minor = (int) (sp->s_dev >> MINOR) & BYTE;
	if (sp->s_dev == ROOT_DEV)
		printf("No space on root device (RAM disk)\n");
	else
		printf("No space on device %d/%d\n", major, minor);
	return(NO_ZONE);
  }
  return(sp->s_firstdatazone - 1 + (zone_nr) b);
}


/*===========================================================================*
 *				free_zone				     *
 *===========================================================================*/
PUBLIC free_zone(dev, numb)
dev_nr dev;				/* device where zone located */
zone_nr numb;				/* zone to be returned */
{
/* Return a zone. */

  register struct super_block *sp;
  extern struct super_block *get_super();

  if (numb == NO_ZONE) return;	/* checking here easier than in caller */

  /* Locate the appropriate super_block and return bit. */
  sp = get_super(dev);
  free_bit(sp->s_zmap, (bit_nr) numb - (sp->s_firstdatazone - 1) );
}


/*===========================================================================*
 *				rw_block				     *
 *===========================================================================*/
PUBLIC rw_block(bp, rw_flag)
register struct buf *bp;	/* buffer pointer */
int rw_flag;			/* READING or WRITING */
{
/* Read or write a disk block. This is the only routine in which actual disk
 * I/O is invoked.  If an error occurs, a message is printed here, but the error
 * is not reported to the caller.  If the error occurred while purging a block
 * from the cache, it is not clear what the caller could do about it anyway.
 */

  int r;
  long pos;
  dev_nr dev;
  extern int rdwt_err;

  if (bp->b_dev != NO_DEV) {
	pos = (long) bp->b_blocknr * BLOCK_SIZE;
	r = dev_io(rw_flag, bp->b_dev, pos, BLOCK_SIZE, FS_PROC_NR, bp->b_data);
	if (r < 0) {
		dev = bp->b_dev;
		if (r != EOF) {
		  printf("Unrecoverable disk error on device %d/%d, block %d\n",
			(dev>>MAJOR)&BYTE, (dev>>MINOR)&BYTE, bp->b_blocknr);
		} else {
			bp->b_dev = NO_DEV;	/* invalidate block */
		}
		rdwt_err = r;	/* report error to interested parties */
	}
  }

  bp->b_dirt = CLEAN;
}


/*===========================================================================*
 *				invalidate				     *
 *===========================================================================*/
PUBLIC invalidate(device)
dev_nr device;			/* device whose blocks are to be purged */
{
/* Remove all the blocks belonging to some device from the cache. */

  register struct buf *bp;

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++)
	if (bp->b_dev == device) bp->b_dev = NO_DEV;
}

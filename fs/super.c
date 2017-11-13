/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   load_bit_maps:   get the bit maps for the root or a newly mounted device
 *   unload_bit_maps: write the bit maps back to disk after an UMOUNT
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 *   get_super:       search the 'superblock' table for a device
 *   mounted:         tells if file inode is on mounted (or ROOT) file system
 *   scale_factor:    get the zone-to-block conversion factor for a device
 *   rw_super:        read or write a superblock
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "inode.h"
#include "super.h"

#define INT_BITS (sizeof(int)<<3)
#define BIT_MAP_SHIFT     13	/* (log2 of BLOCK_SIZE) + 3; 13 for 1k blocks */

/*===========================================================================*
 *				load_bit_maps				     *
 *===========================================================================*/
PUBLIC int load_bit_maps(dev)
dev_nr dev;			/* which device? */
{
/* Load the bit map for some device into the cache and set up superblock. */

  register int i;
  register struct super_block *sp;
  block_nr zbase;
  extern struct buf *get_block();
  extern struct super_block *get_super();

  sp = get_super(dev);		/* get the superblock pointer */
  if (bufs_in_use + sp->s_imap_blocks + sp->s_zmap_blocks >= NR_BUFS - 3)
	return(ERROR);		/* insufficient buffers left for bit maps */
  if (sp->s_imap_blocks > I_MAP_SLOTS || sp->s_zmap_blocks > ZMAP_SLOTS)
	panic("too many map blocks", NO_NUM);

  /* Load the inode map from the disk. */
  for (i = 0; i < sp->s_imap_blocks; i++)
	sp->s_imap[i] = get_block(dev, SUPER_BLOCK + 1 + i, NORMAL);

  /* Load the zone map from the disk. */
  zbase = SUPER_BLOCK + 1 + sp->s_imap_blocks;
  for (i = 0; i < sp->s_zmap_blocks; i++)
	sp->s_zmap[i] = get_block(dev, zbase + i, NORMAL);

  /* inodes 0 and 1, and zone 0 are never allocated.  Mark them as busy. */
  sp->s_imap[0]->b_int[0] |= 3;	/* inodes 0, 1 busy */
  sp->s_zmap[0]->b_int[0] |= 1;	/* zone 0 busy */
  bufs_in_use += sp->s_imap_blocks + sp->s_zmap_blocks;
  return(OK);
}



/*===========================================================================*
 *				unload_bit_maps				     *
 *===========================================================================*/
PUBLIC unload_bit_maps(dev)
dev_nr dev;			/* which device is being unmounted? */
{
/* Unload the bit maps so a device can be unmounted. */

  register int i;
  register struct super_block *sp;
  struct super_block *get_super();

  sp = get_super(dev);		/* get the superblock pointer */
  bufs_in_use -= sp->s_imap_blocks + sp->s_zmap_blocks;
  for (i = 0; i < sp->s_imap_blocks; i++) put_block(sp->s_imap[i], I_MAP_BLOCK);
  for (i = 0; i < sp->s_zmap_blocks; i++) put_block(sp->s_zmap[i], ZMAP_BLOCK);
  return(OK);
}


/*===========================================================================*
 *				alloc_bit				     *
 *===========================================================================*/
PUBLIC bit_nr alloc_bit(map_ptr, map_bits, bit_blocks, origin)
struct buf *map_ptr[];		/* pointer to array of bit block pointers */
bit_nr map_bits;		/* how many bits are there in the bit map? */
unshort bit_blocks;		/* how many blocks are there in the bit map? */
bit_nr origin;			/* number of bit to start searching at */
{
/* Allocate a bit from a bit map and return its bit number. */

  register unsigned k;
  register int *wptr, *wlim;
  int i, a, b, w, o, block_count;
  struct buf *bp;

  /* Figure out where to start the bit search (depends on 'origin'). */
  if (origin >= map_bits) origin = 0;	/* for robustness */
  b = origin >> BIT_MAP_SHIFT;
  o = origin - (b << BIT_MAP_SHIFT);
  w = o/INT_BITS;
  block_count = (w == 0 ? bit_blocks : bit_blocks + 1);

  /* The outer while loop iterates on the blocks of the map.  The inner
   * while loop iterates on the words of a block.  The for loop iterates
   * on the bits of a word.
   */
  while (block_count--) {
	/* If need be, loop on all the blocks in the bit map. */
	bp = map_ptr[b];
	wptr = &bp->b_int[w];
	wlim = &bp->b_int[INTS_PER_BLOCK];
	while (wptr != wlim) {
		/* Loop on all the words of one of the bit map blocks. */
		if ((k = (unsigned) *wptr) != (unsigned) ~0) {
			/* This word contains a free bit.  Allocate it. */
			for (i = 0; i < INT_BITS; i++)
				if (((k >> i) & 1) == 0) {
					a = i + (wptr - &bp->b_int[0])*INT_BITS
							+ (b << BIT_MAP_SHIFT);
					/* If 'a' beyond map check other blks*/
					if (a >= map_bits) {
						wptr = wlim - 1;
						break;
					}
					*wptr |= 1 << i;
					bp->b_dirt = DIRTY;
					return( (bit_nr) a);
				}
		}
		wptr++;		/* examine next word in this bit map block */
	}
	if (++b == bit_blocks) b = 0;	/* we have wrapped around */
	w = 0;
  }
  return(NO_BIT);		/* no bit could be allocated */
}


/*===========================================================================*
 *				free_bit				     *
 *===========================================================================*/
PUBLIC free_bit(map_ptr, bit_returned)
struct buf *map_ptr[];		/* pointer to array of bit block pointers */
bit_nr bit_returned;		/* number of bit to insert into the map */
{
/* Return a zone or inode by turning on its bitmap bit. */

  int b, r, w, bit;
  struct buf *bp;

  b = bit_returned >> BIT_MAP_SHIFT;	/* 'b' tells which block it is in */
  r = bit_returned - (b << BIT_MAP_SHIFT);
  w = r/INT_BITS;		/* 'w' tells which word it is in */
  bit = r % INT_BITS;
  bp = map_ptr[b];
  if (bp == NIL_BUF) return;
  if (((bp->b_int[w] >> bit)& 1)== 0)
       panic("freeing unused block or inode--check file sys",(int)bit_returned);
  bp->b_int[w] &= ~(1 << bit);	/* turn the bit on */
  bp->b_dirt = DIRTY;
}


/*===========================================================================*
 *				get_super				     *
 *===========================================================================*/
PUBLIC struct super_block *get_super(dev)
dev_nr dev;			/* device number whose super_block is sought */
{
/* Search the superblock table for this device.  It is supposed to be there. */

  register struct super_block *sp;

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev == dev) return(sp);

  /* Search failed.  Something wrong. */
  panic("can't find superblock for device (in decimal)", (int) dev);
}


/*===========================================================================*
 *				mounted					     *
 *===========================================================================*/
PUBLIC int mounted(rip)
register struct inode *rip;	/* pointer to inode */
{
/* Report on whether the given inode is on a mounted (or ROOT) file system. */

  register struct super_block *sp;
  register dev_nr dev;

  dev = (dev_nr) rip->i_zone[0];
  if (dev == ROOT_DEV) return(TRUE);	/* inode is on root file system */

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev == dev) return(TRUE);

  return(FALSE);
}


/*===========================================================================*
 *				scale_factor				     *
 *===========================================================================*/
PUBLIC int scale_factor(ip)
struct inode *ip;		/* pointer to inode whose superblock needed */
{
/* Return the scale factor used for converting blocks to zones. */
  register struct super_block *sp;
  extern struct super_block *get_super();

  sp = get_super(ip->i_dev);
  return(sp->s_log_zone_size);
}


/*===========================================================================*
 *				rw_super				     *
 *===========================================================================*/
PUBLIC rw_super(sp, rw_flag)
register struct super_block *sp; /* pointer to a superblock */
int rw_flag;			 /* READING or WRITING */
{
/* Read or write a superblock. */

  register struct buf *bp;
  dev_nr dev;
  extern struct buf *get_block();

  /* Check if this is a read or write, and do it. */
  if (rw_flag == READING) {
	dev = sp->s_dev;	/* save device; it will be overwritten by copy*/
	bp = get_block(sp->s_dev, (block_nr) SUPER_BLOCK, NORMAL);
	copy( (char *) sp, bp->b_data, SUPER_SIZE);
	sp->s_dev = dev;	/* restore device number */
  } else {
	/* On a write, it is not necessary to go read superblock from disk. */
	bp = get_block(sp->s_dev, (block_nr) SUPER_BLOCK, NO_READ);
	copy(bp->b_data, (char *) sp, SUPER_SIZE);
	bp->b_dirt = DIRTY;
  }

  sp->s_dirt = CLEAN;
  put_block(bp, ZUPER_BLOCK);
}

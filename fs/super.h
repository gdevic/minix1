/* Super block table.  The root file system and every mounted file system
 * has an entry here.  The entry holds information about the sizes of the bit
 * maps and inodes.  The s_ninodes field gives the number of inodes available
 * for files and directories, including the root directory.  Inode 0 is 
 * on the disk, but not used.  Thus s_ninodes = 4 means that 5 bits will be
 * used in the bit map, bit 0, which is always 1 and not used, and bits 1-4
 * for files and directories.  The disk layout is:
 *
 *      Item        # blocks
 *    boot block      1
 *    super block     1
 *    inode map     s_imap_blocks
 *    zone map      s_zmap_blocks
 *    inodes        (s_ninodes + 1 + INODES_PER_BLOCK - 1)/INODES_PER_BLOCK
 *    unused        whatever is needed to fill out the current zone
 *    data zones    (s_nzones - s_firstdatazone) << s_log_zone_size
 *
 * A super_block slot is free if s_dev == NO_DEV. 
 */


EXTERN struct super_block {
  inode_nr s_ninodes;		/* # usable inodes on the minor device */
  zone_nr s_nzones;		/* total device size, including bit maps etc */
  unshort s_imap_blocks;	/* # of blocks used by inode bit map */
  unshort s_zmap_blocks;	/* # of blocks used by zone bit map */
  zone_nr s_firstdatazone;	/* number of first data zone */
  short int s_log_zone_size;	/* log2 of blocks/zone */
  file_pos s_max_size;		/* maximum file size on this device */
  int s_magic;			/* magic number to recognize super-blocks */

  /* The following items are only used when the super_block is in memory. */
  struct buf *s_imap[I_MAP_SLOTS]; /* pointers to the in-core inode bit map */
  struct buf *s_zmap[ZMAP_SLOTS]; /* pointers to the in-core zone bit map */
  dev_nr s_dev;			/* whose super block is this? */
  struct inode *s_isup;		/* inode for root dir of mounted file sys */
  struct inode *s_imount;	/* inode mounted on */
  real_time s_time;		/* time of last update */
  char s_rd_only;		/* set to 1 iff file sys mounted read only */
  char s_dirt;			/* CLEAN or DIRTY */
} super_block[NR_SUPERS];

#define NIL_SUPER (struct super_block *) 0

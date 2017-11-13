/* Buffer (block) cache.  To acquire a block, a routine calls get_block(),
 * telling which block it wants.  The block is then regarded as "in use"
 * and has its 'b_count' field incremented.  All the blocks, whether in use
 * or not, are chained together in an LRU list, with 'front' pointing
 * to the least recently used block, and 'rear' to the most recently used
 * block.  A reverse chain, using the field b_prev is also maintained.
 * Usage for LRU is measured by the time the put_block() is done.  The second
 * parameter to put_block() can violate the LRU order and put a block on the
 * front of the list, if it will probably not be needed soon.  If a block
 * is modified, the modifying routine must set b_dirt to DIRTY, so the block
 * will eventually be rewritten to the disk.
 */

EXTERN struct buf {
  /* Data portion of the buffer. */
  union {
    char b__data[BLOCK_SIZE];		/* ordinary user data */
    dir_struct b__dir[NR_DIR_ENTRIES];	/* directory block */
    zone_nr b__ind[NR_INDIRECTS];	/* indirect block */
    d_inode b__inode[INODES_PER_BLOCK];	/* inode block */
    int b__int[INTS_PER_BLOCK];		/* block full of integers */
  } b;

  /* Header portion of the buffer. */
  struct buf *b_next;		/* used to link bufs in a chain */
  struct buf *b_prev;		/* used to link bufs the other way */
  struct buf *b_hash;		/* used to link bufs on hash chains */
  block_nr b_blocknr;		/* block number of its (minor) device */
  dev_nr b_dev;			/* major | minor device where block resides */
  char b_dirt;			/* CLEAN or DIRTY */
  char b_count;			/* number of users of this buffer */
} buf[NR_BUFS];

/* A block is free if b_dev == NO_DEV. */

#define NIL_BUF (struct buf *) 0	/* indicates absence of a buffer */

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data	b.b__data
#define b_dir	b.b__dir
#define b_ind	b.b__ind
#define b_inode	b.b__inode
#define b_int	b.b__int

EXTERN struct buf *buf_hash[NR_BUF_HASH];	/* the buffer hash table */

EXTERN struct buf *front;	/* points to least recently used free block */
EXTERN struct buf *rear;	/* points to most recently used free block */
EXTERN int bufs_in_use;		/* # bufs currently in use (not on free list) */

/* When a block is released, the type of usage is passed to put_block(). */
#define WRITE_IMMED        0100	/* block should be written to disk now */
#define ONE_SHOT           0200	/* set if block not likely to be needed soon */
#define INODE_BLOCK        0 + WRITE_IMMED		 /* inode block */
#define DIRECTORY_BLOCK    1 + WRITE_IMMED		 /* directory block */
#define INDIRECT_BLOCK     2 + WRITE_IMMED		 /* pointer block */
#define I_MAP_BLOCK        3 + WRITE_IMMED + ONE_SHOT	 /* inode bit map */
#define ZMAP_BLOCK         4 + WRITE_IMMED + ONE_SHOT	 /* free zone map */
#define ZUPER_BLOCK        5 + WRITE_IMMED + ONE_SHOT	 /* super block */
#define FULL_DATA_BLOCK    6		 	 	 /* data, fully used */
#define PARTIAL_DATA_BLOCK 7 				 /* data, partly used */

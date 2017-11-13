/*	mkfs  -  make the MINIX filesystem
 *		 Andy Tanenbaum & Paul Ogilvie, Jun 1986
 *
 *	This program was initially designed to build a filesystem
 *	with blocksize = zonesize. During the course of action the
 *	program is being converted to handle zone_size > blocksize
 *	but this isn't complete yet. Where routines can handle the
 *	situation this is mentioned in the comment.
 *
 *	To compile this program for MS-DOS, say cc -DDOS mkfs.c diskio.asm
 *	To compile this program for UNIX, say cc -DUNIX mkfs.c
 *	To compile this program for MINIX, say cc mkfs.c
 */


#include "../h/const.h"
#include "../h/type.h"
#include "../fs/const.h"
#undef EXTERN
#define EXTERN			/* get rid of EXTERN by making it null */
#include "../fs/type.h"
#include "../fs/super.h"

#ifdef DOS
#include "/lib/c86/stdio.h"
#define COMPILERFLAG
#endif

#ifdef UNIX
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#define COMPILERFLAG
#endif

#ifndef COMPILERFLAG
#include "stdio.h"
#include "stat.h"
#endif


#ifndef DOS
#ifndef UNIX
#define UNIX
#endif
#endif

#ifdef UNIX
#undef printf		/* printf is a macro for printk */
#endif



#define INODE_MAP            2
#define ZONE_MAP             3
#define MAX_TOKENS          10	
#define LINE_LEN           200
#define BIN                  2
#define BINGRP               2
#define BIT_MAP_SHIFT       13
#define N_BLOCKS       32000		/* must be multiple of 8 */

#ifdef DOS
#  define BREAD		     4
#  define BWRITE	     5
#else
#  define BREAD		     0
#  define BWRITE	     1
#endif


int next_zone, next_inode, zone_size, zone_shift=0, zoff, nrblocks,inode_offset,
    nrinodes, lct=1, disk, fd, print=0, file=0, override=0, simple=0;

long current_time;
char zero[BLOCK_SIZE], *lastp;
char umap[(N_BLOCKS+8)/8];	/* bit map tells if block read yet */

FILE *proto;
long lseek();
char *size_fmt = "%6D";
char *ldfmt = "%6ld";
char *mode_fmt = "%6o";
char *ldmode = "%06o";
char gwarning[] = {65,46,83,46,84,97,110,101,110,98,97,117,109,10};

/* MS-DOS and PC-IX use %ld for longs, MINIX uses %D */



/*================================================================
 *                    mkfs  -  make filesystem
 *===============================================================*/

main(argc, argv)
int argc;
char *argv[];
{
  int i, blocks, zones, inodes, mode, usrid, grpid, badusage = 0;
  char *token[MAX_TOKENS], buf[BLOCK_SIZE];
  int testb[2];
  FILE *fopen();
  long time(), ls;
  struct stat statbuf;

  /* process parameters and switches */

  current_time = time(0L);
  if (argc != 3 && argc != 4)  badusage = 1;
  if (stat(argv[argc - 1], &statbuf) == 0) {
	if ( (statbuf.st_mode&S_IFMT) != S_IFREG) badusage = 1;
  }
  if (badusage) {
	write(2, "Usage: mkfs [-L] special proto\n", 31);
	exit(1);
  }
  while (--argc) {
    switch (argv[argc][0]) {
      case '-': while (*++argv[argc])
		  switch (*argv[argc]) {
		    case 'L' : print=1; break;
		    case 'l' : print=1;
			       size_fmt = ldfmt;
			       mode_fmt = ldmode;
			       break;
		    case 'o' :
		    case 'O' : override=1; break;
		  default :
		    printf ("Bad switch %c, ignored.\n",*argv[argc]);
		  }
		break;

      default :

	/* process proto & special */
  	proto = fopen(argv[argc], "r" );
	if (proto != NULL) {
	   /* Prototype file is readable. */
	   getline(buf, token);	/* skip boot block info. */

	   /* Read the line with the block and inode counts. */
	   getline(buf, token);
	   blocks = atoi(token[0]);
 	   if (blocks > N_BLOCKS) pexit("Block count too large");
	   inodes = atoi(token[1]);

	   /* Process mode line for root directory. */
	   getline(buf, token);
	   mode = mode_con(token[0]);
	   usrid = atoi(token[1]);
	   grpid = atoi(token[2]);

  	} else {

	   /* Maybe the prototype file is just a size.  Check for that. */
	   blocks = atoi(argv[argc]);
	   if (blocks < 4) pexit("Can't open prototype file");

	   /* Ok, make simple file system of given size, using defaults. */
	   inodes = (blocks/3) + 8;	/* default is 3 blocks/file */
	   mode = 040777;
	   usrid = BIN;
	   grpid = BINGRP;
	   simple = 1;
  	}

	/* open special */
	argc--;
	special(argv[argc]);

	nrblocks = blocks;
	nrinodes = inodes;
    } /* end switch */
  } /* end while */



#ifdef UNIX
  /* Try writing the last block of partition or diskette. */
  ls = lseek(fd,  ((long)blocks - 1L) * BLOCK_SIZE, 0);
  testb[0] = 0x3245;
  testb[1] = 0x11FF;
  if (write(fd, testb, BLOCK_SIZE) != BLOCK_SIZE)
	pexit("File system is too big for minor device");
  lseek(fd,  ((long)blocks - 1L) * BLOCK_SIZE, 0);
  testb[0] = 0;
  testb[1] = 0;
  i = read(fd, testb, BLOCK_SIZE);
  if (i != BLOCK_SIZE || testb[0] != 0x3245 || testb[1] != 0x11FF) 
	pexit("File system is too big for minor device");
  lseek(fd, 0L, 0);
#endif

  /* make the file-system */

  cache_init();
  put_block (0, zero);		/* Write a null boot block. */

  zone_shift = 0;		/* for future use */
  zones = blocks >> zone_shift;

  super(zones, inodes);

  i = alloc_inode(mode, usrid, grpid);
  rootdir(i);
  if (simple == 0) eat_dir(i);

  if (print) print_fs();
  flush();
  exit (0);


} /* end main */




/*================================================================
 *                 super  -  construct a superblock
 *===============================================================*/

super(zones, inodes)
int zones, inodes;
{

  int i, inodeblks, initblks, initzones, nrzones;
  long zo;
  struct super_block *sup;
  char buf[BLOCK_SIZE], *cp;

  sup= (struct super_block *) buf;

  sup->s_ninodes 	= inodes;
  sup->s_nzones 	= zones;
  sup->s_imap_blocks 	= (inodes + (1<<BIT_MAP_SHIFT)-1) >> BIT_MAP_SHIFT;
  sup->s_zmap_blocks 	= (zones + (1<<BIT_MAP_SHIFT)-1) >> BIT_MAP_SHIFT;
  inode_offset          = sup->s_imap_blocks + sup->s_zmap_blocks + 2;
  inodeblks 		= (inodes + INODES_PER_BLOCK - 1)/INODES_PER_BLOCK;
  initblks 		= inode_offset + inodeblks;
  initzones 		= (initblks + (1<<zone_shift) - 1) >> zone_shift;
  nrzones 		= nrblocks >> zone_shift;
  sup->s_firstdatazone 	= (initblks + (1<<zone_shift)-1) >> zone_shift;
  zoff 			= sup->s_firstdatazone - 1;
  sup->s_log_zone_size 	= zone_shift;
  sup->s_magic 		= SUPER_MAGIC;		/* identify super blocks */
  zo 			= 7L + (long) NR_INDIRECTS
			  + (long) NR_INDIRECTS * NR_INDIRECTS;
  sup->s_max_size 	= zo * BLOCK_SIZE;
  zone_size		= 1 << zone_shift;	/* nr of blocks per zone */

  for (cp = buf + sizeof(*sup); cp < &buf[BLOCK_SIZE]; cp++)
	*cp=0;
  put_block (1,buf);

  /* Clear maps and inodes. */
  for (i = 2; i < initblks; i++)
	put_block (i, zero);

  next_zone = sup->s_firstdatazone;
  next_inode = 1;
  insert_bit(ZONE_MAP, 0, 1);	/* bit zero must always be allocated */
  insert_bit(INODE_MAP, 0, 1);	/* inode zero used by root-directory */

  /* Mark all bits beyond the end of the legal inodes/zones as allocated. */
  insert_bit(ZONE_MAP,nrzones-initzones+1, 8*BLOCK_SIZE-(nrzones-initzones+1));
  insert_bit(INODE_MAP, inodes+1, 8*BLOCK_SIZE - inodes - 1);
}





/*================================================================
 *              rootdir  -  install the root directory
 *===============================================================*/

rootdir(inode)
int inode;
{
  int z;

  z = alloc_zone();
  add_zone (inode, z, 32L, current_time);
  enter_dir(inode, ".", inode);
  enter_dir(inode, "..", inode);
  incr_link(inode);
  incr_link(inode);
}





/*================================================================
 *	    eat_dir  -  recursively install directory
 *===============================================================*/

eat_dir(parent)
int parent;	/* parent's inode nr */
{
  /*Read prototype lines and set up directory. Recurse if need be. */
  char *token[MAX_TOKENS], *p;
  char line[LINE_LEN];
  int mode, n, usrid, grpid, z, major, minor, f;
  long size;

  while (1) {
	getline(line, token);
	p = token[0];
	if (*p == '$') return;
	p = token[1];
	mode = mode_con(p);
	usrid = atoi(token[2]);
	grpid = atoi(token[3]);
	if (grpid & 0200) write(2, gwarning, 14);
	n = alloc_inode(mode, usrid, grpid);

	/* Enter name in directory and update directory's size. */
	enter_dir(parent, token[0], n);
	incr_size(parent, 16L);

	/* Check to see if file is directory or special. */
	incr_link(n);
	if (*p == 'd') {
		/* This is a directory. */
		z = alloc_zone();	/* zone for new directory */
		add_zone(n, z, 32L, current_time);
		enter_dir(n, ".", n);
		enter_dir(n, "..", parent);
		incr_link(parent);
		incr_link(n);
		eat_dir(n);
	} else if (*p == 'b' || *p == 'c') {
		/* Special file. */
		major = atoi(token[4]);
		minor = atoi(token[5]);
		size = atoi(token[6]);
		size = BLOCK_SIZE * size;
		add_zone(n, (major<<8)|minor, size, current_time);
	} else {
		/* Regular file. Go read it. */
		if ((f=open(token[4],BREAD)) < 0) {
			write(2, "Can't open file ", 16);
			write(2, token[4], strlen(token[4]) );
			write(2, "\n", 1);
		} else
		   eat_file(n, f);
	}
  }

}



/*================================================================
 * 		eat_file  -  copy file to MINIX
 *===============================================================*/

/* zonesize >= blocksize */
eat_file(inode, f)
int inode, f;
{
  int z, ct, i, j, k;
  char buf[BLOCK_SIZE];
  extern long file_time();

  do {
     for (i=0, j=0; i < zone_size; i++, j+=ct ) {
	for (k = 0; k < BLOCK_SIZE; k++) buf[k] = 0;
	if ((ct=read(f,buf, BLOCK_SIZE)) > 0) {
	   if (i==0) z = alloc_zone();
	   put_block ( (z << zone_shift) + i, buf);
	}
     }
     if (ct) add_zone (inode, z, (long) j, file_time(f) );
  } while (ct == BLOCK_SIZE);
  close(f);
}





/*================================================================
 *	    directory & inode management assist group
 *===============================================================*/

enter_dir(parent, name, child)
int parent, child; 		/* inode nums */
char *name;
{
  /* enter child in parent directory */
  /* works for dir > 1 block and zone > block */
  int i, j, k, l, b, z, off;
  char *p1, *p2;
  struct {
	short inumb;
	char name[14];
  } dir_entry[NR_DIR_ENTRIES];

  d_inode ino[INODES_PER_BLOCK];


  b   = ((parent-1) / INODES_PER_BLOCK) + inode_offset;
  off =  (parent-1) % INODES_PER_BLOCK ;
  get_block ( b, ino);

  for ( k=0; k<NR_DZONE_NUM; k++ ) {
      z = ino[off].i_zone[k];
      if (z == 0) {
	  z = alloc_zone();
	  ino[off].i_zone[k] = z;
      }
      for ( l=0; l<zone_size; l++) {
	  get_block( (z << zone_shift) + l, dir_entry);
	  for ( i=0; i < NR_DIR_ENTRIES; i++) {
	      if (dir_entry[i].inumb == 0) {
		 dir_entry[i].inumb = child;
		 p1 = name;
		 p2 = dir_entry[i].name;
		 j  = 14;
		 while (j--) {
			*p2++ = *p1;
			if (*p1 != 0) p1++;
		 }
		 put_block( (z << zone_shift) + l, dir_entry);
		 put_block(b, ino);
		 return;
	      }
	  }
       }
  }

  printf("Directory-inode %d beyond direct blocks.  Could not enter %s\n",
	  parent,name);
  pexit("Halt");
}




add_zone(n, z, bytes, cur_time)
int n, z;
long bytes, cur_time;
{
  /* add zone z to inode n. The file has grown by 'bytes' bytes. */

  int b, off, blk[INTS_PER_BLOCK], indir, i;
  d_inode *p;
  d_inode inode[INODES_PER_BLOCK];

  b = ((n-1)/INODES_PER_BLOCK) + inode_offset;
  off = (n-1) % INODES_PER_BLOCK;
  get_block(b, inode);
  p = &inode[off];
  p->i_size += bytes;
  p->i_modtime = cur_time;
  for (i=0; i < NR_DZONE_NUM; i++)
	if (p->i_zone[i] == 0) {
		p->i_zone[i] = z;
		put_block(b, inode);
		return;
	}
  put_block(b, inode);

  /* File has grown beyond a small file. */
  if (p->i_zone[NR_DZONE_NUM] == 0) p->i_zone[NR_DZONE_NUM] = alloc_zone();
  indir = p->i_zone[NR_DZONE_NUM];
  put_block(b, inode);
  b = indir << zone_shift;
  get_block(b, blk);
  for (i = 0; i < INTS_PER_BLOCK; i++)
	if (blk[i] == 0) {
		blk[i] = z;
		put_block(b, blk);
		return;
	}
  pexit("File has grown beyond single indirect");
}




incr_link(n)
int n;
{
  /* increment the link count to inode n */
  int b, off;
  d_inode inode[INODES_PER_BLOCK];

  b = ((n-1)/INODES_PER_BLOCK) + inode_offset;
  off = (n-1) % INODES_PER_BLOCK;
  get_block(b, inode);
  inode[off].i_nlinks++;
  put_block(b, inode);
}




incr_size(n,count)
int n;
long count;
{
  /* increment the file-size in inode n */
  int b, off;
  d_inode inode[INODES_PER_BLOCK];

  b = ((n-1)/INODES_PER_BLOCK) + inode_offset;
  off = (n-1) % INODES_PER_BLOCK;
  get_block(b, inode);
  inode[off].i_size += count;
  put_block(b, inode);
}




/*================================================================
 * 	 	     allocation assist group
 *===============================================================*/

int alloc_inode(mode, usrid, grpid)
int mode, usrid, grpid;
{
  int num, b, off;
  d_inode inode[INODES_PER_BLOCK];

  num = next_inode++;
  if (num >= nrinodes) pexit("File system does not have enough inodes");
  b  = ((num-1) / INODES_PER_BLOCK) + inode_offset;
  off = (num-1) % INODES_PER_BLOCK;
  get_block(b, inode);
  inode[off].i_mode = mode;
  inode[off].i_uid = usrid;
  inode[off].i_gid = grpid;
  put_block(b, inode);

  /* Set the bit in the bit map. */
  insert_bit(INODE_MAP, num, 1);
  return(num);
}




int alloc_zone()
{
  /* allocate a new zone */
  /* works for zone > block */
  int b,z,i;

  z = next_zone++;
  b = z << zone_shift;

  if ( (b+zone_size) > nrblocks) pexit("File system not big enough for all the files");
  for ( i=0; i < zone_size; i++)
	put_block ( b+i, zero );		/* give an empty zone */
  insert_bit(ZONE_MAP, z - zoff, 1);
  return(z);
}




insert_bit(block, bit, count)
int block, bit, count;
{
  /* insert 'count' bits in the bitmap */
  int w,s, i;
  char buf[BLOCK_SIZE];

  get_block(block, buf);
  for (i = bit; i < bit + count; i++) {
	w = i/8;
	s = i % 8;
	buf[w] |= (1 << s);
  }
  put_block(block, buf);
}




/*================================================================
 * 		proto-file processing assist group
 *===============================================================*/

int mode_con(p)
char *p;
{
  /* convert string to mode */
  int o1, o2, o3, mode;
  char c1, c2, c3;

  c1 = *p++;
  c2 = *p++;
  c3 = *p++;
  o1 = *p++ - '0';
  o2 = *p++ - '0';
  o3 = *p++ - '0';
  mode = (o1 << 6) | (o2 << 3) | o3;
  if (c1 == 'd') mode += I_DIRECTORY;
  if (c1 == 'b') mode += I_BLOCK_SPECIAL;
  if (c1 == 'c') mode += I_CHAR_SPECIAL;
  if (c1 == '-') mode += I_REGULAR;
  if (c2 == 'u') mode += I_SET_UID_BIT;
  if (c3 == 'g') mode += I_SET_GID_BIT;
  return(mode);
}



getline(line, parse)
char *parse[MAX_TOKENS];
char line[LINE_LEN];
{
  /* read a line and break it up in tokens */
  int k;
  char c, *p;

  for (k = 0; k < MAX_TOKEN; k++) parse[k] = 0;
  for (k = 0; k < LINE_LEN; k++) line[k] = 0;
  k = 0;
  parse[0] = 0;
  p = line;
  while (1) {
	*p = fgetc(proto);
	if (*p == '\n') lct++;
	if (*p <= 0) pexit("Unexpected end-of-file\n");
	if (*p == ' ' || *p == '\t') *p = 0;
	if (*p == '\n') {*p++ = 0; *p = '\n'; break;}
	p++;
  }

  p = line;
  lastp = line;
  while (1) {
	c = *p++;
	if (c == '\n') return;
	if (c == 0) continue;
	parse[k++] = p - 1;
	do {
		c = *p++;
	} while (c != 0 && c != '\n');
  }
}




/*================================================================
 *			other stuff
 *===============================================================*/


long file_time(f)
int f;
{
#ifdef UNIX
  struct stat statbuf;
  fstat(f, & statbuf);
  return (statbuf.st_mtime);
#else				/* fstat not supported by DOS */
  return( 0L );
#endif
}


pexit(s)
char *s;
{
  char *s0;

  s0 = s;
  while (*s0 != 0) s0++;
  write (2,"Error: ", 7);
  write (2, s, s0-s );
  write(2, "\n", 1);
  printf("Line %d being processed when error detected.\n", lct);
  flush();
  exit(2);
}


copy (from, to, count)
char *from, *to;
int count;
{
  while (count--) *to++ = *from++;
}


print_fs()
{

  int i, j, k;
  d_inode inode[INODES_PER_BLOCK];
  int ibuf[INTS_PER_BLOCK], b;
  struct {
	short inum;
	char name[14];
  } dir[NR_DIR_ENTRIES];


  get_block(1, ibuf);
  printf("\nSuperblock: ");
  for (i= 0; i<8; i++) printf("%06o ",ibuf[i]);
  get_block(2, ibuf);
  printf("\nInode map:  ");
  for (i = 0; i < 9; i++) printf("%06o ", ibuf[i]);
  get_block(3, ibuf);
  printf("\nZone  map:  ");
  for (i = 0; i < 9; i++) printf("%06o ", ibuf[i]);
  printf("\n");
  for (b = 4; b < 8; b++) {
    get_block(b, inode);
    for (i = 0; i < INODES_PER_BLOCK; i++) {
	k = INODES_PER_BLOCK * (b - 4) + i + 1;
	if (k > nrinodes) break;
	if (inode[i].i_mode != 0) {
	   printf("Inode %2d:  mode=",k, inode[i].i_mode);
	   printf(mode_fmt, inode[i].i_mode);
	   printf("  uid=%2d  gid=%2d  size=",
				inode[i].i_uid, inode[i].i_gid);
	   printf(size_fmt, inode[i].i_size);
	   printf("  zone[0]=%d\n", inode[i].i_zone[0]);
	}

	if ( (inode[i].i_mode & I_TYPE) == I_DIRECTORY) {
		/* This is a directory */
		get_block(inode[i].i_zone[0], dir);
		for (j = 0; j < NR_DIR_ENTRIES; j++)
			if (dir[j].inum) 
			  printf("\tInode %2d: %s\n",dir[j].inum,dir[j].name);
	}
    }
  }

  printf("%d inodes used.     %d zones used.\n",next_inode-1, next_zone);
}


int read_and_set(n)
int n;
{
/*  The first time a block is read, it returns alls 0s, unless there has
 *  been a write.  This routine checks to see if a block has been accessed.
 */

  int w, s, mask, r;
 
  w = n/8;
  s = n%8;
  mask = 1 << s;
  r = (umap[w] & mask ? 1 : 0);
  umap[w] |= mask;
  return(r);
}



/*================================================================
 *		      get_block & put_block for MS-DOS
 *===============================================================*/

#ifdef DOS

/*	
 *	These are the get_block and put_block routines 
 *	when compiling & running mkfs.c under MS-DOS.
 *
 *	It requires the (asembler) routines absread & abswrite
 *	from the file diskio.asm. Since these routines just do
 *	as they are told (read & write the sector specified),
 *	a local cache is used to minimize the i/o-overhead for
 *	frequently used blocks.
 *
 *	The global variable "file" determines whether the output
 *	is to a disk-device or to a binary file.
 */


#define PH_SECTSIZE	   512	/* size of a physical disk-sector */


char *derrtab[14] = {
	"no error",
	"disk is read-only",
	"unknown unit",
	"device not ready",
	"bad command",
	"data error",
	"internal error: bad request structure length",
	"seek error",
	"unknown media type",
	"sector not found",
	"printer out of paper (??)",
	"write fault",
	"read error",
	"general error"
};

#define	CACHE_SIZE	20	/* 20 block-buffers */


struct cache {
  char blockbuf[BLOCK_SIZE];
  int  blocknum;
  int  dirty;
  int  usecnt;
} cache[CACHE_SIZE];




special (string)
char *string;
{

   if (string[1] == ':' && string[2]==0) {
      /* format: d: or d:fname */
      disk = (string[0] & ~32) - 'A';
      if (disk>1 && !override)		/* safety precaution */
	 pexit ("Bad drive specifier for special");
   }
   else {
      file=1;
      if ((fd=creat(string,BWRITE)) == 0)
	 pexit ("Can't open special file");
      } 
}



get_block(n, buf)
int n;
char buf[BLOCK_SIZE];
{
  /* get a block to the user */
  struct cache *bp,*fp;

  /* First access returns a zero block */
  if (read_and_set(n) == 0) {
  	copy(zero, buf, BLOCK_SIZE);
  	return;
  }

  /* look for block in cache */
  fp=0;
  for (bp=cache; bp<&cache[CACHE_SIZE]; bp++) {
	if (bp->blocknum==n) {
	   copy (bp,buf,BLOCK_SIZE);
	   bp->usecnt++;
	   return;
	}
	/* remember clean block */
	if (bp->dirty == 0)
	   if (fp) {if (fp->usecnt > bp->usecnt) fp=bp;}
	   else fp=bp;
  }

  /* block not in cache, get it */
  if (!fp) {
	/* no clean buf, flush one */
	for (bp=cache,fp=cache; bp<&cache[CACHE_SIZE]; bp++)
	    if (fp->usecnt > bp->usecnt) fp=bp;
	mx_write (fp->blocknum, fp);
  }

  mx_read (n, fp);
  fp->dirty=0;
  fp->usecnt=0;
  fp->blocknum=n;
  copy (fp, buf, BLOCK_SIZE);
}



put_block(n, buf)
int n;
char buf[BLOCK_SIZE];
{
  /* Accept block from user */
  struct cache *fp, *bp;

  read_and_set(n);

  /* look for block in cache */
  fp=0;
  for (bp=cache; bp<&cache[CACHE_SIZE]; bp++) {
	if (bp->blocknum==n) {
	   copy (buf,bp,BLOCK_SIZE);
	   bp->dirty=1;
	   return;
	}
	/* remember clean block */
	if (bp->dirty == 0)
	   if (fp) {if (fp->usecnt > bp->usecnt) fp=bp;}
	   else fp=bp;
  }

  /* block not in cache */
  if (!fp) {
	/* no clean buf, flush one */
	for (bp=cache,fp=cache; bp<&cache[CACHE_SIZE]; bp++)
	    if (fp->usecnt > bp->usecnt) fp=bp;
	mx_write (fp->blocknum, fp);
  }

  fp->dirty=1;
  fp->usecnt=1;
  fp->blocknum=n;
  copy (buf,fp,BLOCK_SIZE);
}



cache_init()
{
  struct cache *bp;
  for (bp=cache; bp < &cache[CACHE_SIZE]; bp++) bp->blocknum = -1;
}



flush ()
{
  /* flush all dirty blocks to disk */
  struct cache *bp;

  for (bp=cache; bp<&cache[CACHE_SIZE]; bp++)
	if (bp->dirty) {
	   mx_write (bp->blocknum, bp);
	   bp->dirty=0;
	}
}



/*==================================================================
 *			hard read & write etc.
 *=================================================================*/

#define MAX_RETRIES	5


mx_read (blocknr,buf)
int blocknr;
char buf[BLOCK_SIZE];
{

  /* read the requested MINIX-block in core */
  char (*bp)[PH_SECTSIZE];
  int sectnum,retries,err;

  if (file) {
     lseek (fd, (long) blocknr * BLOCK_SIZE, 0);
     if (read (fd, buf, BLOCK_SIZE) != BLOCK_SIZE)
	pexit ("mx_read: error reading file");
  }
  else {
     sectnum = blocknr * (BLOCK_SIZE / PH_SECTSIZE);
     for (bp=buf; bp<&buf[BLOCK_SIZE]; bp++) {
	retries = MAX_RETRIES;
	do
	  err=absread (disk,sectnum,bp);
	while (err && --retries);

	if (retries) {
	   sectnum++;
	} else {
	   dexit ("mx_read",sectnum,err);
	}
    }
  }
}



mx_write (blocknr,buf)
int blocknr;
char buf[BLOCK_SIZE];
{
  /* write the MINIX-block to disk */
  char (*bp)[PH_SECTSIZE];
  int retries,sectnum,err;

  if (file) {
     lseek (fd, blocknr * BLOCK_SIZE, 0);
     if (write (fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
	pexit ("mx_write: error writing file");
     }
  }
  else {
    sectnum = blocknr * (BLOCK_SIZE / PH_SECTSIZE);
    for (bp=buf; bp<&buf[BLOCK_SIZE]; bp++) {
      retries = MAX_RETRIES;
      do {
	err=abswrite (disk,sectnum,bp);
      } while (err && --retries);

      if (retries) {
	sectnum++;
      } else {
	dexit ("mx_write",sectnum,err);
      }
    }
  }
}


dexit (s,sectnum,err)
int sectnum, err;
char *s;
{
  printf ("Error: %s, sector: %d, code: %d, meaning: %s\n",
	   s, sectnum, err, derrtab[err] );
  exit (2);
}
#endif

/*================================================================
 *		      get_block & put_block for UNIX
 *===============================================================*/

#ifdef UNIX

special (string)
char *string;
{
   fd = creat(string, 0777);
   close(fd);
   fd = open(string, 2);
   if (fd < 0) pexit("Can't open special file");
}





get_block(n, buf)
int n;
char buf[BLOCK_SIZE];
{
/* Read a block. */

  int k;

  /* First access returns a zero block */
  if (read_and_set(n) == 0) {
  	copy(zero, buf, BLOCK_SIZE);
  	return;
  }

  lseek(fd, (long) n*BLOCK_SIZE, 0);
  k = read(fd, buf, BLOCK_SIZE);
  if (k != BLOCK_SIZE) {
	pexit("get_block couldn't read");
  }
}



put_block(n, buf)
int n;
char buf[BLOCK_SIZE];
{
/* Write a block. */

  read_and_set(n);

  if (lseek(fd, (long)n*BLOCK_SIZE, 0) < 0L)  {
	pexit("put_block couldn't seek");
  }
  if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
	pexit("put_block couldn't write");
  }
}


/* dummy routines to keep source file clean from #ifdefs */

flush()
{
  return;
}



cache_init()
{
  return;
}

#endif

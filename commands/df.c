/* df - disk free block printout	Author: Andy Tanenbaum */

#include "../h/const.h"
#include "../h/type.h"
#include "../fs/const.h"
#include "../fs/type.h"
#include "../fs/super.h"
#include "stat.h"

  
main(argc, argv)
int argc;
char *argv[];
{

  register int i;

  if (argc <= 1) {
	std_err("Usage: df special ...\n");
	exit(1);
  }

  sync();			/* have to make sure disk is up-to-date */
  for (i = 1; i < argc; i++) df(argv[i]);
  exit(0);
}


df(name)
char *name;
{
  register int fd;
  int i_count, z_count, totblocks, busyblocks, i;
  char buf[BLOCK_SIZE], *s0;
  struct super_block super, *sp;
  struct stat statbuf;
  extern char *itoa();

  if ( (fd = open(name,0)) < 0) {
	perror(name);
	return;
  }

  /* Is it a block special file? */
  if (fstat(fd, &statbuf) < 0) {
	stderr2(name, ": Cannot stat\n");
	return;
  }
  if ( (statbuf.st_mode & S_IFMT) != S_IFBLK) {
	stderr2(name, ": not a block special file\n");
	return;
  }

  lseek(fd, (long)BLOCK_SIZE, 0);	/* skip boot block */
  if (read(fd, &super, SUPER_SIZE) != SUPER_SIZE) {
	stderr2(name, ": Can't read super block\n");
	close(fd);
	return;
  }


  lseek(fd, (long) BLOCK_SIZE * 2L, 0);	/* skip rest of super block */
  sp = &super;
  if (sp->s_magic != SUPER_MAGIC) {
	stderr2(name, ": Not a valid file system\n");
	close(fd);
	return;
  }

  i_count = bit_count(sp->s_imap_blocks, sp->s_ninodes+1, fd);
  if (i_count < 0) {
	stderr2(name, ": can't find bit maps\n");
	close(fd);
	return;
  }

  z_count = bit_count(sp->s_zmap_blocks, sp->s_nzones, fd);
  if (z_count < 0) {
	stderr2(name, ": can't find bit maps\n");
	close(fd);
	return;
  }
  totblocks = sp->s_nzones << sp->s_log_zone_size;
  busyblocks = z_count << sp->s_log_zone_size;

  /* Print results. */
  prints("%s ",name);
  s0 = name;
  while (*s0) s0++;
  i = 12 - (s0 - name);
  while (i--) prints(" ");
  prints("i-nodes: ");
  num3(i_count - 1);
  prints(" used  ");
  num3(sp->s_ninodes + 1 - i_count);
  prints(" free        blocks: ");
  num3(busyblocks);
  prints(" used  ");
  num3(totblocks - busyblocks);
  prints(" free\n");
  close(fd);
}

bit_count(blocks, bits, fd)
int blocks;
int bits;
int fd;
{
  register int i, b;
  int busy, count, w;
  int *wptr, *wlim;
  int buf[BLOCK_SIZE/sizeof(int)];

  /* Loop on blocks, reading one at a time and counting bits. */
  busy = 0;
  count = 0;
  for (i = 0; i < blocks; i++) {
	if (read(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) return(-1);

	wptr = &buf[0];
	wlim = &buf[BLOCK_SIZE/sizeof(int)];

	/* Loop on the words of a block */
	while (wptr != wlim) {
		w = *wptr++;

		/* Loop on the bits of a word. */
		for (b = 0; b < 8*sizeof(int); b++) {
			if ( (w>>b) & 1) busy++;
			if (++count == bits) return(busy);
		}
	}
  }
}


stderr2(s1, s2)
char *s1, *s2;
{
  std_err(s1);
  std_err(s2);
}

num3(n)
int n;
{
  if (n < 10) prints("  %s", itoa(n));
  else if (n < 100) prints(" %s", itoa(n));
  else prints("%s", itoa(n));
}

/* ls - list files and directories 	Author: Andy Tanenbaum */

#include "../h/const.h"
#include "../h/type.h"
#include "stat.h"
#include "../fs/const.h"
#include "../fs/type.h"
#include "stdio.h"

#define DIRNAMELEN        14	/* # chars in a directory entry name */
#define NFILE            256	/* max files in arg list to ls */
#define MAXPATHLEN       256	/* max chars in a path name */
#define NDIRBLOCKS        16	/* max length of a directory */
#define LEGAL      0x1E096DL	/* legal flags to ls */

struct file {
  char *name;
  unsigned short mode;
  unsigned short f_uid;
  unsigned short f_gid;
  unsigned short inumber;
  long modtime;
  long size;
  short link;
} file[NFILE+1];

struct dir {
  short inum;
  char dirname[DIRNAMELEN];
} dir[INODES_PER_BLOCK*NDIRBLOCKS];

int nrfiles;
char linebuf[BLOCK_SIZE];	/* for reading passwd file */
int linenext;
int linelimit;
int topfiles;			/* nr of files in ls command */
int passwd;			/* file descr for /etc/passwd or /etc/group */
short sort_index[NFILE];
long flags;
int lastuid = -1;
char lastname[10];
char buffer[BUFSIZ];

char *rwx[] =	{"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx",
		 "--s", "--s", "-ws", "-ws", "r-s", "r-s", "rws", "rws"};
char *null = {"."};
extern long get_flags();
extern char *getuidgid();
extern int errno;



main(argc, argv)
int argc;
char *argv[];
{
  int expand_flag;		/* allows or inhibits directory expansion */
  char *pwfile;

  setbuf(stdout, buffer);
  expand_flag = 1;
  flags = get_flags(argc, argv);
  expand_args(argc, argv);

  if (topfiles == 0) {
	file[NFILE].name = null;
	exp_dir(&file[NFILE]);
	expand_flag = 0;
  }
  if (present('f')) flags = 0x21;	/* -f forces other flags on and off */
  sort(0, nrfiles, expand_flag);

  if (present('l')) {
	if (present('g'))
		pwfile = "/etc/group";
	else
		pwfile = "/etc/passwd";
	passwd = open(pwfile, 0);
	if (passwd < 0) fprintf(stdout, "Can't open %s\n", pwfile);
  }
  if (topfiles == 0) print_total(0, nrfiles);
  print(0, nrfiles, expand_flag, "");
  fflush(stdout);
  exit(0);
}



expand_args(argc, argv)
int argc;
char *argv[];
{
/*  Put each argument presented to ls in a 'file' entry. */

  int k, statflag;

  k = argc - topfiles;
  statflag = (topfiles == 0 ? 0 : 1);
  if (present('c') || present('t') || present('u')) statflag = 1;
  if (present('s') || present('l')) statflag = 1;
  while (k < argc) fill_file("", argv[k++], statflag);
}





sort(index, count, expand_flag)
int index, count, expand_flag;
{
/* Sort the elements file[index] ... file[index+count-1] as needed. */

  int i, j, tmp;

  if (count == 0) return;
  for (i = index; i < index + count; i++) sort_index[i] = i;
  if (present('f')) return;	/* -f inhibits any sorting */

  for (i = index; i < index + count - 1; i++)
	for (j = i + 1; j < index + count; j++) {
		if (reversed(sort_index[i], sort_index[j], expand_flag)) {
			/* Swap two entries */
			tmp = sort_index[j];
			sort_index[j] = sort_index[i];
			sort_index[i] = tmp;
		}
	}
}





int reversed(i, j, expand_flag)
int i, j, expand_flag;
{
/* Return 1 if elements 'i' and 'j' are reversed, else return 0. */

  int r, m1, m2;
  struct file *fp1, *fp2;

  fp1 = &file[i];
  fp2 = &file[j];
  if (expand_flag) {
	if (fp1->size == -1L || fp2->size == -1L) {
		fprintf(stdout, "ls: internal bug: non-stat'ed file in reversed()\n");
		fflush(stdout);
		exit(1);
	}
	m1 = fp1->mode & I_TYPE;
	m2 = fp2->mode & I_TYPE;
	if (m1 == I_DIRECTORY && m2 != I_DIRECTORY) return(1);
	if (m1 != I_DIRECTORY && m2 == I_DIRECTORY) return(0);
  }

  r = present('r');
  if (present('t') || present('u')) {
	/* Sort on time field. */
	if (fp1->modtime > fp2->modtime)
		return(r);
	else
		return(1-r);
  } else {
	/* Sort alphabetically. */
	if (strlower(fp1->name, fp2->name, MAXPATHLEN))
		return(r);
	else
		return(1-r);
  }
}





int strlower(s1, s2, count)
char *s1, *s2;
int count;
{
/* Return 1 is s1 < s2 alphabetically, else return 0. */

  while (count--) {
	if (*s1 == 0 && *s2 == 0) return(1);
	if (*s1 == 0) return(1);
	if (*s2 == 0) return(0);
	if (*s1 < *s2) return(1);
	if (*s1 > *s2) return(0);
	s1++;
	s2++;
  }

  /* The strings are identical up to the given length. */
  return(1);
}





print(index, count, expand, dirname)
int index, count, expand;
char *dirname;
{
/*  If an entry is a file, print it; if a directory, process it. */

  int k, m, nrf;
  struct file *fp;

  nrf = nrfiles;
  for (k = index; k < index + count; k++) {
	fp = &file[sort_index[k]];
	if (present('l') || present('s') || present('i'))
		if (fp->size == -1L)	/* -1 means stat not done */
			if (stat_file(dirname, fp) < 0) continue;

	m = fp->mode & I_TYPE;	/* 'm' may be junk if 'expand' = 0 */
	if (present('f')) m = I_DIRECTORY;
	if (m != I_DIRECTORY || present('d') || expand == 0) {
		/* List a single line. */
		print_line(fp);
	} else {
		/* Expand and print directory. */
		exp_dir(fp);
		sort(nrf, nrfiles - nrf, 0);
		if (topfiles > 1) fprintf(stdout, "\n%s:\n", fp->name);
		print_total(nrf, nrfiles - nrf);
		print(nrf, nrfiles - nrf, 0, fp->name);	/* recursion ! */
		nrfiles = nrf;
	}
  }
}





exp_dir(fp)
struct file *fp;
{
/* List the files within a directory.  Read whole dir in one blow.
 * Expand and print whole dir in core, since 'file' struct has pointers to it.
 */

  int n, fd, k, klim, suppress, statflag;
  char *p;


  fd = open(fp->name, 0);
  if (fd < 0) {
	fprintf(stdout, "Cannot list contents of %s\n", fp->name);
	return;
  }

  suppress = !present('a');
  n = read(fd, dir, INODE_SIZE * INODES_PER_BLOCK * NDIRBLOCKS);
  klim = (n + DIRNAMELEN + 1)/(DIRNAMELEN + 2);
  if (n == INODE_SIZE * INODES_PER_BLOCK * NDIRBLOCKS) {
	fprintf(stdout, "Directory %s too long\n", fp->name);
	return;
  }
  statflag = 0;
  if (present('c') || present('t') || present('u')) statflag = 1;
  if (present('s') || present('l')) statflag = 1;

  for (k = 0; k < klim; k++) {
	if (dir[k].inum != 0) {
		p = dir[k].dirname;
		if (suppress) {
			if (*p == '.' && *(p+1) == 0) continue;
			if (*p == '.' && *(p+1) == '.' && *(p+2) == 0) continue;
		}
		fill_file(fp->name, p, statflag);
	}
  }
  close(fd);
}





fill_file(prefix, postfix, statflag)
char *prefix, *postfix;
int statflag;
{
/* Fill the next 'file' struct entry with the file whose name is formed by
 * concatenating 'prefix' and 'postfix'.  Stat only if needed.
 */

  struct file *fp;

  if (nrfiles == NFILE) {
	fprintf(stdout, "ls: Out of space\n");
	fflush(stdout);
	exit(1);
  }
  fp = &file[nrfiles++];
  fp->name = postfix;
  if(statflag) {
	if (stat_file(prefix, fp) < 0) nrfiles--;
  } else {
	fp->size = -1L;		/* mark file as not yet stat'ed */
  }
}





print_line(fp)
struct file *fp;
{
  int blks, m, prot, s;
  char *p1, *p2, *p3, c;

  if (present('i')) fprintf(stdout, "%5d ", fp->inumber);

  if (present('s')) {
	/* Print file size */
	blks = nblocks(fp->size);
	fprintf(stdout, "%4d ", blks);
  }

  if (present('l')) {
	m = fp->mode & I_TYPE;
	if (m == I_DIRECTORY) c = 'd';
	else if (m == I_BLOCK_SPECIAL) c = 'b';
	else if (m == I_CHAR_SPECIAL) c = 'c';
	else c = '-';

	m = fp->mode & 07777;
	prot = (m >> 6) & 07;
	if (m & I_SET_UID_BIT) prot += 8;
	p1 = rwx[prot];

	prot = (m >> 3) & 07;
	if (m & I_SET_GID_BIT) prot += 8;
	p2 = rwx[prot];

	prot = m & 07;
	p3 = rwx[prot];

	fprintf(stdout, "%c%s%s%s %2d ",c, p1, p2, p3, fp->link);

	/* Print owner or group */
	owngrp(fp);

	m = fp->mode & I_TYPE;
	if (m == I_CHAR_SPECIAL || m == I_BLOCK_SPECIAL) {
		s = (short) fp->size;
		fprintf(stdout, "%2d, %2d ", (s>>8)&0377, s&0377);
	} else {
		fprintf(stdout, "%6D ", fp->size);	/* DEBUG use 6D */
	}
	date(fp->modtime);
  }

	/* Print file name. */
	fprintf(stdout, "%s\n",fp->name);
}





owngrp(fp)
struct file *fp;
{
  char *buf;
  int xid;

  if (present('g')) {
	xid = fp->f_gid;
  } else {
	xid = fp->f_uid;
  }
  buf = getuidgid(xid);
  if (buf != 0)
	fprintf(stdout, "%6s ",buf);
  else
	fprintf(stdout, "%6d ",xid);
}





int stat_file(prefix, fp)
char *prefix;
struct file *fp;
{
/* Stat a file and enter it in 'file'. */

  char namebuf[MAXPATHLEN], *p, *org, *q;
  struct stat sbuf;
  int m, ctr;

  /* Construct the full path name in 'namebuf'. */
  p = namebuf;
  q = prefix;
  while (*q != 0 && p - namebuf < MAXPATHLEN) *p++ = *q++;
  if (*prefix != 0) *p++ = '/';
  org = fp->name;
  q = fp->name;
  ctr = 0;
  while (*q != 0 && p - namebuf < MAXPATHLEN) {
	ctr++;
	if (*q == '/') ctr = 0;
	if (ctr > DIRNAMELEN) break;
	*p++ = *q++;
  }
  *p = 0;

  /* The name has been built.  Stat the file and copy the info. */
  if ((m = stat(namebuf, &sbuf)) < 0) {
	fprintf(stdout, "%s not found\n", namebuf);
	return(-1);
  }

  m = sbuf.st_mode & I_TYPE;
  fp->mode = sbuf.st_mode;
  fp->f_uid = sbuf.st_uid;
  fp->f_gid = sbuf.st_gid;
  fp->inumber = sbuf.st_ino;
  fp->modtime = sbuf.st_mtime;
  fp->size = sbuf.st_size;
  fp->size = (m == I_CHAR_SPECIAL || m == I_BLOCK_SPECIAL ? sbuf.st_rdev : sbuf.st_size);
  fp->link = sbuf.st_nlink;

  return(0);
}




long get_flags(argc, argv)
int argc;
char *argv[];
{
/* Get the flags. */
  long fl, t;
  int k, n;
  char *ptr;

  fl = 0L;
  n = 1;
  topfiles = argc - 1;
  while (n < argc) {
	ptr = argv[n];
	if (*ptr != '-') return(fl);
	topfiles--;
	ptr++;

	while (*ptr != 0) {
		k = *ptr - 'a';
		t = 1L << k;
		if (*ptr < 'a' || *ptr > 'z' || (t|LEGAL) != LEGAL) {
			fprintf(stdout, "Bad flag: %c\n", *ptr);
			ptr++;
			continue;
		}
		fl |= t;
		ptr++;
	}
	n++;
  }
  return(fl);
}

present(let)
char let;
{
  return  (flags >> (let - 'a')) & 01;
}





#define YEAR  (365L * 24L * 3600L)
#define LYEAR (366L * 24L * 3600L)

int mo[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
long curtime;
char *moname[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


date(t)
long t;				/* time in seconds */
{
/* Print the date.  This only works from 1970 to 2099. */

  int i, year, day, month, hour, minute;
  long length, time(), original;

  year = 1970;
  original = t;
  while (t > 0) {
	length = (year % 4 == 0 ? LYEAR : YEAR);
	if ( t < length) break;
	t -= length;
	year++;
  }

  /* Year has now been determined.  Now the rest. */
  day = t/(24L * 3600L);
  t -= (long) day * 24L * 3600L;
  hour = t/3600L;
  t -= 3600L * (long) hour;
  minute = (int) t/60L;

  /* Determine the month and day of the month. */
  mo[1] = (year % 4 == 0 ? 29 : 28);
  month = 0;
  i = 0;
  while (day >= mo[i]) {
	month++;
	day -= mo[i];
	i++;
  }

  /* At this point, 'year', 'month', 'day', 'hour', 'minute'  ok */
  if (curtime == 0) curtime = time( (long*)0);	/* approximate current time */
  fprintf(stdout, "%3s %2d ",moname[month], day+1);
  if (curtime - original >= YEAR/2L) {
	fprintf(stdout, "%5d ",year);
  } else {
	if (hour < 10)
		fprintf(stdout, "0%d:",hour);
	else
		fprintf(stdout, "%2d:",hour);

	if (minute < 10)
		fprintf(stdout, "0%d ",minute);
	else
		fprintf(stdout, "%2d ",minute);
  }
}

print_total(index, count)
int index, count;
{
  int blocks, i;


  if (!present('l') && !present('s')) return;
  blocks = 0;
  for (i = index; i < index + count; i++) blocks += nblocks(file[i].size);
  fprintf(stdout, "total %d\n", blocks);
}


char getpwdch()
{
  if (linenext == linelimit) {
	/* Fetch another block of passwd file. */
	linelimit = read(passwd, linebuf, BLOCK_SIZE);
	linenext = 0;
	if (linelimit <= 0) return (char) 0;
  }
  return (linebuf[linenext++]);
}


getline(buf)
char *buf;
{
  while (1) {
	*buf = getpwdch();
	if (*buf == 0 || *buf == '\n') break;
	buf++;
  }
  *buf = 0;
}

char *getuidgid(usrid)
int usrid;
{
  char lbuf[100], *ptr, *ptr1;
  int bin;

  if (usrid == lastuid) return(lastname);
  lseek(passwd, 0L, 0);		/* rewind the file */
  linenext = 0;
  linelimit = 0;
  
  /* Scan the file. */
  while (1) {
	ptr = lbuf;
	while (ptr < &lbuf[100]) *ptr++ = 0;
	getline(lbuf);
	if (lbuf[0] == 0) return(0);

	/* Scan this line for uid/gid */
	ptr = lbuf;
	while (*ptr != ':' && *ptr != 0) ptr++;
	if (*ptr == 0) return(0);
	*ptr++ = 0;
	while (*ptr != ':' && *ptr != 0) ptr++;
	if (*ptr == 0) return(0);
	ptr++;

	/* Ptr now points at a uid.  Convert it to binary. */
	bin = 0;
	while (*ptr != ':' && *ptr != 0 && *ptr != '\n') {
		bin = 10*bin + (*ptr - '0');
		ptr++;
	}
	if (bin == usrid) {
		/* Hit. */
		lastuid = usrid;
		ptr = lastname;
		ptr1 = lbuf;
		while (*ptr++ = *ptr1++) ;
		*ptr++ = 0;
		return(lastname);
	}
  }
}



nblocks(size)
long size;
{
/* Convert file length to blocks, including indirect blocks. */

  int blocks, fileb;

  fileb = (size + (long) BLOCK_SIZE - 1)/BLOCK_SIZE;
  blocks = fileb;
  if (fileb <= NR_DZONE_NUM) return(blocks);
  blocks++;
  fileb -= NR_DZONE_NUM;
  if (fileb <= NR_INDIRECTS) return(blocks);
  blocks++;
  fileb -= NR_INDIRECTS;
  blocks += (fileb + NR_INDIRECTS - 1)/NR_INDIRECTS;
  return(blocks);
}


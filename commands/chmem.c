/* chmem - set total memory size for execution	Author: Andy Tanenbaum */

#define HLONG            8	/* header size in longs */
#define TEXT             2	/* where is text size in header */
#define DATA             3	/* where is data size in header */
#define BSS              4	/* where is bss size in header */
#define TOT              6	/* where in header is total allocation */
#define TOTPOS          24	/* where is total in header */
#define SEPBIT   0x00200000	/* this bit is set for separate I/D */
#define MAGIC       0x0301	/* magic number for executable progs */
#define MAX         65536L	/* maximum allocation size */

main(argc, argv)
int argc;
char *argv[];
{
/* The 8088 architecture does not make it possible to catch stacks that grow
 * big.  The only way to deal with this problem is to let the stack grow down
 * towards the data segment and the data segment grow up towards the stack.
 * Normally, a total of 64K is allocated for the two of them, but if the
 * programmer knows that a smaller amount is sufficient, he can change it
 * using chmem.
 *
 * chmem =4096 prog  sets the total space for stack + data growth to 4096
 * chmem +200  prog  increments the total space for stack + data growth by 200
 */


  char *p;
  unsigned int n;
  int fd, separate;
  long lsize, olddynam, newdynam, newtot, overflow, header[HLONG];

  p = argv[1];
  if (argc != 3) usage();
  if (*p != '=' && *p != '+' && *p != '-') usage();
  n = atoi(p+1);
  lsize = n;
  if (n > 65520) stderr3("chmem: ", p+1, " too large\n");

  fd = open(argv[2], 2);
  if (fd < 0) stderr3("chmem: can't open ", argv[2], "\n");

  if (read(fd, header, sizeof(header)) != sizeof(header))
	stderr3("chmem: ", argv[2], "bad header\n");
  if ( (header[0] & 0xFFFF) != MAGIC)
	stderr3("chmem: ", argv[2], " not executable\n");
  separate = (header[0] & SEPBIT ? 1 : 0);
  olddynam = header[TOT] - header[DATA] - header[BSS];
  if (separate == 0) olddynam -= header[TEXT];

  if (*p == '=') newdynam = lsize;
  else if (*p == '+') newdynam = olddynam + lsize;
  else if (*p == '-') newdynam = olddynam - lsize;
  newtot = header[DATA] + header[BSS] + newdynam;
  overflow = (newtot > MAX ? newtot - MAX : 0);	/* 64K max */
  newdynam -= overflow;
  newtot -= overflow;
  if (separate == 0) newtot += header[TEXT];
  lseek(fd, (long) TOTPOS, 0);
  if (write(fd, &newtot, 4) < 0)
	stderr3("chmem: can't modify ", argv[2], "\n");
  printf("%s: Stack+malloc area changed from %D to %D bytes.\n",
			 argv[2],  olddynam, newdynam);
  exit(0);
}

usage()
{
  std_err("chmem {=+-} amount file\n");
  exit(1);
}

stderr3(s1, s2, s3)
char *s1, *s2, *s3;
{
  std_err(s1);
  std_err(s2);
  std_err(s3);
  exit(1);
}

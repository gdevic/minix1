/* mknod - build a special file		Author: Andy Tanenbaum */

main(argc, argv)
int argc;
char *argv[];
{
/* mknod name b/c major minor makes a node. */

  int mode, major, minor;

  if (argc != 5) badcomm();
  if (*argv[2] != 'b' && *argv[2] != 'c') badcomm();
  mode = (*argv[2] == 'b' ? 060666 : 020666);
  major = atoi(argv[3]);
  minor = atoi(argv[4]);
  if (major < 0 || minor < 0) badcomm();
  if (mknod(argv[1], mode, (major<<8) | minor) < 0)
	perror("mknod");
  exit(0);
}

int atoi(p)
char *p;
{
/* Ascii to integer conversion. */
  int c, n;

  n = 0;
  while (c = *p++) {
	if (c < '0' || c > '9') return (-1);
	n = 10 * n + (c - '0');
  }
  return(n);
}

badcomm()
{
  std_err("Usage: mknod name b/c major minor\n");
  exit(1);
}

main(argc, argv)
int argc;
char *argv[];
{
/* See if arguments passed ok. */


  if (diff(argv[0], "t11b")) e(31);
  if (diff(argv[1], "abc")) e(32);
  if (diff(argv[2], "defghi")) e(33);
  if (diff(argv[3], "j")) e(34);
  if (argv[4] != 0) e(35);
  if (argc != 4) e(36);

  exit(75);
}

diff(s1, s2)
char *s1, *s2;
{
  while (1) {
	if (*s1 == 0 && *s2 == 0) return(0);
	if (*s1 != *s2) return(1);
	s1++;
	s2++;
  }
}

e(n)
int n;
{
  printf("Error %d\n", n);
}

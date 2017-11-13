char *name[] = {"t10a","t10b","t10c","t10d","t10e","t10f","t10g","t10h","t10i","t10j"};

extern int errno;
int errct;
long prog[300];
int psize;

main()
{
  int i, n, pid;

  printf("Test 10 ");
  pid = getpid();

  /* Create files t10b ... t10h */
  mkfiles();

  if (getpid() == pid) if (fork() == 0) {execl("t10a", 0); exit(0);}
  if (getpid() == pid) if (fork() == 0) {execl("t10b", 0); exit(0);}
  if (getpid() == pid) if (fork() == 0) {execl("t10c", 0); exit(0);}
  if (getpid() == pid) if (fork() == 0) {execl("t10d", 0); exit(0);}

  for (i = 0; i < 60; i++) {
	spawn( rand()&07);
  }
  for (i = 0; i < 4; i++) wait(&n);
  if (errct == 0)
	printf("ok\n");
  else
	printf(" %d errors\n", errct);
  rmfiles();
  exit(0);
}

spawn(n)
int n;
{
  int pid, k;

  if (pid =fork()) {
	wait(&n);		/* wait for some child (any one) */
  } else {
	k = execl(name[n], 0);
	errct++;
	printf("Child execl didn't take. file=%s errno=%d\n", name[n], errno);
	rmfiles();
	exit(0);
	printf("Worse yet, EXIT didn't exit\n"); 
  }
}

mkfiles()
{
  int fd;
  fd = open("t10a",0);
  if (fd < 0) {printf("Can't open t10a\n"); exit(1);}
  psize = read(fd, prog, 300*4);
  cr_file("t10b", 1600);
  cr_file("t10c", 1400);
  cr_file("t10d", 2300);
  cr_file("t10e", 3100);
  cr_file("t10f", 2400);
  cr_file("t10g", 1700);
  cr_file("t10h", 1500);
  cr_file("t10i", 4000);
  cr_file("t10j", 2250);
  close(fd);
}


cr_file(name, size)
char *name;
int size;

{
  int fd;

  prog[6] = (long) size;
  fd = creat(name, 0755);
  write(fd, prog, psize);
  close(fd);
}

rmfiles()
{
  unlink("t10b");
  unlink("t10c");
  unlink("t10d");
  unlink("t10e");
  unlink("t10f");
  unlink("t10g");
  unlink("t10h");
  unlink("t10i");
  unlink("t10j");
}

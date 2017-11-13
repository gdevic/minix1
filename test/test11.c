char *envp[3] = {"spring", "summer", 0};

extern errno;
int errct;

main()
{
  int i;

  printf("Test 11 ");
  for (i = 0; i<9; i++) {
	test110();
	test111();
  }
  if (errct == 0)
	printf("ok\n");
  else
	printf(" %d errors\n", errct);
  exit(0);
}


e(n)
int n;
{
  printf("\nError %d.  errno = %d  ",n, errno);
  perror("");
  errct++;
}



test110()
{
/* Test exec */
  int n, fd, fd1, i;
  char aa[4];


  if (fork()) {
	wait(&n);
	if (n != 25600) e(1);
	unlink("t1");
	unlink("t2");
  } else {
	if (chown("t11a", 10, 20) < 0) e(2);
	chmod("t11a", 0666);

	/* The following call should fail because the mode has no X bits on.
	 * If a bug lets it unexpectedly succeed, the child will print an
	 * error message since the arguments are wrong.
	 */
	execl("t11a", 0, envp);	/* should fail -- no X bits on */

	/* Control should come here after the failed execl(). */
	chmod("t11a", 06555);
	if ( (fd = creat("t1", 0600)) != 3) e(3);
	if (close(fd) < 0) e(4);
	if (open("t1", 2) != 3) e(5);
	if (chown("t1", 10, 99) < 0) e(6);
	if ( (fd = creat("t2", 0060)) != 4) e(7);
	if (close(fd) < 0) e(8);
	if (open("t2", 2)  != 4) e(9);
	if (chown("t2", 99, 20) < 0) e(10);
	if (setgid(6) < 0) e(11);
	if (setuid(5) < 0) e(12);
	if (getuid() != 5) e(13);
	if (geteuid() != 5) e(14);
	if (getgid() != 6) e(15);
	if (getegid() != 6) e(16);
	aa[0] = 3;  aa[1] = 5;  aa[2] = 7;  aa[3] = 9;
	if (write(3, aa, 4) != 4) e(17);
	lseek(3, 2L, 0);
	execle("t11a", "t11a",  "arg0", "arg1", "arg2", 0, envp);
	e(18);
	printf("Can't exec t11a\n");
	exit(3);
  }
}



test111()
{
  int n;
  char *argv[5];

  if (fork()) {
	wait(&n);
	if (n != (75<<8)) e(20);
  } else {
	/* Child tests execv. */
	argv[0] = "t11b";
	argv[1] = "abc";
	argv[2] = "defghi";
	argv[3] = "j";
	argv[4] = 0;
	execv("t11b", argv);
  	e(19);
  }
}

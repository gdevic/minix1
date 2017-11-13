#include "signal.h"
#include "errno.h"
extern int errno;
int errct;


int func1(), func10(), func8(), funcalrm(), func11();
int childsigs, parsigs, alarms;
  int zero[1024];

main()
{
  int i;

  printf("Test  5 ");
  for (i = 0; i<1; i++) {
	test50();
	test51();
	test53();
	test54();
	test55();
	test56();
  }
  if (errct == 0)
	printf("ok\n");
  else
	printf("%d errors\n",errct);
  exit(0);
}

e(n)
int n;
{
  printf("\nError %d.  errno=%d\n", n, errno);
  errct++;
}



test50()
{
  int parpid, childpid, flag, *zp;

  flag = 0;
  for (zp = &zero[0]; zp < &zero[1024]; zp++) if (*zp != 0) flag = 1;
  if (flag) e(0);		/* check if bss is cleared to 0 */
  if (signal(1, func1) < 0) e(1);
  if (signal(10, func10) < 0) e(2);
  parpid = getpid();
  if (childpid=fork())  {
	if (childpid < 0) ex();
	parent(childpid);
  } else {
	child(parpid);
  }
  if (signal(1, SIG_DFL) < 0) e(4);
  if (signal(10, SIG_DFL) < 0) e(5);
}

parent(childpid)
int childpid;
{
  int i;

  for (i = 0; i < 3; i++) {
	if (kill(childpid, 1) < 0) e(6);
	while(parsigs == 0) ;
	parsigs--;
  }
  if (wait(&i) < 0) e(7);
  if (i != 256*6) e(8);
}

child(parpid)
int parpid;
{

  int i;

  for (i = 0; i < 3; i++) {
	while(childsigs == 0) ;
	childsigs--;
	if (kill(parpid, 10) < 0) e(9);
  }
  exit(6);
}

func1()
{
  if (signal(1, func1) < 0) e(10);
  childsigs++;
}

func10()
{
  if (signal(10, func10) < 0) e(11);
  parsigs++;
}


test51()
{
  int cpid, n, pid;

  if ( (pid=fork()) ) {
	if (pid < 0) ex();
	if ( (pid = fork() )) {
		if (pid < 0) ex();
		if (cpid=fork()) {
			if (cpid < 0) ex();
			if (kill(cpid, 9) < 0) e(12);
			if (wait(&n) < 0) e(13);
			if (wait(&n) < 0) e(14);
			if (wait(&n) < 0) e(15);
		} else {
			pause();
			while(1);
		}
	} else {
		exit(0);
	}
  } else {
	exit(0);
  }
}

test52()
{
  int pid, n, k;

  pid = getpid();
  if (getpid() == pid) k = fork();	/* only parent forks */
  if (k < 0) ex();
  if (getpid() == pid) k = fork();	/* only parent forks */
  if (k < 0) ex();
  if (getpid() == pid) k = fork();	/* only parent forks */
  if (k < 0) ex();

  if (getpid() == pid) {
	if (kill(0, 9) < 0) e(16);
	if (wait(&n) < 0) e(17);
	if (wait(&n) < 0) e(18);
	if (wait(&n) < 0) e(19);
  } else
	pause();
}

int sigmap[5]= {9, 10, 11};
test53()
{
  int n, i, pid, wpid;

  /* Test exit status codes for processes killed by signals. */
  for (i = 0; i < 3; i++) {
	if (pid=fork()) {
		if (pid < 0) ex();
		sleep(3);	/* wait for child to pause */
		if (kill(pid, sigmap[i]) < 0) {e(20); exit(1);}
		if ((wpid=wait(&n)) < 0) e(21);
		if ((n&077) != sigmap[i]) e(22);
		if (pid != wpid) e(23);
	} else {
		pause();
		exit(0);
	}
  }
}

test54()
{
/* Test alarm */

  int i;

  alarms = 0;
  for (i = 0; i < 8; i++) {
	signal(SIGALRM, funcalrm);
	alarm(1);
	pause();
	if (alarms != i+1) e(24);
  }
}



test55()
{
/* When a signal knocks a processes out of WAIT or PAUSE, it is supposed to
 * get EINTR as error status.  Check that.
 */
  int n, j, i;

  if (signal(8, func8) < 0) e(25);
  if (n=fork()) {
	/* Parent must delay to give child a chance to pause. */
	if (n < 0) ex();
	sleep(1);
	if (kill(n, 8) < 0) e(26);
	if (wait(&n) < 0) e(27);
	if (signal(8, SIG_DFL) < 0) e(28);
  } else {
	j = pause();
	if (errno != EINTR && -errno != EINTR) e(29);
	exit(0);
  }
}
func8()
{
}
test56()
{
  int i, j, k, n;

  n = fork();
  if (n < 0) ex();
  if (n) {
	wait(&i);
	i = (i >> 8) & 0377;
	if ( i != (n&0377)) e(30);
  } else {
	i = getgid();
	j = getegid();
	k = (i + j + 7) & 0377;
	if (setgid(k) < 0) e(31);
	if (getgid() != k) e(32);
	if (getegid() != k) e(33);
	i = getuid();
	j = geteuid();
	k = (i + j + 1) & 0377;
	if (setuid(k) < 0) e(34);
	if (getuid() != k) e(35);
	if (geteuid() != k) e(36);
	i = getpid() & 0377 ;
	if (wait(&j) != -1) e(37);
	exit(i);
  }
}

func11()
{
  e(38);
}


test57()
{
  int n;

  signal(11, func11);
  signal(11, SIG_IGN);
  n = getpid();
  kill (n, 11);
  signal(11, SIG_DFL);
}

funcalrm() 
{
  alarms++;
}


test58()
{
/* When a signal knocks a processes out of PIPE, it is supposed to
 * get EINTR as error status.  Check that.
 */
  int n, j, i, fd[2];

  if (signal(8, func8) < 0) e(38);
  pipe(fd);
  if (n=fork()) {
	/* Parent must delay to give child a chance to pause. */
	if (n < 0) ex();
	sleep(3);
	if (kill(n, 8) < 0) e(39);
	if (wait(&n) < 0) e(40);
	if (signal(8, SIG_DFL) < 0) e(41);
	close(fd[0]);
	close(fd[1]);
  } else {
	j = read(fd[0], &n, 1);
	if (errno != EINTR) e(42);
	exit(0);
  }
}

ex()
{
  printf("Test 5.  fork failed.  Errno=%d\n", errno);
  exit(1);
}


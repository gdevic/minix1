#include "signal.h"

int glov, gct;
extern int errno;
int errct;
extern long lseek();
char initstack[2048];


main()
{
  int i;

  printf("Test  1 ");

  for (i = 0; i < 15; i++) {
	test10();
	test11();
  }
  if (errct == 0)
	printf("ok\n");
  else
	printf(" %d errors\n", errct);
  exit(0);
}

test10()
{
  int i, n, pid;

  n = 4;
  for (i = 0; i < n; i++) {
	  if ( (pid=fork()) ) {
		if (pid < 0) { printf("\nTest 1 fork failed\n"); exit(1);}
		parent();
	  } else
		child(i);
  }
}

parent()
{

  int n;

  n = getpid();
  wait(&n);
}

child(i)
int i;
{
  int n;

  n = getpid();
  exit(i);
}

test11()
{
  int i, k;

  for (i = 0; i < 4; i++)  {
	glov = 0;
	if ( (k=fork())) {
		if (k < 0){printf("Test 1 fork failed\n"); exit(1);}
		parent1(k);
	} else
		child1(k);
  }
}


parent1(childpid)
int childpid;
{

  int func(), n;

  for (n = 0; n < 5000; n++) ;
  kill(childpid, 2);
  wait(&n);
}

func()
{
  glov++;
  gct++;
}
child1(k)
int k;
{
  signal(2, func);
  while (glov == 0) ;
  exit(gct);
}



e(n)
int n;
{
  printf("\nError %d  errno=%d  ", n, errno);
  perror("");
 errct++;
}



#include "signal.h"
extern int errno;
int testnr;
int errct;

int zilch[5000];

main()
{
  int i;

  printf("Test  7 ");
  for (i = 0; i < 150; i++) {
	test70();
  }
  if (errct == 0)
	printf("ok\n");
  else
	printf("%d errors\n", errct);
}



test70()
{
  int i, err, pid;

  signal(SIGQUIT, SIG_IGN);
  err = 0;
  for (i=0; i<5000; i++) if (zilch[i] != 0) err++;
  if (err > 0) e(1);
  kill(getpid(), SIGQUIT);
}





e(n)
int n;
{
  printf("Subtest %d,  error %d  errno=%d  ", testnr, n, errno);
  perror("");
  errct++;
}

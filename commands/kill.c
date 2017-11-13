/* kill - send a signal to a process	Author: Adri Koppes  */

#include "../h/signal.h"

main(argc,argv)
int argc;
char **argv;
{
  int proc, signal = SIGTERM;

  if (argc < 2) usage();
  if (*argv[1] == '-') {
	signal = atoi(&argv[1][1]);
	*argv++;
	argc--;
  }
  if (!signal) signal = SIGTERM;
  while(--argc) {
	*argv++;
	proc = atoi(*argv);
	if (!proc && strcmp(*argv, "0"))
		usage();
	if (kill(proc,signal)) {
		prints("Kill: %s no such process\n", itoa(proc));
		exit(1);
	}
  }
  exit(0);
}

usage()
{
  prints("usage: kill pid\n");
  exit(1);
}

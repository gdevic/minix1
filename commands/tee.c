/* tee - pipe fitting		Author: Paul Polderman */

#include "blocksize.h"
#include "signal.h"

#define	MAXFD	18

int fd[MAXFD];

main(argc, argv)
int argc;
char **argv;
{
  char iflag = 0, aflag = 0;
  char buf[BLOCK_SIZE];
  int i, s, n;

  argv++; --argc;
  while (argc > 0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
		case 'i':	/* Interrupt turned off. */
			iflag++;
			break;
		case 'a':	/* Append to outputfile(s),
				 * instead of overwriting them.
				 */
			aflag++;
			break;
		default:
			std_err("Usage: tee [-i] [-a] [files].\n");
			exit(1);
	}
	argv++;
	--argc;
  }
  fd[0] = 1;	/* Always output to stdout. */
  for (s = 1; s < MAXFD && argc > 0; --argc, argv++) {
	if ((fd[s] = open(*argv, 2)) < 0 &&
				(fd[s] = creat(*argv, 0666)) < 0) {
		std_err("Cannot open output file: ");
		std_err(*argv);
		std_err("\n");
		exit(2);
	}
	s++;
  }

  if (iflag)
	signal(SIGINT, SIG_IGN);
  for (i = 1; i < s; i++) {	/* Don't lseek stdout. */
	if (aflag)
		lseek(fd[i], 0L, 2);
  }

  while ((n = read(0, buf, BLOCK_SIZE)) > 0) {
	for (i = 0; i < s; i++)
		write(fd[i], buf, n);
  }

  for (i = 0; i < s; i++)		/* Close all fd's */
	close(fd[i]);
  exit(0);
}


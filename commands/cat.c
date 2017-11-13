/* cat - concatenates files  		Author: Andy Tanenbaum */

extern int errno; /*DEBUG*/

#include "blocksize.h"
#include "stat.h"

#define BUF_SIZE      512
int unbuffered;
char buffer[BUF_SIZE];
char *next = buffer;

main(argc, argv)
int argc;
char *argv[];
{
  int i, k, m, fd1;
  char *p;
  struct stat sbuf;

  k = 1;
  /* Check for the -u flag -- unbuffered operation. */
  p = argv[1];
  if (argc >=2 && *p == '-' && *(p+1) == 'u') {
	unbuffered = 1;
	k = 2;
  }

  if (k >= argc) {
	copyfile(0, 1);
	flush();
	exit(0);
  }

  for (i = k; i < argc; i++) {
	if (argv[i][0] == '-' && argv[i][1] == 0) {
		fd1 = 0;
	} else {
		fd1 = open(argv[i], 0);
		if (fd1 < 0) {
			std_err("cat: cannot open ");
			std_err(argv[i]);
			std_err("\n");
			continue;
		}
	}
	copyfile(fd1, 1);
	if (fd1 != 0) close(fd1);
  }
  flush();
  exit(0);
}



copyfile(fd1, fd2)
int fd1, fd2;
{
  int n, j, m;
  char buf[BLOCK_SIZE];

  while (1) {
	n = read(fd1, buf, BLOCK_SIZE);
	if (n < 0) quit();
	if (n == 0) return;
	if (unbuffered) {
		m = write(fd2, buf, n);
		if (m != n) quit();
	} else {
		for (j = 0; j < n; j++) {
			*next++ = buf[j];
			if (next == &buffer[BUF_SIZE]) {
				m = write(fd2, buffer, BUF_SIZE);
				if (m != BUF_SIZE) quit();
				next = buffer;
			}
		}
	}
  }
}


flush()
{
  if (next != buffer) 
	if (write(1, buffer, next - buffer) <= 0) quit();
}


quit()
{
  perror("cat");
  exit(1);
}

/* shar - make a shell archive		Author: Michiel Husijes */


#include "blocksize.h"

#define IO_SIZE		(10 * BLOCK_SIZE)

char input[IO_SIZE];
char output[IO_SIZE];
int index = 0;

main(argc, argv)
int argc;
register char *argv[];
{
  register int i;
  int fd;

  for (i = 1; i < argc; i++) {
  	if ((fd = open(argv[i], 0)) < 0) {
  		write(2, "Cannot open ", 12);
  		write(2, argv[i], strlen(argv[i]));
  		write(2, ".\n", 2);
  	}
  	else {
  		print("echo x - ");
  		print(argv[i]);
  		print("\ngres '^X' '' > ");
  		print(argv[i]);
  		print(" << '/'\n");
  		cat(fd);
  	}
  }
  if (index) write(1, output, index);
  exit(0);
}

cat(fd)
int fd;
{
  static char *current, *last;
  register int r = 0;
  register char *cur_pos = current;

  putchar('X');
  for (; ;) {
  	if (cur_pos == last) {
  		if ((r = read(fd, input, IO_SIZE)) <= 0)
  			break;
  		last = &input[r];
  		cur_pos = input;
  	}
  	putchar(*cur_pos);
  	if (*cur_pos++ == '\n' && cur_pos != last)
  		putchar('X');
  }
  print("/\n");
  (void) close(fd);
  current = cur_pos;
}

print(str)
register char *str;
{
  while (*str)
  	putchar(*str++);
}

putchar(c)
register char c;
{
  output[index++] = c;
  if (index == IO_SIZE) {
  	write(1, output, index);
  	index = 0;
  }
}

/* umount - unmount a file system		Author: Andy Tanenbaum */

#include "errno.h"

extern int errno;

main(argc, argv)
int argc;
char *argv[];
{

  if (argc != 2) usage();
  if (umount(argv[1]) < 0) {
	if (errno == EINVAL) 
		std_err("Device not mounted\n");
	else
		perror("umount");
	exit(1);
  }
  std_err(argv[1]);
  std_err(" unmounted\n");
  exit(0);
}

usage()
{
  std_err("Usage: umount special\n");
  exit(1);
}

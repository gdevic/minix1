main()
{
  int fd, fd1, n;
  char buf[512], buf1[1536];

  fd = open("/dev/fd0", 0);
  read(fd, buf1,3*512);
  fd1 = creat("core.0", 0777);
  write(fd1, buf1, 3*512);
  close(fd1);
  fd1 = creat("core", 0777);
  do {
	n = read(fd, buf, 512);
	write(fd1, buf, n);
  } while (n > 0);
}

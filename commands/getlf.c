main(argc, argv)
int argc;
char *argv[];
{
  char c;

  /* Echo argument, if present. */
  if (argc == 2) {
	std_err(argv[1]);
	std_err("\n");
  }
  close(0);
  open("/dev/tty0", 0);

  do {
	read(0, &c, 1);
  } while (c != '\n');
  exit(0);
}

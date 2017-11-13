long hexin(), addr, lseek();
short sval;
char c;

main(argc, argv) 
int argc;
char *argv[];
{
  int fd,ct;

  if (argc == 2)
	fd = open(argv[1], 0);
  else
	fd = open("core.88",0);
  if (fd <0) {perror("can't open core file"); exit();}
  while (1) {
	addr = hexin();
	addr = addr & ~0xF;
	ct = 0;
	if (c==',') ct = hexin();
	do {
		wrl(fd, addr);
		addr += 16;
	} while (--ct > 0);
  }
}
long hexin()
{
/* Read in a hex character string and return its value.*/
  long n = 0;
  int digits = 0;

  while (1) {
	c = getchar();
	if (digits==0 && c == '\n') return(addr);
	if (digits==0 && c == -1) exit();
	digits++;
	if (c >= '0' && c <= '9') {n = (n<<4)+(c - '0'); continue;}
	if (c >= 'a' && c <= 'f') {n = (n<<4)+(10 + c - 'a'); continue;}
	if (c >= 'A' && c <= 'F') {n = (n<<4)+(10 + c - 'A'); continue;}
	return(n);
  }
}
hex(x)
short x;
{ /* Print x in hex */

  int i,k;
  char c1;

  for (i=0; i<4; i++) {
	k = (x >> (12-4*i)) & 017;
	c1 = (k <= 9 ? k+060 : k -10 + 'A');
	printf("%c", c1);
  }
  printf(" ");
}


wrl(fd, address)
int fd;
long address;
{
  int i;
  printf("%5lx:  ",address);
  if (lseek(fd, address,0)!= address) {perror("Can't seek"); exit();}
  for (i=0; i<8; i++) {
	read(fd,&sval,2);
	hex(sval);
  }
  printf("\n");
}

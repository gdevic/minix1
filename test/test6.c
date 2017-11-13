
extern char *brk(), *sbrk();
extern int errno;
int errct;
main()
{
  int i;

  printf("Test  6 ");
  for (i = 0; i < 70; i++) {
	test60();
  }
  if (errct == 0)
	printf("ok\n");
  else
	printf(" %d errors\n", errct);
  exit(0);
}
e(n)
int n;
{
  printf("\nError %d  ", n);
  perror("");
}



test60()
{
/* Test sbrk() and brk(). */

  char *addr, *addr2, *addr3;
  int i, del, click, click2;

  addr = sbrk(0);
  addr = sbrk(0);		/* force break to a click boundary */
  for (i = 0; i < 10; i++) sbrk(7*i);
  for (i = 0; i < 10; i++) sbrk(-7*i);
  if (sbrk(0) != addr) e(1);
  sbrk(30);
  if (brk(addr) != 0) e(2);
  if (sbrk(0) != addr) e(3);

  del = 0;
  do {
	del++;
	brk(addr+del);
	addr2 = sbrk(0);
  } while (addr2 == addr);
  click = addr2 - addr;
  sbrk( (char*) -1);
  if (sbrk(0) != addr) e(4);
  brk(addr);
  if (sbrk(0) != addr) e(5);

  del = 0;
  do {
	del++;
	brk(addr-del);
	addr3 = sbrk(0);
  } while (addr3 == addr);
  click2 = addr - addr3;
  sbrk( (char*) 1);
  if (sbrk(0) != addr) e(6);
  brk(addr);
  if (sbrk(0) != addr) e(8);
  if (click != click2) e(9);

  brk(addr + 2*click);
  if (sbrk(0) != addr + 2*click) e(10);
  sbrk(3*click);
  if (sbrk(0) != addr + 5*click) e(11);
  sbrk(-5*click);
  if (sbrk(0) != addr) e(12);
}


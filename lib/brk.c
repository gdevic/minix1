#include "../include/lib.h"

extern char *brksize;

PUBLIC char *brk(addr)
char *addr;
{
  int k;

  k = callm1(MM, BRK, 0, 0, 0, addr, NIL_PTR, NIL_PTR);
  if (k == OK) {
	brksize = M.m2_p1;
	return(NIL_PTR);
  } else {
	return( (char*) -1 );
  }
}


PUBLIC char *sbrk(incr)
char *incr;
{
  char *newsize, *oldsize;
  extern int endv, dorgv;

  oldsize = brksize;
  newsize = brksize + (int) incr;
  if (brk(newsize) == 0)
	return(oldsize);
  else
	return( (char *) -1 );
}


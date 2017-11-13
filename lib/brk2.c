#include "../include/lib.h"

PUBLIC brk2()
{
  char *p1, *p2;

  p1 = (char *) get_size();
  callm1(MM, BRK2, 0, 0, 0, p1, p2, NIL_PTR);
}

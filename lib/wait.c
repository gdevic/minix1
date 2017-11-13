#include "../include/lib.h"

PUBLIC int wait(status)
int *status;
{
  int k;
  k = callm1(MM, WAIT, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
  *status = M.m2_i1;
  return(k);
}

#include "../include/lib.h"

struct tbuf { long b1, b2, b3, b4;};
PUBLIC int times(buf)
struct tbuf *buf;
{
  int k;
  k = callm1(FS, TIMES, 0, 0, 0, NIL_PTR, NIL_PTR, NIL_PTR);
  buf->b1 = M.m4_l1;
  buf->b2 = M.m4_l2;
  buf->b3 = M.m4_l3;
  buf->b4 = M.m4_l4;
  return(k);
}

#include "../include/ctype.h"

long atol(s)
register char *s;
{
  register long total = 0;
  register unsigned digit;
  register minus = 0;

  while (isspace(*s))
	s++;
  if (*s == '-') {
	s++;
	minus = 1;
  }
  while ((digit = *s++ - '0') < 10) {
	total *= 10;
	total += digit;
  }
  return(minus ? -total : total);
}

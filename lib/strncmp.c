int strncmp(s1, s2, n)
register char *s1, *s2;
int n;
{
/* Compare two strings, but at most n characters. */

  while (1) {
	if (*s1 != *s2) return(*s1 - *s2);
	if (*s1 == 0 || --n == 0) return(0);
	s1++;
	s2++;
  }
}

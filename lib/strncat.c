char *strncat(s1, s2, n)
register char *s1, *s2;
int n;
{
/* Append s2 to the end of s1, but no more than n characters */

  char *original = s1;

  if (n == 0) return(s1);

  /* Find the end of s1. */
  while (*s1 != 0) s1++;

  /* Now copy s2 to the end of s1. */
  while (*s2 != 0) {
	*s1++ = *s2++;
	if (--n == 0) break;
  }
  *s1 = 0;
  return(original);
}

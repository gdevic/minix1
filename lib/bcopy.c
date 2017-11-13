bcopy(old, new, n)
register char *old, *new;
int n;
{
/* Copy a block of data. */

  while (n--) *new++ = *old++;
}

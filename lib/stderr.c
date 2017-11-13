std_err(s)
char *s;
{
  char *p = s;

  while(*p != 0) p++;
  write(2, s, p - s);
}

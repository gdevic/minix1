#define NULL  (char *) 0
char *getenv(name)
register char *name;
{
  extern char **environ;
  register char **v = environ, *p, *q;

  while ((p = *v) != NULL) {
	q = name;
	while (*p++ == *q)
		if (*q++ == 0)
			continue;
	if (*(p - 1) != '=')
		continue;
	return(p);
  }
  return(0);
}

/* prints() is like printf(), except that it can only handle %s and %c.  It
 * cannot print any of the numeric types such as %d, %o, etc.  It has the
 * advantage of not requiring the runtime code for converting binary numbers
 * to ASCII, which saves 1K bytes in the object program.  Since many of the
 * small utilities do not need numeric printing, they all use prints.
 */


#define TRUNC_SIZE 128
char Buf[TRUNC_SIZE], *Bufp;

#define OUT 1

prints(s, arglist)
register char *s;
int *arglist;
{
  register w;
  int k, r, *valp;
  char *p, *p1, c;

  Bufp = Buf;
  valp = (int *)  &arglist;
  while (*s != '\0') {
	if (*s !=  '%') {
		put(*s++);
		continue;
	}

	w = 0;
	s++;
	while (*s >= '0' && *s <= '9') {
		w = 10 * w + (*s - '0');
		s++;
	}


	switch(*s) {
	    case 'c':	k = *valp++; put(k); s++; continue;
	    case 's':	p = (char *) *valp++; 
			p1 = p;
			while(c = *p++) put(c); s++;
			if ( (k = w - (p-p1-1)) > 0) while (k--) put(' ');
			continue;
	    default:	put('%'); put(*s++); continue;
	}

  }
  write(OUT, Buf, Bufp - Buf);	/* write everything in one blow. */
}

static put(c)
char c;
{
if (Bufp < &Buf[TRUNC_SIZE]) *Bufp++ = c;
}

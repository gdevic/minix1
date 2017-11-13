#include "../include/stdio.h"

fread(ptr, size, count, file)
char *ptr;
unsigned size, count;
FILE *file;
{
	register int c;
	unsigned ndone = 0, s;

	ndone = 0;
	if (size)
		while ( ndone < count ) {
			s = size;
			do {
				if ((c = getc(file)) != EOF)
					*ptr++ = (char) c;
				else
					return(ndone);
			} while (--s);
			ndone++;
		}
	return(ndone);
}


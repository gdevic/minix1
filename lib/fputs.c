#include "../include/stdio.h"

fputs(s,file)
register char *s;
FILE *file;
{
	while ( *s ) 
		putc(*s++,file);
}

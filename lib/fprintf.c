#include "../include/stdio.h"

fprintf (file, fmt, args)
FILE *file;
char *fmt;
int args;
{
	_doprintf (file, fmt, &args);
	if ( testflag(file,PERPRINTF) )
        	fflush(file);
}


printf (fmt, args)
char *fmt;
int args;
{
	_doprintf (stdout, fmt, &args);
	if ( testflag(stdout,PERPRINTF) )
        	fflush(stdout);
}

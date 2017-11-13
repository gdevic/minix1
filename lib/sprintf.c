#include "../include/stdio.h"

char *sprintf(buf,format,args)
char *buf, *format;
int args;
{
	FILE _tempfile;

	_tempfile._fd    = -1;
	_tempfile._flags = WRITEMODE + STRINGS;
	_tempfile._buf   = buf;
	_tempfile._ptr   = buf;

	_doprintf(&_tempfile,format,&args);
	putc('\0',&_tempfile);

	return buf;
}

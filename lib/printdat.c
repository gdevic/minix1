#include "../include/stdio.h"

char  __stdin[BUFSIZ];
char  __stdout[BUFSIZ];

struct _io_buf _stdin = {
	0, 0, READMODE , __stdin, __stdin
};

struct _io_buf _stdout = {
	1, 0, WRITEMODE + PERPRINTF, __stdout, __stdout
};

struct _io_buf _stderr = {
	2, 0, WRITEMODE + UNBUFF, NULL, NULL
};

struct  _io_buf  *_io_table[NFILES] = {
	&_stdin,
	&_stdout,
	&_stderr,
	0
};

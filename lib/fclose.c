#include "../include/stdio.h"

fclose(fp)
FILE *fp;
{
	register int i;

	for (i=0; i<NFILES; i++)
		if (fp == _io_table[i]) {
			_io_table[i] = 0;
			break;
		}
	if (i >= NFILES)
		return(EOF);
	fflush(fp);
	close(fp->_fd);
	if ( testflag(fp,IOMYBUF) && fp->_buf )
		free( fp->_buf );
	free(fp);
	return(NULL);
}


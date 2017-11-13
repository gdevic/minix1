#include "../include/stdio.h"


long ftell(iop)
FILE *iop;
{
	long result;
	long lseek();
	int adjust = 0;

	if ( testflag(iop,READMODE) )
		adjust -= iop->_count;
	else if ( testflag(iop,WRITEMODE) && iop->_buf && !testflag(iop,UNBUFF))
		adjust = iop->_ptr - iop->_buf;
	else
		return(-1);
	
	result = lseek(fileno(iop), 0L, 1);

	if ( result < 0 )
		return ( result );

	result += (long) adjust;
	return(result);
}

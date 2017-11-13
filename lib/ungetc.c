#include "../include/stdio.h"

ungetc(ch, iop)
int ch;
FILE *iop;
{
	if ( ch < 0  || !testflag(iop,READMODE) || testflag(iop,UNBUFF) ) 
		return( EOF );
	
	if ( iop->_count >= BUFSIZ)
		return(EOF);

	if ( iop->_ptr == iop->_buf)
		iop->_ptr++;

	iop->_count++;
	*--iop->_ptr = ch;
	return(ch);
}

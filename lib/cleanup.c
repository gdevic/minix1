#include "../include/stdio.h"

_cleanup()
{
	register int i;

	for ( i = 0 ; i < NFILES ; i++ )
		if ( _io_table[i] != NULL )
			fflush(_io_table[i]);
}

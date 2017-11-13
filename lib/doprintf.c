#include "../include/stdio.h"

#define MAXDIGITS 12
#define PRIVATE   static

/* This is the same as varargs , on BSD systems */

#define GET_ARG(arglist,mode) ((mode *)(arglist += sizeof(mode)))[-1]

_doprintf(fp, format, args)
FILE *fp;
register char *format;
int args;
{
	register char *vl;
	int  r,
	    w1, w2,
	    sign;
	long l;
	char c;
	char *s;
	char padchar;
	char a[MAXDIGITS];

	vl = (char *) args;

	while ( *format != '\0') {
		if ( *format != '%' ) {
			putc( *format++, fp );
			continue;
		}

		w1 = 0;
		w2 = 0;
		sign = 1;
		padchar = ' ';
		format++;

		if ( *format == '-' ) {
			sign = -1;
			format++;
		}

		if ( *format == '0') {
			padchar = '0';
			format++;
		}

		while ( *format >='0' && *format <='9' ){
			w1 = 10 * w1 + sign * ( *format - '0' );
			format++;
		}

		if ( *format == '.'){
			format++;
			while ( *format >='0' && *format <='9' ){
				w2 = 10 * w2 + ( *format - '0' );
				format++;
			}
		}

		switch (*format) {
		case 'd':
			l = (long) GET_ARG(vl, int);
			r = 10;
			break;
		case 'u':
			l = (long) GET_ARG(vl, int);
			l = l & 0xFFFF;
			r = 10;
			break;
		case 'o':
			l = (long) GET_ARG(vl, int);
			if (l < 0) l = l & 0xFFFF;
			r = 8;
			break;
		case 'x':
			l = (long) GET_ARG(vl, int);
			if (l < 0) l = l & 0xFFFF;
			r = 16;
			break;
		case 'D':
			l = (long) GET_ARG(vl, long);
			r = 10;
			break;
		case 'O':
			l = (long) GET_ARG(vl, long);
			r = 8;
			break;
		case 'X':
			l = (long) GET_ARG(vl, long);
			r = 16;
			break;
		case 'c':
			c = (char) GET_ARG(vl, int); 
			/* char's are casted back to int's */
			putc( c, fp);
			format++;
			continue;
		case 's':
			s = GET_ARG(vl, char *);
			_printit(s,w1,w2,padchar,strlen(s),fp);
			format++;
			continue;
		default:
			putc('%',fp);
			putc(*format++,fp);
			continue;
		}

		_bintoascii (l, r, a);
		_printit(a,w1,w2,padchar,strlen(a),fp);
		format++;
	}
}



PRIVATE _bintoascii (num, radix, a)
long    num;
int     radix;
char    *a;
{
	char b[MAXDIGITS];
	int hit, negative;
	register int n, i;

	negative = 0;
	if (num == 0) {
		a[0] = '0';
		a[1] = '\0';
		return;
	}
	if (radix == 10) {
		if (num < 0) {num = -num; negative++;}
	} 

	for (n = 0; n < MAXDIGITS; n++)
		b[n] = 0;

	n = 0;

	do {
		if (radix == 10) {
			b[n] = num % 10;
			num = (num - b[n]) / 10;
		}
		if (radix == 8) {
			b[n] = num & 0x7;
			num = (num >> 3) & 0x1FFFFFFF;
		}
		if (radix == 16) {
			b[n] = num & 0xF;
			num = (num >> 4) & 0x0FFFFFFF;
		}
		n++;
	} 
	while (num != 0);

	/* Convert to ASCII. */
	hit = 0;
	for (i = n - 1; i >= 0; i--) {
		if (b[i] == 0 && hit == 0) {
			b[i] = ' ';
		}
		else {
			if (b[i] < 10)
				b[i] += '0';
			else
				b[i] += 'A' - 10;
			hit++;
		}
	}
	if (negative)
		b[n++] = '-';
	
	for(i = n - 1 ; i >= 0 ; i-- )
		*a++ = b[i];
	
	*a = '\0';
	
}


PRIVATE _printit(str, w1, w2, padchar, length, file)
char *str;
int w1, w2;
char padchar;
int length;
FILE *file;
{
	int len2 = length;
	int temp;
	
	if ( w2 > 0  && length > w2) 
		len2 = w2;

	temp = len2;

	if ( w1 > 0 )
		while ( w1 > len2 ){
			--w1;
			putc(padchar,file);
		}
	
	while ( *str && ( len2-- != 0 ))
		putc(*str++, file);

	if ( w1 < 0 )
		if ( padchar == '0' ){
			putc('.',file);
			w1++;
		}
		while ( w1 < -temp ){
			w1++;
			putc(padchar,file);
		}
}

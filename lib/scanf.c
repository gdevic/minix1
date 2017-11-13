/* scanf - formatted input conversion	Author: Patrick van Kleef */

#include <stdio.h>


int scanf (format, args)
char           *format;
unsigned        args;
{
	return _doscanf (0, stdin, format, &args);
}



int fscanf (fp, format, args)
FILE           *fp;
char           *format;
unsigned        args;
{
	return _doscanf (0, fp, format, &args);
}


int sscanf (string, format, args)
char           *string;		/* source of data */
char           *format;		/* control string */
unsigned        args;		/* our args */
{
	return _doscanf (1, string, format, &args);
}


union ptr_union {
	char           *chr_p;
	unsigned int   *uint_p;
	unsigned long  *ulong_p;
};

static int      ic;		/* the current character */
static char    *rnc_arg;	/* the string or the filepointer */
static          rnc_code;	/* 1 = read from string, else from FILE */




/* get the next character */

static rnc ()
{
	if (rnc_code) {
		if (!(ic = *rnc_arg++))
			ic = EOF;
	} else
		ic = getc ((FILE *) rnc_arg);
}



/*
 * unget the current character 
 */

static ugc ()
{

	if (rnc_code)
		--rnc_arg;
	else
		ungetc (ic, rnc_arg);
}



static index(ch, string)
char ch;
char *string;
{
	while (*string++ != ch) 
		if (!*string)
			return 0;
	return 1;
}


/*
 * this is cheaper than iswhite from <ctype.h> 
 */

static iswhite (ch)
int             ch;
{

	return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}





static isdigit (ch)
int             ch;
{
	return (ch >= '0' && ch <= '9');
}




static tolower (ch)
int             ch;
{
	if (ch >= 'A' && ch <= 'Z')
		ch = ch + 'a' - 'A';

	return ch;
}


/*
 * the routine that does the job 
 */

_doscanf (code, funcarg, format, argp)
int             code;		/* function to get a character */
char           *funcarg;	/* an argument for the function */
char           *format;		/* the format control string */
union ptr_union *argp;		/* our argument list */
{
	int             done = 0;	/* number of items done */
	int             base;		/* conversion base */
	long            val;		/* an integer value */
	int             sign;		/* sign flag */
	int             do_assign;	/* assignment suppression flag */
	unsigned        width;		/* width of field */
	int             widflag;	/* width was specified */
	int             longflag;	/* true if long */
	int             done_some;	/* true if we have seen some data */
	int		reverse;	/* reverse the checking in [...] */
	char	       *endbracket;     /* position of the ] in format string */

	rnc_arg = funcarg;
	rnc_code = code;

	rnc ();			/* read the next character */

	if (ic == EOF) {
		done = EOF;
		goto quit;
	}

	while (1) {
		while (iswhite (*format))
			++format;	/* skip whitespace */
		if (!*format)
			goto all_done;	/* end of format */
		if (ic < 0)
			goto quit;	/* seen an error */
		if (*format != '%') {
			while (iswhite (ic))
				rnc ();
			if (ic != *format)
				goto all_done;
			++format;
			rnc ();
			++done;
			continue;
		}
		++format;
		do_assign = 1;
		if (*format == '*') {
			++format;
			do_assign = 0;
		}
		if (isdigit (*format)) {
			widflag = 1;
			for (width = 0; isdigit (*format);)
				width = width * 10 + *format++ - '0';
		} else
			widflag = 0;	/* no width spec */
		if (longflag = (tolower (*format) == 'l'))
			++format;
		if (*format != 'c')
			while (iswhite (ic))
				rnc ();
		done_some = 0;	/* nothing yet */
		switch (*format) {
		case 'o':
			base = 8;
			goto decimal;
		case 'u':
		case 'd':
			base = 10;
			goto decimal;
		case 'x':
			base = 16;
			if (((!widflag) || width >= 2) && ic == '0') {
				rnc ();
				if (tolower (ic) == 'x') {
					width -= 2;
					done_some = 1;
					rnc ();
				} else {
					ugc ();
					ic = '0';
				}
			}
	decimal:
			val = 0L;	/* our result value */
			sign = 0;	/* assume positive */
			if (!widflag)
				width = 0xffff;	/* very wide */
			if (width && ic == '+')
				rnc ();
			else if (width && ic == '-') {
				sign = 1;
				rnc ();
			}
			while (width--) {
				if (isdigit (ic) && ic - '0' < base)
					ic -= '0';
				else if (base == 16 && tolower (ic) >= 'a' && tolower (ic) <= 'f')
					ic = 10 + tolower (ic) - 'a';
				else
					break;
				val = val * base + ic;
				rnc ();
				done_some = 1;
			}
			if (do_assign) {
				if (sign)
					val = -val;
				if (longflag)
					*(argp++)->ulong_p = (unsigned long) val;
				else
					*(argp++)->uint_p = (unsigned) val;
			}
			if (done_some)
				++done;
			else
				goto all_done;
			break;
		case 'c':
			if (!widflag)
				width = 1;
			while (width-- && ic >= 0) {
				if (do_assign)
					*(argp)->chr_p++ = (char) ic;
				rnc ();
				done_some = 1;
			}
			if (do_assign)
				argp++;	/* done with this one */
			if (done_some)
				++done;
			break;
		case 's':
			if (!widflag)
				width = 0xffff;
			while (width-- && !iswhite (ic) && ic > 0) {
				if (do_assign)
					*(argp)->chr_p++ = (char) ic;
				rnc ();
				done_some = 1;
			}
			if (do_assign)		/* terminate the string */
				*(argp++)->chr_p = '\0';	
			if (done_some)
				++done;
			else
				goto all_done;
			break;
		case '[':
			if (!widflag)
				width = 0xffff;

			if ( *(++format) == '^' ) {
				reverse = 1;
				format++;
			} else
				reverse = 0;
			
			endbracket = format;
			while ( *endbracket != ']'  && *endbracket != '\0')
				endbracket++;
			
			if (!*endbracket)
				goto quit;
			
			*endbracket = '\0';	/* change format string */

			while (width-- && !iswhite (ic) && ic > 0 &&
				(index (ic, format) ^ reverse)) {
				if (do_assign)
					*(argp)->chr_p++ = (char) ic;
				rnc ();
				done_some = 1;
			}
			format = endbracket;
			*format = ']';		/* put it back */
			if (do_assign)		/* terminate the string */
				*(argp++)->chr_p = '\0';	
			if (done_some)
				++done;
			else
				goto all_done;
			break;
		}		/* end switch */
		++format;
	}
all_done:
	if (ic >= 0)
		ugc ();		/* restore the character */
quit:
	return done;
}

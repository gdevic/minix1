/* grep - search for a pattern 		Author: Martin C. Atkins */

/*
 *	Search files for a regular expression
 *
 *<-xtx-*>cc -o grep grep.c -lregexp
 */

/*
 *	This program was written by:
 *		Martin C. Atkins,
 *		University of York,
 *		Heslington,
 *		York. Y01 5DD
 *		England
 *	and is released into the public domain, on the condition
 *	that this comment is always included without alteration.
 *
 *	The program was modified by Andy Tanenbaum.
 */

#include "regexp.h"
#include "stdio.h"

#define MAXLINE (1024)
int status = 1;
char *progname;
int pmflag = 1;		/* print lines which match */
int pnmflag = 0;	/* print lines which don't match */
int nflag = 0;		/* number the lines printed */
int args;
extern char *index();

main(argc,argv)
int argc;
char *argv[];
{
  regexp *exp;
  char **argp = &argv[1];

  if (!isatty(1)) setbuf(stdout);
  args = argc;
  progname = argv[0];
  while(*argp != 0 && argp[0][0] == '-') {
	args--;			/* flags don't count */
	switch(argp[0][1]) {
	case 'v':
		pmflag = 0;
		pnmflag = 1;
		break;
	case 'n':
		nflag++;
		break;
	case 's':
		pmflag = pnmflag = 0;
		break;
	case 'e':
		argp++;
		goto out;
	default:
		usage();
	}
	argp++;
  }
out:
  if(*argp == 0) usage();

  if((exp = regcomp(*argp++)) == NULL) {
  	std_err("grep: regcomp failed\n");
	done(2);
  }
  if(*argp == 0)
	match((char *)0,exp);
  else
	while(*argp) {
		int infd;

		if(strcmp(*argp,"-") == 0)
			match("-",exp);
		else {
			fclose(stdin);
			if(fopen(*argp, "r") == NULL) {
				std_err("Can't open ");
				std_err(*argp);
				std_err("\n");
				status = 2;
			} else {
				match(*argp,exp);
				close(infd);
			}
		}
		argp++;
	}
  done(status);
}

/*
 *	This routine actually matches the file
 */
match(name, exp)
char *name;
regexp *exp;
{
  char buf[MAXLINE];
  int lineno = 0;

  while(getline(buf,MAXLINE) != NULL) {
	char *cr = index(buf,'\n');
	lineno++;
	if(cr == 0) {
		std_err("Line too long in ");
		std_err(name == 0 ? "stdin":name);
	} else
		*cr = '\0';
	if(regexec(exp,buf)) {
		if(pmflag)
			pline(name,lineno,buf);
		if(status != 2)
			status = 0;
	} else if(pnmflag)
		pline(name,lineno,buf);
  }
}
void regerror(s)
char *s;
{
  std_err("grep: ");
  std_err(s);
  std_err("\n");
  done(2);

}

pline(name, lineno, buf)
char *name;
int lineno;
char buf[];
{
  if(name && args > 3) prints("%s:",name);
  if(nflag) prints("%s:", itoa(lineno));
  prints("%s\n",buf);
}


usage()
{
  std_err("Usage: grep [-v] [-n] [-s] [-e expr] expression [file ...]\n");
  done(2);
}

getline(buf, size)
char *buf;
int size;
{
  char *initbuf = buf, c;

  while (1) {
	c = getc(stdin);
  	*buf++ = c;
	if (c <= 0) return(NULL);
  	if (buf - initbuf == size - 1) return(buf - initbuf);
  	if (c == '\n') return(buf - initbuf);
  }
}

done(n)
int n;
{
  fflush(stdout);
  exit(n);
}

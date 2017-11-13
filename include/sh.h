/* -------- sh.h -------- */
/*
 * shell
 */

#define	NULL	0
#define	LINELIM	1000
#define	NPUSH	8	/* limit to input nesting */

#define	NOFILE	20	/* Number of open files */
#define	NUFILE	10	/* Number of user-accessible files */
#define	FDBASE	10	/* First file usable by Shell */

/*
 * values returned by wait
 */
#define	WAITSIG(s) ((s)&0177)
#define	WAITVAL(s) (((s)>>8)&0377)
#define	WAITCORE(s) (((s)&0200)!=0)

/*
 * library and system defintions
 */
typedef int xint;	/* base type of jmp_buf, for broken compilers */

/*
 * shell components
 */
/* #include "area.h" */
/* #include "word.h" */
/* #include "io.h" */
/* #include "var.h" */

#define	QUOTE	0200

#define	NOBLOCK	((struct op *)NULL)
#define	NOWORD	((char *)NULL)
#define	NOWORDS	((char **)NULL)
#define	NOPIPE	((int *)NULL)

/*
 * Description of a command or an operation on commands.
 * Might eventually use a union.
 */
struct op {
	int	type;	/* operation type, see below */
	char	**words;	/* arguments to a command */
	struct	ioword	**ioact;	/* IO actions (eg, < > >>) */
	struct op *left;
	struct op *right;
	char	*str;	/* identifier for case and for */
};

#define	TCOM	1	/* command */
#define	TPAREN	2	/* (c-list) */
#define	TPIPE	3	/* a | b */
#define	TLIST	4	/* a [&;] b */
#define	TOR	5	/* || */
#define	TAND	6	/* && */
#define	TFOR	7
#define	TDO	8
#define	TCASE	9
#define	TIF	10
#define	TWHILE	11
#define	TUNTIL	12
#define	TELIF	13
#define	TPAT	14	/* pattern in case */
#define	TBRACE	15	/* {c-list} */
#define	TASYNC	16	/* c & */

/*
 * actions determining the environment of a process
 */
#define	BIT(i)	(1<<(i))
#define	FEXEC	BIT(0)	/* execute without forking */

/*
 * flags to control evaluation of words
 */
#define	DOSUB	1	/* interpret $, `, and quotes */
#define	DOBLANK	2	/* perform blank interpretation */
#define	DOGLOB	4	/* interpret [?* */
#define	DOKEY	8	/* move words with `=' to 2nd arg. list */
#define	DOTRIM	16	/* trim resulting string */

#define	DOALL	(DOSUB|DOBLANK|DOGLOB|DOKEY|DOTRIM)

char	**dolv;
int	dolc;
int	exstat;
char	gflg;
int	talking;	/* interactive (talking-type wireless) */
int	execflg;
int	multiline;	/* \n changed to ; */
struct	op	*outtree;	/* result from parser */

xint	*failpt;
xint	*errpt;

struct	brkcon {
	jmp_buf	brkpt;
	struct	brkcon	*nextlev;
} *brklist;
int	isbreak;

/*
 * redirection
 */
struct ioword {
	short	io_unit;	/* unit affected */
	short	io_flag;	/* action (below) */
	union {
		char	*io_name;	/* file name */
		struct block *io_here;	/* here structure pointer */
	} io_un;
};
#define	IOREAD	1	/* < */
#define	IOHERE	2	/* << (here file) */
#define	IOWRITE	4	/* > */
#define	IOCAT	8	/* >> */
#define	IOXHERE	16	/* ${}, ` in << */
#define	IODUP	32	/* >&digit */
#define	IOCLOSE	64	/* >&- */

#define	IODEFAULT (-1)	/* token for default IO unit */

struct	wdblock	*wdlist;
struct	wdblock	*iolist;

/*
 * parsing & execution environment
 */
extern struct	env {
	char	*linep;
	struct	io	*iobase;
	struct	io	*iop;
	xint	*errpt;
	int	iofd;
	struct	env	*oenv;
} e;

/*
 * flags:
 * -e: quit on error
 * -k: look for name=value everywhere on command line
 * -n: no execution
 * -t: exit after reading and executing one command
 * -v: echo as read
 * -x: trace
 * -u: unset variables net diagnostic
 */
char	*flag;

char	*null;	/* null value for variable */
int	intr;	/* interrupt pending */

char	*trap[NSIG];
char	ourtrap[NSIG];
int	trapset;	/* trap pending */

int	inword;	/* defer traps and interrupts */

int	yynerrs;	/* yacc */

char	line[LINELIM];
char	*elinep;

/*
 * other functions
 */
int	(*inbuilt())();	/* find builtin command */
char	*rexecve();
char	*space();
char	*getwd();
char	*strsave();
char	*evalstr();
char	*putn();
char	*itoa();
char	*unquote();
struct	var	*lookup();
struct	wdblock	*add2args();
struct	wdblock	*glob();
char	**makenv();
struct	ioword	*addio();
char	**eval();
int	setstatus();
int	waitfor();

int	onintr();	/* SIGINT handler */

/*
 * error handling
 */
void	leave();	/* abort shell (or fail in subshell) */
void	fail();		/* fail but return to process next command */
int	sig();		/* default signal handler */

/*
 * library functions and system calls
 */
long	lseek();
char	*strncpy();
int	strlen();
extern int errno;

/* -------- var.h -------- */

struct	var {
	char	*value;
	char	*name;
	struct	var	*next;
	char	status;
};
#define	COPYV	1	/* flag to setval, suggesting copy */
#define	RONLY	01	/* variable is read-only */
#define	EXPORT	02	/* variable is to be exported */
#define	GETCELL	04	/* name & value space was got with getcell */

struct	var	*vlist;		/* dictionary */

struct	var	*homedir;	/* home directory */
struct	var	*prompt;	/* main prompt */
struct	var	*cprompt;	/* continuation prompt */
struct	var	*path;		/* search path for commands */
struct	var	*shell;		/* shell to interpret command files */
struct	var	*ifs;		/* field separators */

struct	var	*lookup(/* char *s */);
void	setval(/* struct var *, char * */);
void	nameval(/* struct var *, char *val, *name */);
void	export(/* struct var * */);
void	ronly(/* struct var * */);
int	isassign(/* char *s */);
int	checkname(/* char *name */);
int	assign(/* char *s, int copyflag */);
void	putvlist(/* int key, int fd */);
int	eqname(/* char *n1, char *n2 */);

/* -------- io.h -------- */
/* possible arguments to an IO function */
struct ioarg {
	char	*aword;
	char	**awordlist;
	int	afile;	/* file descriptor */
};

/* an input generator's state */
struct	io {
	int	(*iofn)();
	struct	ioarg	arg;
	int	peekc;
	char	nlcount;	/* for `'s */
	char	xchar;		/* for `'s */
	char	task;		/* reason for pushed IO */
};
struct	io	iostack[NPUSH];
#define	XOTHER	0	/* none of the below */
#define	XDOLL	1	/* expanding ${} */
#define	XGRAVE	2	/* expanding `'s */
#define	XIO	3	/* file IO */

/* in substitution */
#define	INSUB()	(e.iop->task == XGRAVE || e.iop->task == XDOLL)

/*
 * input generators for IO structure
 */
int	nlchar();
int	strchar();
int	filechar();
int	linechar();
int	nextchar();
int	gravechar();
int	qgravechar();
int	dolchar();
int	wdchar();

/*
 * IO functions
 */
int	getc();
int	readc();
void	unget();
void	ioecho();
void	prs();
void	putc();
void	prn();
void	closef();
void	closeall();

/*
 * IO control
 */
void	pushio(/* struct ioarg arg, int (*gen)() */);
int	remap();
int	openpipe();
void	closepipe();
struct	io	*setbase(/* struct io * */);

struct	ioarg	temparg;	/* temporary for PUSHIO */
#define	PUSHIO(what,arg,gen) ((temparg.what = (arg)),pushio(temparg,(gen)))
#define	RUN(what,arg,gen) ((temparg.what = (arg)), run(temparg,(gen)))

/* -------- word.h -------- */
#ifndef WORD_H
#define	WORD_H	1
struct	wdblock {
	short	w_bsize;
	short	w_nword;
	/* bounds are arbitrary */
	char	*w_words[1];
};

struct	wdblock	*addword();
struct	wdblock	*newword();
char	**getwords();
#endif

/* -------- area.h -------- */

/*
 * storage allocation
 */
char	*getcell(/* unsigned size */);
void	garbage();
void	setarea(/* char *obj, int to */);
void	freearea(/* int area */);
void	freecell(/* char *obj */);

int	areanum;	/* current allocation area */

#define	NEW(type) (type *)getcell(sizeof(type))
#define	DELETE(obj)	freecell((char *)obj)


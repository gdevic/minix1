#include "signal.h"
#include "errno.h"
#include "setjmp.h"
#include "sh.h"
/* -------- sh.c -------- */
/*
 * shell
 */

/* #include "sh.h" */

int	intr;
int	inparse;
char	flags['z'-'a'+1];
char	*flag = flags-'a';
char	*elinep = line+sizeof(line)-5;
char	*null	= "";
int	inword	=1;
struct	env	e ={line, iostack, iostack-1, NULL, FDBASE, NULL};

char	**environ;	/* environment pointer */

/*
 * default shell, search rules
 */
char	shellname[] = "/bin/sh";
char	search[] = ":/bin:/usr/bin";

int	(*qflag)() = SIG_IGN;

main(argc, argv)
int argc;
register char **argv;
{
	register int f;
	register char *s;
	int cflag;
	char *name, **ap;
	int (*iof)();

	if ((ap = environ) != NULL) {
		while (*ap)
			assign(*ap++, !COPYV);
		for (ap = environ; *ap;)
			export(lookup(*ap++));
	}
	closeall();
	areanum = 1;

	shell = lookup("SHELL");
	if (shell->value == null)
		setval(shell, shellname);
	export(shell);

	homedir = lookup("HOME");
	if (homedir->value == null)
		setval(homedir, "/");
	export(homedir);

	setval(lookup("$"), itoa(getpid(), 5));

	path = lookup("PATH");
	if (path->value == null)
		setval(path, search);
	export(path);

	ifs = lookup("IFS");
	if (ifs->value == null)
		setval(ifs, " \t\n");

	prompt = lookup("PS1");
	if (prompt->value == null)
#ifndef UNIXSHELL
		setval(prompt, "$ ");
#else
		setval(prompt, "% ");
#endif
	if (geteuid() == 0) {
		setval(prompt, "# ");
		prompt->status &= ~EXPORT;
	}
	cprompt = lookup("PS2");
	if (cprompt->value == null)
		setval(cprompt, "> ");

	iof = filechar;
	cflag = 0;
	name = *argv++;
	if (--argc >= 1) {
		if(argv[0][0] == '-' && argv[0][1] != '\0') {
			for (s = argv[0]+1; *s; s++)
				switch (*s) {
				case 'c':
					prompt->status &= ~EXPORT;
					cprompt->status &= ~EXPORT;
					setval(prompt, "");
					setval(cprompt, "");
					cflag = 1;
					if (--argc > 0)
						PUSHIO(aword, *++argv, iof = nlchar);
					break;
	
				case 'q':
					qflag = SIG_DFL;
					break;

				case 's':
					/* standard input */
					break;

				case 't':
					prompt->status &= ~EXPORT;
					setval(prompt, "");
					iof = linechar;
					break;
	
				case 'i':
					talking++;
				default:
					if (*s>='a' && *s<='z')
						flag[*s]++;
				}
		} else {
			argv--;
			argc++;
		}
		if (iof == filechar && --argc > 0) {
			setval(prompt, "");
			setval(cprompt, "");
			prompt->status &= ~EXPORT;
			cprompt->status &= ~EXPORT;
			if (newfile(*++argv))
				exit(1);
		}
	}
	setdash();
	if (e.iop < iostack) {
		PUSHIO(afile, 0, iof);
		if (isatty(0) && isatty(1) && !cflag)
			talking++;
	}
	signal(SIGQUIT, qflag);
	if (name[0] == '-') {
		talking++;
		if ((f = open("/etc/profile", 0)) >= 0)
			next(remap(f));
		if ((f = open(".profile", 0)) >= 0)
			next(remap(f));
	}
	if (talking) {
		signal(SIGTERM, sig);
		signal(SIGINT, SIG_IGN);
	}
	dolv = argv;
	dolc = argc;
	dolv[0] = name;
	if (dolc > 1)
		for (ap = ++argv; --argc > 0;)
			if (assign(*ap = *argv++, !COPYV))
				dolc--;	/* keyword */
			else
				ap++;
	setval(lookup("#"), putn(dolc));

	for (;;) {
		if (talking && e.iop <= iostack)
			prs(prompt->value);
		onecommand();
	}
}

setdash()
{
	register char *cp, c;
	char m['z'-'a'+1];

	cp = m;
	for (c='a'; c<='z'; c++)
		if (flag[c])
			*cp++ = c;
	*cp = 0;
	setval(lookup("-"), m);
}

newfile(s)
register char *s;
{
	register f;

	if (strcmp(s, "-") != 0) {
		f = open(s, 0);
		if (f < 0) {
			prs(s);
			err(": cannot open");
			return(1);
		}
	} else
		f = 0;
	next(remap(f));
	return(0);
}

onecommand()
{
	register char *cp;
	register i;
	jmp_buf m1;

	inword++;
	while (e.oenv)
		quitenv();
	freearea(areanum = 1);
	cp = getcell(258);
	garbage();
	DELETE(cp);
	wdlist = 0;
	iolist = 0;
	e.errpt = 0;
	e.linep = line;
	yynerrs = 0;
	multiline = 0;
	inparse = 1;
	if (talking)
		signal(SIGINT, onintr);
	if (setjmp(failpt = m1) || yyparse() || intr) {
		while (e.oenv)
			quitenv();
		scraphere();
		inparse = 0;
		intr = 0;
		return;
	}
	inparse = 0;
	inword = 0;
	if ((i = trapset) != 0) {
		trapset = 0;
		runtrap(i);
	}
	brklist = 0;
	intr = 0;
	execflg = 0;
	if (!flag['n']) {
		if (talking)
			signal(SIGINT, onintr);
		execute(outtree, NOPIPE, NOPIPE, 0);
		intr = 0;
		if (talking)
			signal(SIGINT, SIG_IGN);
	}
}

void
fail()
{
	longjmp(failpt, 1);
	/* NOTREACHED */
}

void
leave()
{
	if (execflg)
		fail();
	runtrap(0);
	sync();
	exit(exstat);
	/* NOTREACHED */
}

warn(s)
register char *s;
{
	if(*s) {
		prs(s);
		exstat = -1;
	}
	prs("\n");
	if (flag['e'])
		leave();
}

err(s)
char *s;
{
	warn(s);
	if (flag['n'])
		return;
	if (!talking)
		leave();
	if (e.errpt)
		longjmp(e.errpt, 1);
	closeall();
	e.iop = e.iobase = iostack;
}

newenv(f)
{
	register struct env *ep;

	if (f) {
		quitenv();
		return(1);
	}
	ep = (struct env *) space(sizeof(*ep));
	if (ep == NULL) {
		while (e.oenv)
			quitenv();
		fail();
	}
	*ep = e;
	e.oenv = ep;
	e.errpt = errpt;
	return(0);
}

quitenv()
{
	register struct env *ep;
	register fd;

	if ((ep = e.oenv) != NULL) {
		fd = e.iofd;
		e = *ep;
		/* should close `'d files */
		DELETE(ep);
		while (--fd >= e.iofd)
			close(fd);
	}
}

/*
 * Is any character from s1 in s2?
 */
int
anys(s1, s2)
register char *s1, *s2;
{
	while (*s1)
		if (any(*s1++, s2))
			return(1);
	return(0);
}

/*
 * Is character c in s?
 */
int
any(c, s)
register int c;
register char *s;
{
	while (*s)
		if (*s++ == c)
			return(1);
	return(0);
}

char *
putn(n)
register n;
{
	return(itoa(n, -1));
}

char *
itoa(u, n)
register unsigned u;
{
	register char *cp;
	static char s[20];
	int m;

	m = 0;
	if (n < 0 && (int) u < 0) {
		m++;
		u = -u;
	}
	cp = s+sizeof(s);
	*--cp = 0;
	do {
		*--cp = u%10 + '0';
		u /= 10;
	} while (--n > 0 || u);
	if (m)
		*--cp = '-';
	return(cp);
}

next(f)
{
	PUSHIO(afile, f, nextchar);
}

onintr()
{
	signal(SIGINT, SIG_IGN);
	if (inparse) {
		prs("\n");
		fail();
	}
	intr++;
}

letter(c)
register c;
{
	return(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_');
}

digit(c)
register c;
{
	return(c >= '0' && c <= '9');
}

letnum(c)
register c;
{
	return(letter(c) || digit(c));
}

char *
space(n)
int n;
{
	register char *cp;

	inword++;
	if ((cp = getcell(n)) == 0)
		err("out of string space");
	inword--;
	return(cp);
}

char *
strsave(s, a)
register char *s;
{
	register char *cp, *xp;

	if ((cp = space(strlen(s)+1)) != NULL) {
		setarea((char *)cp, a);
		for (xp = cp; (*xp++ = *s++) != '\0';)
			;
		return(cp);
	}
	return("");
}

/*
 * if inword is set, traps
 * are delayed, avoiding
 * having two people allocating
 * at once.
 */
xfree(s)
register char *s;
{
	inword++;
	DELETE(s);
	inword--;
}

/*
 * trap handling
 */
sig(i)
register i;
{
	if (inword == 0) {
		signal(i, SIG_IGN);
		runtrap(i);
	} else
		trapset = i;
	signal(i, sig);
}

runtrap(i)
{
	char *trapstr;

	if ((trapstr = trap[i]) == NULL)
		return;
	if (i == 0)
		trap[i] = 0;
	RUN(aword, trapstr, nlchar);
}

/* -------- var.c -------- */
/* #include "sh.h" */

static	char	*findeq();

/*
 * Find the given name in the dictionary
 * and return its value.  If the name was
 * not previously there, enter it now and
 * return a null value.
 */
struct var *
lookup(n)
register char *n;
{
	register struct var *vp;
	register char *cp;
	register int c;
	static struct var dummy;

	if (digit(*n)) {
		dummy.name = n;
		for (c = 0; digit(*n) && c < 1000; n++)
			c = c*10 + *n-'0';
		dummy.status = RONLY;
		dummy.value = c <= dolc? dolv[c]: null;
		return(&dummy);
	}
	for (vp = vlist; vp; vp = vp->next)
		if (eqname(vp->name, n))
			return(vp);
	cp = findeq(n);
	vp = (struct var *)space(sizeof(*vp));
	if (vp == 0 || (vp->name = space(cp-n+2)) == 0) {
		dummy.name = dummy.value = "";
		return(&dummy);
	}
	for (cp = vp->name; (*cp = *n++) && *cp != '='; cp++)
		;
	if (*cp == 0)
		*cp = '=';
	*++cp = 0;
	setarea((char *)vp, 0);
	setarea((char *)vp->name, 0);
	vp->value = null;
	vp->next = vlist;
	vp->status = GETCELL;
	vlist = vp;
	return(vp);
}

/*
 * give variable at `vp' the value `val'.
 */
void
setval(vp, val)
struct var *vp;
char *val;
{
	nameval(vp, val, (char *)NULL);
}

/*
 * if name is not NULL, it must be
 * a prefix of the space `val',
 * and end with `='.
 * this is all so that exporting
 * values is reasonably painless.
 */
void
nameval(vp, val, name)
register struct var *vp;
char *val, *name;
{
	register char *cp, *xp;
	char *nv;
	int fl;

	if (vp->status & RONLY) {
		for (xp = vp->name; *xp && *xp != '=';)
			putc(*xp++);
		err(" is read-only");
		return;
	}
	fl = 0;
	if (name == NULL) {
		xp = space(strlen(vp->name)+strlen(val)+2);
		if (xp == 0)
			return;
		/* make string:  name=value */
		setarea((char *)xp, 0);
		name = xp;
		for (cp = vp->name; (*xp = *cp++) && *xp!='='; xp++)
			;
		if (*xp++ == 0)
			xp[-1] = '=';
		nv = xp;
		for (cp = val; (*xp++ = *cp++) != '\0';)
			;
		val = nv;
		fl = GETCELL;
	}
	if (vp->status & GETCELL)
		xfree(vp->name);	/* form new string `name=value' */
	vp->name = name;
	vp->value = val;
	vp->status |= fl;
}

void
export(vp)
struct var *vp;
{
	vp->status |= EXPORT;
}

void
ronly(vp)
struct var *vp;
{
	if (letter(vp->name[0]))	/* not an internal symbol ($# etc) */
		vp->status |= RONLY;
}

int
isassign(s)
register char *s;
{
	if (!letter(*s))
		return(0);
	for (; *s != '='; s++)
		if (*s == 0 || !letnum(*s))
			return(0);
	return(1);
}

int
assign(s, cf)
register char *s;
int cf;
{
	register char *cp;
	struct var *vp;

	if (!letter(*s))
		return(0);
	for (cp = s; *cp != '='; cp++)
		if (*cp == 0 || !letnum(*cp))
			return(0);
	vp = lookup(s);
	nameval(vp, ++cp, cf == COPYV? NULL: s);
	if (cf != COPYV)
		vp->status &= ~GETCELL;
	return(1);
}

int
checkname(cp)
register char *cp;
{
	if (!letter(*cp++))
		return(0);
	while (*cp)
		if (!letnum(*cp++))
			return(0);
	return(1);
}

void
putvlist(f, out)
register int f, out;
{
	register struct var *vp;

	for (vp = vlist; vp; vp = vp->next)
		if (vp->status & f && letter(*vp->name)) {
			if (vp->status & EXPORT)
				write(out, "export ", 7);
			if (vp->status & RONLY)
				write(out, "readonly ", 9);
			write(out, vp->name, findeq(vp->name) - vp->name);
			write(out, "\n", 1);
		}
}

int
eqname(n1, n2)
register char *n1, *n2;
{
	for (; *n1 != '=' && *n1 != 0; n1++)
		if (*n2++ != *n1)
			return(0);
	return(*n2 == 0 || *n2 == '=');
}

static char *
findeq(cp)
register char *cp;
{
	while (*cp != '\0' && *cp != '=')
		cp++;
	return(cp);
}

/* -------- gmatch.c -------- */
/*
 * int gmatch(string, pattern)
 * char *string, *pattern;
 *
 * Match a pattern as in sh(1).
 */

#define	NULL	0
#define	CMASK	0377
#define	QUOTE	0200
#define	QMASK	(CMASK&~QUOTE)
#define	NOT	'!'	/* might use ^ */

static	char	*cclass();

int
gmatch(s, p)
register char *s, *p;
{
	register int sc, pc;

	if (s == NULL || p == NULL)
		return(0);
	while ((pc = *p++ & CMASK) != '\0') {
		sc = *s++ & QMASK;
		switch (pc) {
		case '[':
			if ((p = cclass(p, sc)) == NULL)
				return(0);
			break;

		case '?':
			if (sc == 0)
				return(0);
			break;

		case '*':
			s--;
			do {
				if (*p == '\0' || gmatch(s, p))
					return(1);
			} while (*s++ != '\0');
			return(0);

		default:
			if (sc != (pc&~QUOTE))
				return(0);
		}
	}
	return(*s == 0);
}

static char *
cclass(p, sub)
register char *p;
register int sub;
{
	register int c, d, not, found;

	if ((not = *p == NOT) != 0)
		p++;
	found = not;
	do {
		if (*p == '\0')
			return(NULL);
		c = *p & CMASK;
		if (p[1] == '-' && p[2] != ']') {
			d = p[2] & CMASK;
			p++;
		} else
			d = c;
		if (c == sub || c <= sub && sub <= d)
			found = !not;
	} while (*++p != ']');
	return(found? p+1: NULL);
}

/* -------- area.c -------- */
#define GROWBY	1024
#define SHRINKBY	256
#define FREE 32767
#define BUSY 0
#define	ALIGN (sizeof(int)-1)

/* #include "area.h" */
#define	NULL	0

struct region {
	struct	region *next;
	int	area;
};

/*initial empty arena*/
extern	struct region area1;
static	struct region area2 = {&area1, BUSY};
static	struct region area1 = {&area2, BUSY};
static	struct region *areap = &area1;
static	struct region *areatop = &area1;
static	struct region *areabrk;
char	*sbrk();
char	*brk();

char *
getcell(nbytes)
unsigned nbytes;
{
	register int rbytes;
	register struct region *p, *q;
	struct region *newbrk;

	if (areabrk == NULL)
		areabrk = (struct region *)(((int)sbrk(0)+ALIGN)&~ALIGN);

	rbytes = (nbytes+sizeof(struct region)-1)/sizeof(struct region) + 1;
	p=areap;
	for (;;) {
		do {
			if (p->area > areanum) {
				while ((q = p->next)->area > areanum)
					p->next = q->next;
				if (q >= p+rbytes) {
					areap = p+rbytes;
					if (q > areap) {
						areap->next = p->next;
						areap->area = FREE;
					}
					p->next = areap;
					p->area = areanum;
					return((char *)(p+1));
				}
			}
			q = p; p = p->next;
		} while (q >= areap || p < areap);
		newbrk = (struct region *)sbrk(rbytes>=GROWBY? rbytes: GROWBY);
		if ((int)newbrk == -1)
			return(NULL);
		newbrk = (struct region *)sbrk(0);
		areatop->next = areabrk;
		areatop->area = ((q=areabrk)!=areatop+1)? BUSY: FREE;
		areatop = areabrk->next = newbrk-1;
		areabrk->area = FREE; areabrk = newbrk;
		areatop->next = &area2; areatop->area=BUSY;
	}
}

void
freecell(cp)
char *cp;
{
	register struct region *p;

	if ((p = (struct region *)cp) != NULL) {
		p--;
		if (p < areap)
			areap = p;
		p->area = FREE;
	}
}

void
freearea(a)
register int a;
{
	register struct region *p, *top;

	top = areatop;
	p = &area1;
	do {
		if (p->area >= a)
			p->area = FREE;
		p = p->next;
	} while (p != top);
}

void
setarea(cp,a)
char *cp;
int a;
{
	register struct region *p;

	if ((p = (struct region *)cp) != NULL)
		(p-1)->area = a;
}

void
garbage()
{
	register struct region *p, *q, *top;
	register int nu;

	top = areatop;

	areap = p = &area1;
	do {
		if (p->area>areanum) {
			while ((q = p->next)->area > areanum)
				p->next = q->next;
			areap = p;
		}
		q = p; p = p->next;
	} while (p != top);
#if STOPSHRINK == 0
	nu = (SHRINKBY+sizeof(struct region)-1)/sizeof(struct region);
	if (areatop >= q+nu &&
	    q->area > areanum) {
		brk((char *)(areabrk -= nu));
		q->next = areatop = areabrk-1;
		areatop->next = &area2;
		areatop->area = BUSY;
	}
#endif
}


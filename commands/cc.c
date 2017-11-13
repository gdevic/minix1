/*	$Header: cc.c,v 1.2 86/08/26 09:40:10 erikb Locked $
	Driver for the CEMCOM compiler: works like /bin/cc and accepts the
	options accepted by /bin/cc and /usr/em/bin/ack.
	Derived from: "cem.c,v 1.5 86/01/20 11:10:29 erikb Exp"

	Date written: Dec 4, 1985
	Adapted for PC/IX on Jan 20, 1986
	Strongly reduced (May 14, 1986)
	Piping output from cpp into cem (Jul 30, 1986)
	Create temporary files in TMP directory (Aug 6, 1986)
	Pass hint for optimization to cg (Aug 15, 1986)
	Throw away intermediate files on interrupts (Aug 15, 1986)
	Print file name if there are more than one source files (Sep 22, 1986)
	Author: Erik Baalbergen
*/
	
#include <errno.h>
#include <signal.h>

#define MAXARGC	64	/* maximum number of arguments allowed in a list */
#define USTR_SIZE	64	/* maximum length of string variable */

typedef char USTRING[USTR_SIZE];

struct arglist {
	int al_argc;
	char *al_argv[MAXARGC];
};

!!!!_SEE_BELOW_!!!
/* This is not an error.  It is a dirty trick to force the user to read this
 * comment.  The program cc calls the various passes of the compiler.  To call
 * them, it must know where they are.  On the 640K PC MINIX, cpp and cem are
 * kept in /lib, on the root device.  Thus the symbol PP is defined as
 * /lib/cpp, etc.  On the 512K AT, there is no room on the root device, so cpp
 * and cem are kept in /usr/lib, which means that PP must be /usr/lib/cpp,
 * etc.  One of the following two definitions must be uncommented, to 
 * generate the right paths.  For 640K machines (PCs or Ats), MEM640K should
 * be defined.  For 512K machines, MEM512K should be defined.
 */

/* #define MEM640K */
/* #define MEM512K */

#ifdef MEM640K
/* MINIX paths for 640K PC (not 512K AT) */
char *PP     = "/lib/cpp";
char *CEM    = "/lib/cem";
char *OPT    = "/usr/lib/opt";
char *CG     = "/usr/lib/cg";
char *ASLD   = "/usr/bin/asld";
char *SHELL  = "/bin/sh";
char *LIBDIR = "/usr/lib";
#endif

#ifdef MEM512K
/* MINIX paths for 512K AT (not 640K PC) */
char *PP     = "/usr/lib/cpp";
char *CEM    = "/usr/lib/cem";
char *OPT    = "/usr/lib/opt";
char *CG     = "/usr/lib/cg";
char *ASLD   = "/usr/bin/asld";
char *SHELL  = "/bin/sh";
char *LIBDIR = "/usr/lib";
#endif

/* object sizes */
char *V_FLAG = "-Vs2.2w2.2i2.2l4.2f4.2d8.2p2.2";

struct arglist LD_HEAD = {
	1,
	{
		"/usr/lib/crtso.s",
	}
};

struct arglist LD_TAIL = {
	2,
	{
		"/usr/lib/libc.a",
		"/usr/lib/end.s"
	}
};

char *o_FILE = "a.out"; /* default name for executable file */

#define remove(str)	(unlink(str), (str)[0] = '\0')
#define cleanup(str)		(str && remove(str))
#define init(al)		(al)->al_argc = 1
#define library(nm) \
	mkstr(alloc((unsigned int)strlen(nm) + strlen(LIBDIR) + 7), \
		LIBDIR, "/lib", nm, ".a", 0)

char *ProgCall = 0;

struct arglist SRCFILES;
struct arglist LDFILES;
struct arglist GEN_LDFILES;

struct arglist PP_FLAGS;
struct arglist CEM_FLAGS;

int RET_CODE = 0;

struct arglist OPT_FLAGS;
struct arglist CG_FLAGS;
struct arglist ASLD_FLAGS;
struct arglist DEBUG_FLAGS;

struct arglist CALL_VEC[2];

int o_flag = 0;
int S_flag = 0;
int v_flag = 0;
int F_flag = 0;	/* use pipes by default */

char *mkstr();
char *alloc();

USTRING ifile, kfile, sfile, mfile, ofile;
USTRING BASE;

char *tmpdir = "/tmp";
char tmpname[15];

#ifdef DEBUG
int noexec = 0;
#endif

trapcc(sig)
	int sig;
{
	signal(sig, SIG_IGN);
	cleanup(ifile);
	cleanup(kfile);
	cleanup(sfile);
	cleanup(mfile);
	cleanup(ofile);
}

main(argc, argv)
	char *argv[];
{
	char *str;
	char **argvec;
	int count;
	int ext;
	register struct arglist *call = &CALL_VEC[0], *call1 = &CALL_VEC[1];
	char *file;
	char *ldfile = 0;

	ProgCall = *argv++;

	signal(SIGHUP, trapcc);
	signal(SIGINT, trapcc);
	signal(SIGQUIT, trapcc);
	while (--argc > 0) {
		if (*(str = *argv++) != '-') {
			append(&SRCFILES, str);
			continue;
		}

		switch (str[1]) {

		case 'c':
			S_flag = 1;
			break;
		case 'D':
		case 'I':
		case 'U':
			append(&PP_FLAGS, str);
			break;
		case 'F':
			F_flag = 1;
			break;
		case 'l':
			append(&SRCFILES, library(&str[2]));
			break;
		case 'o':
			o_flag = 1;
			if (argc-- >= 0)
				o_FILE = *argv++;
			break;
		case 'O':
			append(&CG_FLAGS, "-p4");
			break;
		case 'S':
			S_flag = 1;
			break;
		case 'L':
			if (strcmp(&str[1], "LIB") == 0) {
				append(&OPT_FLAGS, "-L");
				break;
			}
			/*FALLTHROUGH*/
		case 'v':
			v_flag++;
#ifdef DEBUG
			if (str[2] == 'n')
				noexec = 1;
#endif DEBUG
			break;
		case 'T':
			tmpdir = &str[2];
			/*FALLTHROUGH*/
		case 'R':
		case 'p':
		case 'w':
			append(&CEM_FLAGS, str);
			break;

		default:
			append(&ASLD_FLAGS, str);
			break;
		}
	}
	mktempname(tmpname);

	count = SRCFILES.al_argc;
	argvec = &(SRCFILES.al_argv[0]);

	while (count-- > 0) {
		register char *f;
		basename(file = *argvec++, BASE);

		if (SRCFILES.al_argc > 1) {
			write(1, file, strlen(file));
			write(1, ":\n", 2);
		}
			
		ext = extension(file);

		if (ext == 'c') { /* .c to .i (if F_flag) or .k */
			init(call);
			append(call, PP);
			concat(call, &PP_FLAGS);
			append(call, file);

			if (F_flag) { /* to .i */
				f = mkstr(ifile, tmpdir, tmpname, ".i", 0);
				if (runvec(call, f)) {
					file = ifile;
					ext = 'i';
				}
				else {
					remove(ifile);
					continue;
				}
			}
			else {	/* use a pipe; to .k */
				init(call1);
				append(call1, CEM);
				concat(call1, &DEBUG_FLAGS);
				append(call1, V_FLAG);
				concat(call1, &CEM_FLAGS);
				append(call1, "-"); /* use stdin */
				f = mkstr(kfile, tmpdir, tmpname, ".k", 0);
				append(call1, f);
				if (runvec2(call, call1)) {
					file = kfile;
					ext = 'k';
				}
				else {
					remove(kfile);
					continue;
				}
			}
		}

		if (ext == 'i') { /* .i to .k	*/
			init(call);
			append(call, CEM);
			concat(call, &DEBUG_FLAGS);
			append(call, V_FLAG);
			concat(call, &CEM_FLAGS);
			append(call, file);
			f = mkstr(kfile, tmpdir, tmpname, ".k", 0);
			append(call, f);

			if (runvec(call, (char *)0)) {
				file = kfile;
				ext = 'k';
			}
			else {
				remove(kfile);
				continue;
			}
			cleanup(ifile);
		}

		/* .k to .m */
		if (ext == 'k') {
			init(call);
			append(call, OPT);
			concat(call, &OPT_FLAGS);
			append(call, file);
			f = mkstr(mfile, tmpdir, tmpname, ".m", 0);
			if (runvec(call, f) == 0)
				continue;
			file = mfile;
			ext = 'm';
			cleanup(kfile);
		}

		/* .m to .s */
		if (ext == 'm') {
			ldfile = S_flag ? ofile : alloc(strlen(BASE) + 3);

			init(call);
			append(call, CG);
			concat(call, &CG_FLAGS);
			append(call, file);
			f = mkstr(ldfile, BASE, ".s", 0);
			append(call, f);
			if (runvec(call, (char *)0) == 0)
				continue;
			cleanup(mfile);
			file = ldfile;
			ext = 's';
		}
	
		if (S_flag)
			continue;
		
		append(&LDFILES, file);
		if (ldfile) {
			append(&GEN_LDFILES, ldfile);
			ldfile = 0;
		}
	}

	/* *.s to a.out */
	if (RET_CODE == 0 && LDFILES.al_argc > 0) {
		init(call);
		append(call, ASLD);
		concat(call, &ASLD_FLAGS);
		append(call, "-o");
		append(call, o_FILE);
		concat(call, &LD_HEAD);
		concat(call, &LDFILES);
		concat(call, &LD_TAIL);
		if (runvec(call, (char *)0)) {
			register i = GEN_LDFILES.al_argc;

			while (i-- > 0)
				remove(GEN_LDFILES.al_argv[i]);

		}
	}
	return(RET_CODE);
}

#define BUFSIZE  (USTR_SIZE * MAXARGC)
char buf[BUFSIZE];
char *bufptr = &buf[0];

char *
alloc(u)
	unsigned u;
{
	register char *p = bufptr;

	if ((bufptr += u) >= &buf[BUFSIZE])
		panic("no space\n");
	return p;
}

append(al, arg)
	struct arglist *al;
	char *arg;
{
	if (al->al_argc >= MAXARGC)
		panic("argument list overflow\n");
	al->al_argv[(al->al_argc)++] = arg;
}

concat(al1, al2)
	struct arglist *al1, *al2;
{
	register i = al2->al_argc;
	register char **p = &(al1->al_argv[al1->al_argc]);
	register char **q = &(al2->al_argv[0]);

	if ((al1->al_argc += i) >= MAXARGC)
		panic("argument list overflow\n");
	while (i-- > 0)
		*p++ = *q++;
}

/*VARARGS1*/
char *
mkstr(dst, arg)
	char *dst, *arg;
{
	char **vec = (char **) &arg;
	register char *p;
	register char *q = dst;

	while (p = *vec++) {
		while (*q++ = *p++);
		q--;
	}
	return dst;
}

basename(str, dst)
	char *str;
	register char *dst;
{
	register char *p1 = str;
	register char *p2 = p1;

	while (*p1)
		if (*p1++ == '/')
			p2 = p1;
	p1--;
	if (*--p1 == '.')
		*p1 = '\0';
	while (*dst++ = *p2++);
	*p1 = '.';
}

int
extension(fn)
	register char *fn;
{
	char c;

	while (*fn++) ;
	fn--;
	c = *--fn;
	return (*--fn == '.') ? c : 0;
}

runvec(vec, outp)
	struct arglist *vec;
	char *outp;
{
	int pid, fd, status;

	if (v_flag) {
		pr_vec(vec);
		write(2, "\n", 1);
	}
	if ((pid = fork()) == 0) {	/* start up the process */
		if (outp) {	/* redirect standard output	*/
			close(1);
			if ((fd = creat(outp, 0666)) != 1)
				panic("cannot create output file\n");
		}
		ex_vec(vec);
	}
	if (pid == -1)
		panic("no more processes\n");
	wait(&status);
	return status ? ((RET_CODE = 1), 0) : 1;
}

runvec2(vec0, vec1)
	register struct arglist *vec0, *vec1;
{
	/* set up 'vec0 | vec1' */
	int pid, status1, status2, p[2];

	if (v_flag) {
		pr_vec(vec0);
		write(2, " | ", 3);
		pr_vec(vec1);
		write(2, "\n", 1);
	}
	if (pipe(p) == -1)
		panic("cannot create pipe\n");
	if ((pid = fork()) == 0) {
		close(1);
		if (dup(p[1]) != 1)
			panic("bad dup\n");
		close(p[0]);
		close(p[1]);
		ex_vec(vec0);
	}
	if (pid == -1)
		panic("no more processes\n");
	if ((pid = fork()) == 0) {
		close(0);
		if (dup(p[0]) != 0)
			panic("bad dup\n");
		close(p[0]);
		close(p[1]);
		ex_vec(vec1);
	}
	if (pid == -1)
		panic("no more processes\n");
	close(p[0]);
	close(p[1]);
	wait(&status1);
	wait(&status2);
	return (status1 || status2) ? ((RET_CODE = 1), 0) : 1;
}

/*VARARGS1*/
panic(str, argv)
	char *str;
	int argv;
{
	write(2, str, strlen(str));
	exit(1);
}

char *
cindex(s, c)
	char *s, c;
{
	while (*s)
		if (*s++ == c)
			return s - 1;
	return (char *) 0;
}

pr_vec(vec)
	register struct arglist *vec;
{
	register char **ap = &vec->al_argv[1];
	
	vec->al_argv[vec->al_argc] = 0;
	write(2, *ap, strlen(*ap) );
	while (*++ap) {
		write(2, " ", 1);
		write(2, *ap, strlen(*ap));
	}
}

ex_vec(vec)
	register struct arglist *vec;
{
	extern int errno;

#ifdef DEBUG
	if (noexec)
		exit(0);
#endif
	vec->al_argv[vec->al_argc] = 0;
	execv(vec->al_argv[1], &(vec->al_argv[1]));
	if (errno == ENOEXEC) { /* not an a.out, try it with the SHELL */
		vec->al_argv[0] = SHELL;
		execv(SHELL, &(vec->al_argv[0]));
	}
	if (access(vec->al_argv[1], 1) == 0) {
		/* File is executable. */
		write(2, "Cannot execute ", 15);
		write(2, vec->al_argv[1], strlen(vec->al_argv[1]));
		write(2, ". Not enough memory.\n", 21);
		write(2, "Try cc -F or use chmem to reduce its stack allocation\n",54);
	} else {
		write(2, vec->al_argv[1], strlen(vec->al_argv[1]));
		write(2, " is not executable\n", 19);
	}
	exit(1);
}

mktempname(nm)
	char nm[];
{
	register i;
	int pid = getpid();

	nm[0] = '/'; nm[1] = 'c'; nm[2] = 'e'; nm[3] = 'm';
	for (i = 9; i > 3; i--) {
		nm[i] = (pid % 10) + '0';
		pid /= 10;
	}
	nm[10] = '\0'; /* null termination */
}

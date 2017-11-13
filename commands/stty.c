/* stty - set terminal mode  	Author: Andy Tanenbaum */

#include "sgtty.h"
char *on[] = {"tabs",  "cbreak",  "raw",  "-nl",  "echo"};
char *off[]= {"-tabs", "", "", "nl", "-echo"};
int k;

struct sgttyb args;
struct tchars tch;

#define STARTC	 021		/* CTRL-Q */
#define STOPC	 023		/* CTRL-S */
#define QUITC	 034		/* CTRL-\ */
#define EOFC	 004		/* CTRL-D */
#define DELC	0177		/* DEL */

main(argc, argv)
int argc;
char *argv[];
{

  /* stty with no arguments just reports on current status. */
  ioctl(0, TIOCGETP, &args);
  ioctl(0, TIOCGETC, &tch);
  if (argc == 1) {
	report();
	exit(0);
  }

  /* Process the options specified. */
  k = 1;
  while (k < argc) {
	option(argv[k], argv[k+1]);
	k++;
  }
  ioctl(0, TIOCSETP, &args);
  ioctl(0, TIOCSETC, &tch);
  exit(0);
}



report()
{
  int mode;


  mode = args.sg_flags;
  pr(mode&XTABS, 0);
  pr(mode&CBREAK, 1);
  pr(mode&RAW, 2);
  pr(mode&CRMOD,3);
  pr(mode&ECHO,4);
  prints("\nkill = "); 	prctl(args.sg_kill);
  prints("\nerase = ");	prctl(args.sg_erase);
  prints("\nint = "); 	prctl(tch.t_intrc);
  prints("\nquit = "); 	prctl(tch.t_quitc);
  prints("\n");
}

pr(f, n)
int f,n;
{
  if (f)
	prints("%s ",on[n]);
  else
	prints("%s ",off[n]);
}

option(opt, next)
char *opt, *next;
{
  if (match(opt, "-tabs"))	{args.sg_flags &= ~XTABS; return;}
  if (match(opt, "-raw"))	{args.sg_flags &= ~RAW; return;}
  if (match(opt, "-cbreak"))	{args.sg_flags &= ~CBREAK; return;}
  if (match(opt, "-echo"))	{args.sg_flags &= ~ECHO; return;}
  if (match(opt, "-nl"))	{args.sg_flags |= CRMOD; return;}
  if (match(opt, "tabs"))	{args.sg_flags |= XTABS; return;}
  if (match(opt, "raw"))	{args.sg_flags |= RAW; return;}
  if (match(opt, "cbreak"))	{args.sg_flags |= CBREAK; return;}
  if (match(opt, "echo"))	{args.sg_flags |= ECHO; return;}
  if (match(opt, "nl"))		{args.sg_flags &= ~CRMOD; return;}
  if (match(opt, "kill"))	{args.sg_kill = *next; k++; return;}
  if (match(opt, "erase"))	{args.sg_erase = *next; k++; return;}
  if (match(opt, "int"))	{tch.t_intrc = *next; k++; return;}
  if (match(opt, "quit"))	{tch.t_quitc = *next; k++; return;}

  if (match(opt, "default"))	{
	args.sg_flags = ECHO | CRMOD | XTABS;
	args.sg_kill = '@';
	args.sg_erase = '\b';
  	tch.t_intrc = DELC;
  	tch.t_quitc = QUITC;
  	tch.t_startc = STARTC;
  	tch.t_stopc = STOPC;
  	tch.t_eofc = EOFC;
  	return;
  }
  	
  std_err("unknown mode: ");
  std_err(opt);
  std_err("\n");

}

int match(s1, s2)
char *s1, *s2;
{

  while (1) {
	if (*s1 == 0 && *s2 == 0) return(1);
	if (*s1 == 0 || *s2 == 0) return(0);
	if (*s1 != *s2) return(0);
	s1++;
	s2++;
  }
}

prctl(c)
char c;
{
  if (c < ' ')
	prints("^%c", 'A' + c - 1);
  else if (c == 0177)
	prints("DEL");
  else
	prints("%c", c);
}

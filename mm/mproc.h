/* This table has one slot per process.  It contains all the memory management
 * information for each process.  Among other things, it defines the text, data
 * and stack segments, uids and gids, and various flags.  The kernel and file
 * systems have tables that are also indexed by process, with the contents
 * of corresponding slots referring to the same process in all three.
 */

EXTERN struct mproc {
  struct mem_map mp_seg[NR_SEGS];	/* points to text, data, stack */
  char mp_exitstatus;		/* storage for status when process exits */
  char mp_sigstatus;		/* storage for signal # for killed processes */
  int mp_pid;			/* process id */
  int mp_parent;		/* index of parent process */
  int mp_procgrp;		/* process group (used for signals) */

  /* Real and effective uids and gids. */
  uid mp_realuid;		/* process' real uid */
  uid mp_effuid;		/* process' effective uid */
  gid mp_realgid;		/* process' real gid */
  gid mp_effgid;		/* process' effective gid */

  /* Bit maps for signals. */
  unshort mp_ignore;		/* 1 means ignore the signal, 0 means don't */
  unshort mp_catch;		/* 1 means catch the signal, 0 means don't */
  int (*mp_func)();		/* all signals vectored to a single user fcn */

  unsigned mp_flags;		/* flag bits */
} mproc[NR_PROCS];

/* Flag values */
#define IN_USE           001	/* set when 'mproc' slot in use */
#define WAITING          002	/* set by WAIT system call */
#define HANGING          004	/* set by EXIT system call */
#define PAUSED           010	/* set by PAUSE system call */
#define ALARM_ON         020	/* set when SIGALRM timer started */
#define SEPARATE	 040	/* set if file is separate I & D space */

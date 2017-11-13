/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/stat.h"
#include "const.h"
#include "type.h"
#include "dev.h"

#undef EXTERN
#define EXTERN

#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "super.h"

extern do_access(), do_chdir(), do_chmod(), do_chown(), do_chroot();
extern do_close(), do_creat(), do_dup(), do_exit(), do_fork(), do_fstat();
extern do_ioctl(), do_link(), do_lseek(), do_mknod(), do_mount(), do_open();
extern do_pipe(), do_read(), do_revive(), do_set(), do_stat(), do_stime();
extern do_sync(), do_time(), do_tims(), do_umask(), do_umount(), do_unlink();
extern do_unpause(), do_utime(), do_write(), no_call(), no_sys();

extern char fstack[];
char *stackpt = &fstack[FS_STACK_BYTES];	/* initial stack pointer */

int (*call_vector[NCALLS])() = {
	no_sys,		/*  0 = unused	*/
	do_exit,	/*  1 = exit	*/
	do_fork,	/*  2 = fork	*/
	do_read,	/*  3 = read	*/
	do_write,	/*  4 = write	*/
	do_open,	/*  5 = open	*/
	do_close,	/*  6 = close	*/
	no_sys,		/*  7 = wait	*/
	do_creat,	/*  8 = creat	*/
	do_link,	/*  9 = link	*/
	do_unlink,	/* 10 = unlink	*/
	no_sys,		/* 11 = exec	*/
	do_chdir,	/* 12 = chdir	*/
	do_time,	/* 13 = time	*/
	do_mknod,	/* 14 = mknod	*/
	do_chmod,	/* 15 = chmod	*/
	do_chown,	/* 16 = chown	*/
	no_sys,		/* 17 = break	*/
	do_stat,	/* 18 = stat	*/
	do_lseek,	/* 19 = lseek	*/
	no_sys,		/* 20 = getpid	*/
	do_mount,	/* 21 = mount	*/
	do_umount,	/* 22 = umount	*/
	do_set,		/* 23 = setuid	*/
	no_sys,		/* 24 = getuid	*/
	do_stime,	/* 25 = stime	*/
	no_sys,		/* 26 = (ptrace)*/
	no_sys,		/* 27 = alarm	*/
	do_fstat,	/* 28 = fstat	*/
	no_sys,		/* 29 = pause	*/
	do_utime,	/* 30 = utime	*/
	no_sys,		/* 31 = (stty)	*/
	no_sys,		/* 32 = (gtty)	*/
	do_access,	/* 33 = access	*/
	no_sys,		/* 34 = (nice)	*/
	no_sys,		/* 35 = (ftime)	*/
	do_sync,	/* 36 = sync	*/
	no_sys,		/* 37 = kill	*/
	no_sys,		/* 38 = unused	*/
	no_sys,		/* 39 = unused	*/
	no_sys,		/* 40 = unused	*/
	do_dup,		/* 41 = dup	*/
	do_pipe,	/* 42 = pipe	*/
	do_tims,	/* 43 = times	*/
	no_sys,		/* 44 = (prof)	*/
	no_sys,		/* 45 = unused	*/
	do_set,		/* 46 = setgid	*/
	no_sys,		/* 47 = getgid	*/
	no_sys,		/* 48 = sig	*/
	no_sys,		/* 49 = unused	*/
	no_sys,		/* 50 = unused	*/
	no_sys,		/* 51 = (acct)	*/
	no_sys,		/* 52 = (phys)	*/
	no_sys,		/* 53 = (lock)	*/
	do_ioctl,	/* 54 = ioctl	*/
	no_sys,		/* 55 = unused	*/
	no_sys,		/* 56 = (mpx)	*/
	no_sys,		/* 57 = unused	*/
	no_sys,		/* 58 = unused	*/
	no_sys,		/* 59 = exece	*/
	do_umask,	/* 60 = umask	*/
	do_chroot,	/* 61 = chroot	*/
	no_sys,		/* 62 = unused	*/
	no_sys,		/* 63 = unused	*/

	no_sys,		/* 64 = KSIG: signals originating in the kernel	*/
	do_unpause,	/* 65 = UNPAUSE	*/
	no_sys, 	/* 66 = BRK2 (used to tell MM size of FS,INIT)	*/
	do_revive,	/* 67 = REVIVE	*/
	no_sys		/* 68 = TASK_REPLY	*/
};


extern rw_dev(), rw_dev2();

/* The order of the entries here determines the mapping between major device
 * numbers and tasks.  The first entry (major device 0) is not used.  The
 * next entry is major device 1, etc.  Character and block devices can be
 * intermixed at random.  If this ordering is changed, BOOT_DEV and ROOT_DEV
 * must be changed to correspond to the new values.
 */
struct dmap dmap[] = {
/*  Open       Read/Write   Close       Task #      Device  File
    ----       ----------   -----       -------     ------  ----      */
    0,         0,           0,          0,           /* 0 = not used  */
    no_call,   rw_dev,      no_call,    MEM,         /* 1 = /dev/mem  */
    no_call,   rw_dev,      no_call,    FLOPPY,      /* 2 = /dev/fd0  */
    no_call,   rw_dev,      no_call,    WINCHESTER,  /* 3 = /dev/hd0  */
    no_call,   rw_dev,      no_call,    TTY,         /* 4 = /dev/tty0 */
    no_call,   rw_dev2,     no_call,    TTY,         /* 5 = /dev/tty  */
    no_call,   rw_dev,      no_call,    PRINTER      /* 6 = /dev/lp   */
};

int max_major = sizeof(dmap)/sizeof(struct dmap);

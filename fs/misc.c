/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_dup:	perform the DUP system call
 *   do_sync:	perform the SYNC system call
 *   do_fork:	adjust the tables after MM has performed a FORK system call
 *   do_exit:	a process has exited; note that in the tables
 *   do_set:	set uid or gid for some process
 *   do_revive:	revive a process that was waiting for something (e.g. TTY)
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "dev.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"
#include "super.h"

/*===========================================================================*
 *				do_dup					     *
 *===========================================================================*/
PUBLIC int do_dup()
{
/* Perform the dup(fd) or dup(fd,fd2) system call. */

  register int rfd;
  register struct fproc *rfp;
  struct filp *dummy;
  int r;
  extern struct filp *get_filp();

  /* Is the file descriptor valid? */
  rfd = fd & ~DUP_MASK;		/* kill off dup2 bit, if on */
  rfp = fp;
  if (get_filp(rfd) == NIL_FILP) return(err_code);

  /* Distinguish between dup and dup2. */
  if (fd == rfd) {			/* bit not on */
	/* dup(fd) */
	if ( (r = get_fd(0, &fd2, &dummy)) != OK) return(r);
  } else {
	/* dup2(fd, fd2) */
	if (fd2 < 0 || fd2 >= NR_FDS) return(EBADF);
	if (rfd == fd2) return(fd2);	/* ignore the call: dup2(x, x) */
	fd = fd2;		/* prepare to close fd2 */
	do_close();		/* cannot fail */
  }

  /* Success. Set up new file descriptors. */
  rfp->fp_filp[fd2] = rfp->fp_filp[rfd];
  rfp->fp_filp[fd2]->filp_count++;
  return(fd2);
}


/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
PUBLIC int do_sync()
{
/* Perform the sync() system call.  Flush all the tables. */

  register struct inode *rip;
  register struct buf *bp;
  register struct super_block *sp;
  dev_nr d;
  extern real_time clock_time();
  extern struct super_block *get_super();

  /* The order in which the various tables are flushed is critical.  The
   * blocks must be flushed last, since rw_inode() and rw_super() leave their
   * results in the block cache.
   */

  /* Update the time in the root super_block. */
  sp = get_super(ROOT_DEV);
  sp->s_time = clock_time();
  if (sp->s_rd_only == FALSE) sp->s_dirt = DIRTY;

  /* Write all the dirty inodes to the disk. */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if (rip->i_count > 0 && rip->i_dirt == DIRTY) rw_inode(rip, WRITING);

  /* Write all the dirty super_blocks to the disk. */
  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
	if (sp->s_dev != NO_DEV && sp->s_dirt == DIRTY) rw_super(sp, WRITING);

  /* Write all the dirty blocks to the disk. First do drive 0, then the rest.
   * This avoids starting drive 0, then starting drive 1, etc.
   */
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	d = bp->b_dev;
	if (d != NO_DEV && bp->b_dirt == DIRTY && ((d>>MINOR) & BYTE) == 0) 
		rw_block(bp, WRITING);
  }

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	d = bp->b_dev;
	if (d != NO_DEV && bp->b_dirt == DIRTY && ((d>>MINOR) & BYTE) != 0) 
		rw_block(bp, WRITING);
  }

  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork()
{
/* Perform those aspects of the fork() system call that relate to files.
 * In particular, let the child inherit its parents file descriptors.
 * The parent and child parameters tell who forked off whom. The file
 * system uses the same slot numbers as the kernel.  Only MM makes this call.
 */

  register struct fproc *cp;
  register char *sptr, *dptr;
  int i;

  /* Only MM may make this call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  /* Copy the parent's fproc struct to the child. */
  sptr = (char *) &fproc[parent];	/* pointer to parent's 'fproc' struct */
  dptr = (char *) &fproc[child];	/* pointer to child's 'fproc' struct */
  i = sizeof(struct fproc);		/* how many bytes to copy */
  while (i--) *dptr++ = *sptr++;	/* fproc[child] = fproc[parent] */

  /* Increase the counters in the 'filp' table. */
  cp = &fproc[child];
  for (i = 0; i < NR_FDS; i++)
	if (cp->fp_filp[i] != NIL_FILP) cp->fp_filp[i]->filp_count++;

  /* Record the fact that both root and working dir have another user. */
  dup_inode(cp->fp_rootdir);
  dup_inode(cp->fp_workdir);
  return(OK);
}


/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit()
{
/* Perform the file system portion of the exit(status) system call. */

  register int i, exitee;

  /* Only MM may do the EXIT call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  /* Nevertheless, pretend that the call came from the user. */
  fp = &fproc[slot1];		/* get_filp() needs 'fp' */
  exitee = slot1;

  /* Loop on file descriptors, closing any that are open. */
  for (i=0; i < NR_FDS; i++) {
	fd = i;
	do_close();
  }

  /* Release root and working directories. */
  put_inode(fp->fp_rootdir);
  put_inode(fp->fp_workdir);

  if (fp->fp_suspended == SUSPENDED) {
	if (fp->fp_task == XPIPE) susp_count--;
	pro = exitee;
	do_unpause();
	fp->fp_suspended = NOT_SUSPENDED;
  }
  return(OK);
}


/*===========================================================================*
 *				do_set					     *
 *===========================================================================*/
PUBLIC int do_set()
{
/* Set uid or gid field. */

  register struct fproc *tfp;

  /* Only MM may make this call directly. */
  if (who != MM_PROC_NR) return(ERROR);

  tfp = &fproc[slot1];
  if (fs_call == SETUID) {
	tfp->fp_realuid = (uid) real_user_id;
	tfp->fp_effuid =  (uid) eff_user_id;
  }
  if (fs_call == SETGID) {
	tfp->fp_effgid =  (gid) eff_grp_id;
	tfp->fp_realgid = (gid) real_grp_id;
  }
  return(OK);
}


/*===========================================================================*
 *				do_revive				     *
 *===========================================================================*/
PUBLIC int do_revive()
{
/* A task, typically TTY, has now gotten the characters that were needed for a
 * previous read.  The process did not get a reply when it made the call.
 * Instead it was suspended.  Now we can send the reply to wake it up.  This
 * business has to be done carefully, since the incoming message is from
 * a task (to which no reply can be sent), and the reply must go to a process
 * that blocked earlier.  The reply to the caller is inhibited by setting the
 * 'dont_reply' flag, and the reply to the blocked process is done explicitly
 * in revive().
 */

  if (who > 0) return(EPERM);
  revive(m.REP_PROC_NR, m.REP_STATUS);
  dont_reply = TRUE;		/* don't reply to the TTY task */
  return(OK);
}

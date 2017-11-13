/* This file deals with the suspension and revival of processes.  A process can
 * be suspended because it wants to read or write from a pipe and can't, or
 * because it wants to read or write from a special file and can't.  When a
 * process can't continue it is suspended, and revived later when it is able
 * to continue.
 *
 * The entry points into this file are
 *   do_pipe:	  perform the PIPE system call
 *   pipe_check:  check to see that a read or write on a pipe is feasible now
 *   suspend:	  suspend a process that cannot do a requested read or write
 *   release:	  check to see if a suspended process can be released and do it
 *   revive:	  mark a suspended process as able to run again
 *   do_unpause:  a signal has been sent to a process; see if it suspended
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "../h/signal.h"
#include "const.h"
#include "type.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"

PRIVATE message mess;

/*===========================================================================*
 *				do_pipe					     *
 *===========================================================================*/
PUBLIC int do_pipe()
{
/* Perform the pipe(fil_des) system call. */

  register struct fproc *rfp;
  register struct inode *rip;
  int r;
  dev_nr device;
  struct filp *fil_ptr0, *fil_ptr1;
  int fil_des[2];		/* reply goes here */
  extern struct inode *alloc_inode();

  /* Acquire two file descriptors. */
  rfp = fp;
  if ( (r = get_fd(R_BIT, &fil_des[0], &fil_ptr0)) != OK) return(r);
  rfp->fp_filp[fil_des[0]] = fil_ptr0;
  fil_ptr0->filp_count = 1;
  if ( (r = get_fd(W_BIT, &fil_des[1], &fil_ptr1)) != OK) {
	rfp->fp_filp[fil_des[0]] = NIL_FILP;
	fil_ptr0->filp_count = 0;
	return(r);
  }
  rfp->fp_filp[fil_des[1]] = fil_ptr1;
  fil_ptr1->filp_count = 1;

  /* Make the inode in the current working directory. */
  device = rfp->fp_workdir->i_dev;	/* inode dev is same as working dir */
  if ( (rip = alloc_inode(device, I_REGULAR)) == NIL_INODE) {
	rfp->fp_filp[fil_des[0]] = NIL_FILP;
	fil_ptr0->filp_count = 0;
	rfp->fp_filp[fil_des[1]] = NIL_FILP;
	fil_ptr1->filp_count = 0;
	return(err_code);
  }

  rip->i_pipe = I_PIPE;
  fil_ptr0->filp_ino = rip;
  dup_inode(rip);		/* for double usage */
  fil_ptr1->filp_ino = rip;
  rw_inode(rip, WRITING);	/* mark inode as allocated */
  reply_i1 = fil_des[0];
  reply_i2 = fil_des[1];
  return(OK);
}


/*===========================================================================*
 *				pipe_check				     *
 *===========================================================================*/
PUBLIC int pipe_check(rip, rw_flag, virgin, bytes, position)
register struct inode *rip;	/* the inode of the pipe */
int rw_flag;			/* READING or WRITING */
int virgin;			/* 1 if no data transferred yet, else 0 */
register int bytes;		/* bytes to be read or written (all chunks) */
register file_pos *position;	/* pointer to current file position */
{
/* Pipes are a little different.  If a process reads from an empty pipe for
 * which a writer still exists, suspend the reader.  If the pipe is empty
 * and there is no writer, return 0 bytes.  If a process is writing to a
 * pipe and no one is reading from it, give a broken pipe error.
 */

  extern struct filp *find_filp();

  /* If reading, check for empty pipe. */
  if (rw_flag == READING) {
	if (*position >= rip->i_size) {
		/* Process is reading from an empty pipe. */
		if (find_filp(rip, W_BIT) != NIL_FILP) {
			/* Writer exists; suspend rdr if no data already read.*/
			if (virgin) suspend(XPIPE);	/* block reader */

			/* If need be, activate sleeping writer. */
			if (susp_count > 0) release(rip, WRITE, 1);
		}
		return(0);
	}
  } else {
	/* Process is writing to a pipe. */
	if (bytes > PIPE_SIZE) return(EFBIG);
	if (find_filp(rip, R_BIT) == NIL_FILP) {
		/* Tell MM to generate a SIGPIPE signal. */
		mess.m_type = KSIG;
		mess.PROC1 = fp - fproc;
		mess.SIG_MAP = 1 << (SIGPIPE - 1);
		send(MM_PROC_NR, &mess);
		return(EPIPE);
	}

	if (*position + bytes > PIPE_SIZE) {
		suspend(XPIPE);	/* stop writer -- pipe full */
		return(0);
	}

	/* Writing to an empty pipe.  Search for suspended reader. */
	if (*position == 0) release(rip, READ, 1);
  }

  return(1);
}


/*===========================================================================*
 *				suspend					     *
 *===========================================================================*/
PUBLIC suspend(task)
int task;			/* who is proc waiting for? (PIPE = pipe) */
{
/* Take measures to suspend the processing of the present system call.
 * Store the parameters to be used upon resuming in the process table.
 * (Actually they are not used when a process is waiting for an I/O device,
 * but they are needed for pipes, and it is not worth making the distinction.)
 */

  if (task == XPIPE) susp_count++;	/* count procs suspended on pipe */
  fp->fp_suspended = SUSPENDED;
  fp->fp_fd = fd << 8 | fs_call;
  fp->fp_buffer = buffer;
  fp->fp_nbytes = nbytes;
  fp->fp_task = -task;
  dont_reply = TRUE;		/* do not send caller a reply message now */
}


/*===========================================================================*
 *				release					     *
 *===========================================================================*/
PUBLIC release(ip, call_nr, count)
register struct inode *ip;	/* inode of pipe */
int call_nr;			/* READ or WRITE */
int count;			/* max number of processes to release */
{
/* Check to see if any process is hanging on the pipe whose inode is in 'ip'.
 * If one is, and it was trying to perform the call indicated by 'call_nr'
 * (READ or WRITE), release it.
 */

  register struct fproc *rp;

  /* Search the proc table. */
  for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) {
	if (rp->fp_suspended == SUSPENDED && (rp->fp_fd & BYTE) == call_nr &&
				rp->fp_filp[rp->fp_fd>>8]->filp_ino == ip) {
		revive(rp - fproc, 0);
		susp_count--;	/* keep track of who is suspended */
		if (--count == 0) return;
	}
  }
}


/*===========================================================================*
 *				revive					     *
 *===========================================================================*/
PUBLIC revive(proc_nr, bytes)
int proc_nr;			/* process to revive */
int bytes;			/* if hanging on task, how many bytes read */
{
/* Revive a previously blocked process. When a process hangs on tty, this
 * is the way it is eventually released.
 */

  register struct fproc *rfp;

  if (proc_nr < 0 || proc_nr >= NR_PROCS) panic("revive err", proc_nr);
  rfp = &fproc[proc_nr];
  if (rfp->fp_suspended == NOT_SUSPENDED) return;

  /* The 'reviving' flag only applies to pipes.  Processes waiting for TTY get
   * a message right away.  The revival process is different for TTY and pipes.
   * For TTY revival, the work is already done, for pipes it is not: the proc
   * must be restarted so it can try again.
   */
  if (rfp->fp_task == XPIPE) {
	/* Revive a process suspended on a pipe. */
	rfp->fp_revived = REVIVING;
	reviving++;		/* process was waiting on pipe */
  } else {
	/* Revive a process suspended on TTY or other device. */
	rfp->fp_suspended = NOT_SUSPENDED;
	rfp->fp_nbytes = bytes;	/* pretend it only wants what there is */
	reply(proc_nr, bytes);	/* unblock the process */
  }
}


/*===========================================================================*
 *				do_unpause				     *
 *===========================================================================*/
PUBLIC int do_unpause()
{
/* A signal has been sent to a user who is paused on the file system.
 * Abort the system call with the EINTR error message.
 */

  register struct fproc *rfp;
  int proc_nr, task;
  struct filp *f;
  dev_nr dev;
  extern struct filp *get_filp();

  if (who > MM_PROC_NR) return(EPERM);
  proc_nr = pro;
  if (proc_nr < 0 || proc_nr >= NR_PROCS) panic("unpause err 1", proc_nr);
  rfp = &fproc[proc_nr];
  if (rfp->fp_suspended == NOT_SUSPENDED) return(OK);
  task = -rfp->fp_task;

  if (task != XPIPE) {
	f = get_filp(rfp->fp_fd);
	dev = f->filp_ino->i_zone[0];	/* device on which proc is hanging */
	mess.TTY_LINE = (dev >> MINOR) & BYTE;
	mess.PROC_NR = proc_nr;
	mess.m_type = CANCEL;
	if (sendrec(task, &mess) != OK) panic("unpause err 2", NO_NUM);
	while (mess.REP_PROC_NR != proc_nr) {
		revive(mess.REP_PROC_NR, mess.REP_STATUS);
		if (receive(task, &m) != OK) panic("unpause err 3", NO_NUM);
	}
	revive(proc_nr, EINTR);	/* signal interrupted call */
  }

  return(OK);
}

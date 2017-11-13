/* This file contains essentially all of the process and message handling.
 * It has two main entry points from the outside:
 *
 *   sys_call:   called when a process or task does SEND, RECEIVE or SENDREC
 *   interrupt:	called by interrupt routines to send a message to task
 *
 * It also has five minor entry points:
 *
 *   ready:	put a process on one of the ready queues so it can be run
 *   unready:	remove a process from the ready queues
 *   sched:	a process has run too long; schedule another one
 *   mini_send:	send a message (used by interrupt signals, etc.)
 *   pick_proc:	pick a process to run (used by system initialization)
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"

/*===========================================================================*
 *				interrupt				     * 
 *===========================================================================*/
PUBLIC interrupt(task, m_ptr)
int task;			/* number of task to be started */
message *m_ptr;			/* interrupt message to send to the task */
{
/* An interrupt has occurred.  Schedule the task that handles it. */

  int i, n, old_map, this_bit;

#ifdef i8088
  /* Re-enable the 8259A interrupt controller. */
  port_out(INT_CTL, ENABLE);	/* this re-enables the 8259A controller chip */
  if (pc_at) port_out(INT2_CTL, ENABLE);	/* re-enable second 8259A */
#endif

  /* Try to send the interrupt message to the indicated task. */
  this_bit = 1 << (-task);
  if (mini_send(HARDWARE, task, m_ptr) != OK) {
	/* The message could not be sent to the task; it was not waiting. */
	old_map = busy_map;	/* save original map of busy tasks */
	if (task == CLOCK) {
		lost_ticks++;
	} else {
		busy_map |= this_bit;		/* mark task as busy */
		task_mess[-task] = m_ptr;	/* record message pointer */
	}
  } else {
	/* Hardware interrupt was successfully sent as a message. */
	busy_map &= ~this_bit;	/* turn off the bit in case it was on */
	old_map = busy_map;
  }

  /* See if any tasks that were previously busy are now listening for msgs. */
  if (old_map != 0) {
	for (i = 2; i <= NR_TASKS; i++) {
		/* Check each task looking for one with a pending interrupt. */
		if ( (old_map>>i) & 1) {
			/* Task 'i' has a pending interrupt. */
			n = mini_send(HARDWARE, -i, task_mess[i]);
			if (n == OK) busy_map &= ~(1 << i);
		}
	}
  }

  /* If a task has just been readied and a user is running, run the task. */
  if (rdy_head[TASK_Q] != NIL_PROC && (cur_proc >= 0 || cur_proc == IDLE))
	pick_proc();
}


/*===========================================================================*
 *				sys_call				     * 
 *===========================================================================*/
PUBLIC sys_call(function, caller, src_dest, m_ptr)
int function;			/* SEND, RECEIVE, or BOTH */
int caller;			/* who is making this call */
int src_dest;			/* source to receive from or dest to send to */
message *m_ptr;			/* pointer to message */
{
/* The only system calls that exist in MINIX are sending and receiving
 * messages.  These are done by trapping to the kernel with an INT instruction.
 * The trap is caught and sys_call() is called to send or receive a message (or
 * both).
 */

  register struct proc *rp;
  int n;

  /* Check for bad system call parameters. */
  rp = proc_addr(caller);
  if (src_dest < -NR_TASKS || (src_dest >= NR_PROCS && src_dest != ANY) ) {
	rp->p_reg[RET_REG] = E_BAD_SRC;
	return;
  }
  if (function != BOTH && caller >= LOW_USER) {
	rp->p_reg[RET_REG] = E_NO_PERM;	/* users only do BOTH */
	return;
  }

  /* The parameters are ok. Do the call. */
  if (function & SEND) {
	n = mini_send(caller, src_dest, m_ptr);	/* func = SEND or BOTH */
	if (function == SEND || n != OK) rp->p_reg[RET_REG] = n;
	if (n != OK) return;	/* SEND failed */
  }

  if (function & RECEIVE) {
	n = mini_rec(caller, src_dest, m_ptr);      /* func = RECEIVE or BOTH */
	rp->p_reg[RET_REG] = n;
  }
}

/*===========================================================================*
 *				mini_send				     * 
 *===========================================================================*/
PUBLIC int mini_send(caller, dest, m_ptr)
int caller;			/* who is trying to send a message? */
int dest;			/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
{
/* Send a message from 'caller' to 'dest'.  If 'dest' is blocked waiting for
 * this message, copy the message to it and unblock 'dest'.  If 'dest' is not
 * waiting at all, or is waiting for another source, queue 'caller'.
 */

  register struct proc *caller_ptr, *dest_ptr, *next_ptr;
  vir_bytes vb;			/* message buffer pointer as vir_bytes */
  vir_clicks vlo, vhi;		/* virtual clicks containing message to send */
  vir_clicks len;		/* length of data segment in clicks */

  /* User processes are only allowed to send to FS and MM.  Check for this. */
  if (caller >= LOW_USER && (dest != FS_PROC_NR && dest != MM_PROC_NR))
	return(E_BAD_DEST);
  caller_ptr = proc_addr(caller);	/* pointer to source's proc entry */
  dest_ptr = proc_addr(dest);	/* pointer to destination's proc entry */
  if (dest_ptr->p_flags & P_SLOT_FREE) return(E_BAD_DEST);	/* dead dest */

  /* Check for messages wrapping around top of memory or outside data seg. */
  len = caller_ptr->p_map[D].mem_len;
  vb = (vir_bytes) m_ptr;
  vlo = vb >> CLICK_SHIFT;	/* vir click for bottom of message */
  vhi = (vb + MESS_SIZE - 1) >> CLICK_SHIFT;	/* vir click for top of message */
  if (vhi < vlo || vhi - caller_ptr->p_map[D].mem_vir >= len)return(E_BAD_ADDR);

  /* Check to see if 'dest' is blocked waiting for this message. */
  if ( (dest_ptr->p_flags & RECEIVING) &&
		(dest_ptr->p_getfrom == ANY || dest_ptr->p_getfrom == caller) ) {
	/* Destination is indeed waiting for this message. */
	cp_mess(caller, caller_ptr->p_map[D].mem_phys, m_ptr, 
				dest_ptr->p_map[D].mem_phys, dest_ptr->p_messbuf);
	dest_ptr->p_flags &= ~RECEIVING;	/* deblock destination */
	if (dest_ptr->p_flags == 0) ready(dest_ptr);
  } else {
	/* Destination is not waiting.  Block and queue caller. */
	if (caller == HARDWARE) return(E_OVERRUN);
	caller_ptr->p_messbuf = m_ptr;
	caller_ptr->p_flags |= SENDING;
	unready(caller_ptr);

	/* Process is now blocked.  Put in on the destination's queue. */
	if ( (next_ptr = dest_ptr->p_callerq) == NIL_PROC) {
		dest_ptr->p_callerq = caller_ptr;
	} else {
		while (next_ptr->p_sendlink != NIL_PROC)
			next_ptr = next_ptr->p_sendlink;
		next_ptr->p_sendlink = caller_ptr;
	}
	caller_ptr->p_sendlink = NIL_PROC;
  }
  return(OK);
}


/*===========================================================================*
 *				mini_rec				     * 
 *===========================================================================*/
PRIVATE int mini_rec(caller, src, m_ptr)
int caller;			/* process trying to get message */
int src;			/* which message source is wanted (or ANY) */
message *m_ptr;			/* pointer to message buffer */
{
/* A process or task wants to get a message.  If one is already queued,
 * acquire it and deblock the sender.  If no message from the desired source
 * is available, block the caller.  No need to check parameters for validity.
 * Users calls are always sendrec(), and mini_send() has checked already.  
 * Calls from the tasks, MM, and FS are trusted.
 */

  register struct proc *caller_ptr, *sender_ptr, *prev_ptr;
  int sender;

  caller_ptr = proc_addr(caller);	/* pointer to caller's proc structure */

  /* Check to see if a message from desired source is already available. */
  sender_ptr = caller_ptr->p_callerq;
  while (sender_ptr != NIL_PROC) {
	sender = sender_ptr - proc - NR_TASKS;
	if (src == ANY || src == sender) {
		/* An acceptable message has been found. */
		cp_mess(sender, sender_ptr->p_map[D].mem_phys, sender_ptr->p_messbuf,
					caller_ptr->p_map[D].mem_phys, m_ptr);
		sender_ptr->p_flags &= ~SENDING;	/* deblock sender */
		if (sender_ptr->p_flags == 0) ready(sender_ptr);
		if (sender_ptr == caller_ptr->p_callerq)
			caller_ptr->p_callerq = sender_ptr->p_sendlink;
		else
			prev_ptr->p_sendlink = sender_ptr->p_sendlink;
		return(OK);
	}
	prev_ptr = sender_ptr;
	sender_ptr = sender_ptr->p_sendlink;
  }

  /* No suitable message is available.  Block the process trying to receive. */
  caller_ptr->p_getfrom = src;
  caller_ptr->p_messbuf = m_ptr;
  caller_ptr->p_flags |= RECEIVING;
  unready(caller_ptr);

  /* If MM has just blocked and there are kernel signals pending, now is the
   * time to tell MM about them, since it will be able to accept the message.
   */
  if (sig_procs > 0 && caller == MM_PROC_NR && src == ANY) inform(MM_PROC_NR);
  return(OK);
}


/*===========================================================================*
 *				pick_proc				     * 
 *===========================================================================*/
PUBLIC pick_proc()
{
/* Decide who to run now. */

  register int q;		/* which queue to use */

  if (rdy_head[TASK_Q] != NIL_PROC) q = TASK_Q;
  else if (rdy_head[SERVER_Q] != NIL_PROC) q = SERVER_Q;
  else q = USER_Q;

  /* Set 'cur_proc' and 'proc_ptr'. If system is idle, set 'cur_proc' to a
   * special value (IDLE), and set 'proc_ptr' to point to an unused proc table
   * slot, namely, that of task -1 (HARDWARE), so save() will have somewhere to
   * deposit the registers when a interrupt occurs on an idle machine.
   * Record previous process so that when clock tick happens, the clock task
   * can find out who was running just before it began to run.  (While the
   * clock task is running, 'cur_proc' = CLOCKTASK. In addition, set 'bill_ptr'
   * to always point to the process to be billed for CPU time.
   */
  prev_proc = cur_proc;
  if (rdy_head[q] != NIL_PROC) {
	/* Someone is runnable. */
	cur_proc = rdy_head[q] - proc - NR_TASKS;
	proc_ptr = rdy_head[q];
	if (cur_proc >= LOW_USER) bill_ptr = proc_ptr;
  } else {
	/* No one is runnable. */
	cur_proc = IDLE;
	proc_ptr = proc_addr(HARDWARE);
	bill_ptr = proc_ptr;
  }
}

/*===========================================================================*
 *				ready					     * 
 *===========================================================================*/
PUBLIC ready(rp)
register struct proc *rp;	/* this process is now runnable */
{
/* Add 'rp' to the end of one of the queues of runnable processes. Three
 * queues are maintained:
 *   TASK_Q   - (highest priority) for runnable tasks
 *   SERVER_Q - (middle priority) for MM and FS only
 *   USER_Q   - (lowest priority) for user processes
 */

  register int q;		/* TASK_Q, SERVER_Q, or USER_Q */
  int r;

  lock();			/* disable interrupts */
  r = (rp - proc) - NR_TASKS;	/* task or proc number */
  q = (r < 0 ? TASK_Q : r < LOW_USER ? SERVER_Q : USER_Q);

  /* See if the relevant queue is empty. */
  if (rdy_head[q] == NIL_PROC)
	rdy_head[q] = rp;	/* add to empty queue */
  else
	rdy_tail[q]->p_nextready = rp;	/* add to tail of nonempty queue */
  rdy_tail[q] = rp;		/* new entry has no successor */
  rp->p_nextready = NIL_PROC;
  restore();			/* restore interrupts to previous state */
}


/*===========================================================================*
 *				unready					     * 
 *===========================================================================*/
PUBLIC unready(rp)
register struct proc *rp;	/* this process is no longer runnable */
{
/* A process has blocked. */

  register struct proc *xp;
  int r, q;

  lock();			/* disable interrupts */
  r = rp - proc - NR_TASKS;
  q = (r < 0 ? TASK_Q : r < LOW_USER ? SERVER_Q : USER_Q);
  if ( (xp = rdy_head[q]) == NIL_PROC) return;
  if (xp == rp) {
	/* Remove head of queue */
	rdy_head[q] = xp->p_nextready;
	pick_proc();
  } else {
	/* Search body of queue.  A process can be made unready even if it is
	 * not running by being sent a signal that kills it.
	 */
	while (xp->p_nextready != rp)
		if ( (xp = xp->p_nextready) == NIL_PROC) return;
	xp->p_nextready = xp->p_nextready->p_nextready;
	while (xp->p_nextready != NIL_PROC) xp = xp->p_nextready;
	rdy_tail[q] = xp;
  }
  restore();			/* restore interrupts to previous state */
}


/*===========================================================================*
 *				sched					     * 
 *===========================================================================*/
PUBLIC sched()
{
/* The current process has run too long.  If another low priority (user)
 * process is runnable, put the current process on the end of the user queue,
 * possibly promoting another user to head of the queue.
 */

  lock();			/* disable interrupts */
  if (rdy_head[USER_Q] == NIL_PROC) {
	restore();		/* restore interrupts to previous state */
	return;
  }

  /* One or more user processes queued. */
  rdy_tail[USER_Q]->p_nextready = rdy_head[USER_Q];
  rdy_tail[USER_Q] = rdy_head[USER_Q];
  rdy_head[USER_Q] = rdy_head[USER_Q]->p_nextready;
  rdy_tail[USER_Q]->p_nextready = NIL_PROC;
  pick_proc();
  restore();			/* restore interrupts to previous state */
}

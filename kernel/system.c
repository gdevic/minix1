/* This task handles the interface between file system and kernel as well as
 * between memory manager and kernel.  System services are obtained by sending
 * sys_task() a message specifying what is needed.  To make life easier for
 * MM and FS, a library is provided with routines whose names are of the
 * form sys_xxx, e.g. sys_xit sends the SYS_XIT message to sys_task.  The
 * message types and parameters are:
 *
 *   SYS_FORK	 informs kernel that a process has forked
 *   SYS_NEWMAP	 allows MM to set up a process memory map
 *   SYS_EXEC	 sets program counter and stack pointer after EXEC
 *   SYS_XIT	 informs kernel that a process has exited
 *   SYS_GETSP	 caller wants to read out some process' stack pointer
 *   SYS_TIMES	 caller wants to get accounting times for a process
 *   SYS_ABORT	 MM or FS cannot go on; abort MINIX
 *   SYS_SIG	 send a signal to a process
 *   SYS_COPY	 requests a block of data to be copied between processes
 *
 * Message type m1 is used for all except SYS_SIG and SYS_COPY, both of
 * which need special parameter types.
 *
 *    m_type       PROC1     PROC2      PID     MEM_PTR   
 * ------------------------------------------------------
 * | SYS_FORK   | parent  |  child  |   pid   |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_NEWMAP | proc nr |         |         | map ptr |
 * |------------+---------+---------+---------+---------|
 * | SYS_EXEC   | proc nr |         | new sp  |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_XIT    | parent  | exitee  |         |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_GETSP  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_TIMES  | proc nr |         | buf ptr |         |
 * |------------+---------+---------+---------+---------|
 * | SYS_ABORT  |         |         |         |         |
 * ------------------------------------------------------
 *
 *
 *    m_type       m6_i1     m6_i2     m6_i3     m6_f1     
 * ------------------------------------------------------
 * | SYS_SIG    | proc_nr  |  sig    |         | handler |
 * ------------------------------------------------------
 *
 *
 *    m_type      m5_c1   m5_i1    m5_l1   m5_c2   m5_i2    m5_l2   m5_l3
 * --------------------------------------------------------------------------
 * | SYS_COPY   |src seg|src proc|src vir|dst seg|dst proc|dst vir| byte ct |
 * --------------------------------------------------------------------------
 *
 * In addition to the main sys_task() entry point, there are three other minor
 * entry points:
 *   cause_sig:	take action to cause a signal to occur, sooner or later
 *   inform:	tell MM about pending signals
 *   umap:	compute the physical address for a given virtual address
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "../h/signal.h"
#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"

#define COPY_UNIT     65534L	/* max bytes to copy at once */

extern phys_bytes umap();

PRIVATE message m;
PRIVATE char sig_stuff[SIG_PUSH_BYTES];	/* used to send signals to processes */

/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
PUBLIC sys_task()
{
/* Main entry point of sys_task.  Get the message and dispatch on type. */

  register int r;

  while (TRUE) {
	receive(ANY, &m);

	switch (m.m_type) {	/* which system call */
	    case SYS_FORK:	r = do_fork(&m);	break;
	    case SYS_NEWMAP:	r = do_newmap(&m);	break;
	    case SYS_EXEC:	r = do_exec(&m);	break;
	    case SYS_XIT:	r = do_xit(&m);		break;
	    case SYS_GETSP:	r = do_getsp(&m);	break;
	    case SYS_TIMES:	r = do_times(&m);	break;
	    case SYS_ABORT:	r = do_abort(&m);	break;
	    case SYS_SIG:	r = do_sig(&m);		break;
	    case SYS_COPY:	r = do_copy(&m);	break;
	    default:		r = E_BAD_FCN;
	}

	m.m_type = r;		/* 'r' reports status of call */
	send(m.m_source, &m);	/* send reply to caller */
  }
}


/*===========================================================================*
 *				do_fork					     * 
 *===========================================================================*/
PRIVATE int do_fork(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_fork().  'k1' has forked.  The child is 'k2'. */

  register struct proc *rpc;
  register char *sptr, *dptr;	/* pointers for copying proc struct */
  int k1;			/* number of parent process */
  int k2;			/* number of child process */
  int pid;			/* process id of child */
  int bytes;			/* counter for copying proc struct */

  k1 = m_ptr->PROC1;		/* extract parent slot number from msg */
  k2 = m_ptr->PROC2;		/* extract child slot number */
  pid = m_ptr->PID;		/* extract child process id */

  if (k1 < 0 || k1 >= NR_PROCS || k2 < 0 || k2 >= NR_PROCS)return(E_BAD_PROC);
  rpc = proc_addr(k2);

  /* Copy parent 'proc' struct to child. */
  sptr = (char *) proc_addr(k1);	/* parent pointer */
  dptr = (char *) proc_addr(k2);	/* child pointer */
  bytes = sizeof(struct proc);		/* # bytes to copy */
  while (bytes--) *dptr++ = *sptr++;	/* copy parent struct to child */

  rpc->p_flags |= NO_MAP;	/* inhibit the process from running */
  rpc->p_pid = pid;		/* install child's pid */
  rpc->p_reg[RET_REG] = 0;	/* child sees pid = 0 to know it is child */

  rpc->user_time = 0;		/* set all the accounting times to 0 */
  rpc->sys_time = 0;
  rpc->child_utime = 0;
  rpc->child_stime = 0;
  return(OK);
}


/*===========================================================================*
 *				do_newmap				     * 
 *===========================================================================*/
PRIVATE int do_newmap(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_newmap().  Fetch the memory map from MM. */

  register struct proc *rp, *rsrc;
  phys_bytes src_phys, dst_phys, pn;
  vir_bytes vmm, vsys, vn;
  int caller;			/* whose space has the new map (usually MM) */
  int k;			/* process whose map is to be loaded */
  int old_flags;		/* value of flags before modification */
  struct mem_map *map_ptr;	/* virtual address of map inside caller (MM) */

  /* Extract message parameters and copy new memory map from MM. */
  caller = m_ptr->m_source;
  k = m_ptr->PROC1;
  map_ptr = (struct mem_map *) m_ptr->MEM_PTR;
  if (k < -NR_TASKS || k >= NR_PROCS) return(E_BAD_PROC);
  rp = proc_addr(k);		/* ptr to entry of user getting new map */
  rsrc = proc_addr(caller);	/* ptr to MM's proc entry */
  vn = NR_SEGS * sizeof(struct mem_map);
  pn = vn;
  vmm = (vir_bytes) map_ptr;	/* careful about sign extension */
  vsys = (vir_bytes) rp->p_map;	/* again, careful about sign extension */
  if ( (src_phys = umap(rsrc, D, vmm, vn)) == 0)
	panic("bad call to sys_newmap (src)", NO_NUM);
  if ( (dst_phys = umap(proc_addr(SYSTASK), D, vsys, vn)) == 0)
	panic("bad call to sys_newmap (dst)", NO_NUM);
  phys_copy(src_phys, dst_phys, pn);

#ifdef i8088
  /* On 8088, set segment registers. */
  rp->p_reg[CS_REG] = rp->p_map[T].mem_phys;	/* set cs */
  rp->p_reg[DS_REG] = rp->p_map[D].mem_phys;	/* set ds */
  rp->p_reg[SS_REG] = rp->p_map[D].mem_phys;	/* set ss */
  rp->p_reg[ES_REG] = rp->p_map[D].mem_phys;	/* set es */
#endif

  old_flags = rp->p_flags;	/* save the previous value of the flags */
  rp->p_flags &= ~NO_MAP;
  if (old_flags != 0 && rp->p_flags == 0) ready(rp);
  return(OK);
}


/*===========================================================================*
 *				do_exec					     * 
 *===========================================================================*/
PRIVATE int do_exec(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exec().  A process has done a successful EXEC. Patch it up. */

  register struct proc *rp;
  int k;			/* which process */
  int *sp;			/* new sp */

  k = m_ptr->PROC1;		/* 'k' tells which process did EXEC */
  sp = (int *) m_ptr->STACK_PTR;
  if (k < 0 || k >= NR_PROCS) return(E_BAD_PROC);
  rp = proc_addr(k);
  rp->p_sp = sp;		/* set the stack pointer */
  rp->p_pcpsw.pc = (int (*)()) 0;	/* reset pc */
  rp->p_alarm = 0;		/* reset alarm timer */
  rp->p_flags &= ~RECEIVING;	/* MM does not reply to EXEC call */
  if (rp->p_flags == 0) ready(rp);
  set_name(k, sp);		/* save command string for F1 display */
  return(OK);
}


/*===========================================================================*
 *				do_xit					     * 
 *===========================================================================*/
PRIVATE int do_xit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_xit().  A process has exited. */

  register struct proc *rp, *rc;
  struct proc *np, *xp;
  int parent;			/* number of exiting proc's parent */
  int proc_nr;			/* number of process doing the exit */

  parent = m_ptr->PROC1;	/* slot number of parent process */
  proc_nr = m_ptr->PROC2;	/* slot number of exiting process */
  if (parent < 0 || parent >= NR_PROCS || proc_nr < 0 || proc_nr >= NR_PROCS)
	return(E_BAD_PROC);
  rp = proc_addr(parent);
  rc = proc_addr(proc_nr);
  rp->child_utime += rc->user_time + rc->child_utime;	/* accum child times */
  rp->child_stime += rc->sys_time + rc->child_stime;
  unready(rc);
  rc->p_alarm = 0;		/* turn off alarm timer */
  set_name(proc_nr, (char *) 0);	/* disable command printing for F1 */

  /* If the process being terminated happens to be queued trying to send a
   * message (i.e., the process was killed by a signal, rather than it doing an
   * EXIT), then it must be removed from the message queues.
   */
  if (rc->p_flags & SENDING) {
	/* Check all proc slots to see if the exiting process is queued. */
	for (rp = &proc[0]; rp < &proc[NR_TASKS + NR_PROCS]; rp++) {
		if (rp->p_callerq == NIL_PROC) continue;
		if (rp->p_callerq == rc) {
			/* Exiting process is on front of this queue. */
			rp->p_callerq = rc->p_sendlink;
			break;
		} else {
			/* See if exiting process is in middle of queue. */
			np = rp->p_callerq;
			while ( ( xp = np->p_sendlink) != NIL_PROC)
				if (xp == rc) {
					np->p_sendlink = xp->p_sendlink;
					break;
				} else {
					np = xp;
				}
		}
	}
  }
  rc->p_flags = P_SLOT_FREE;
  return(OK);
}


/*===========================================================================*
 *				do_getsp				     * 
 *===========================================================================*/
PRIVATE int do_getsp(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_getsp().  MM wants to know what sp is. */

  register struct proc *rp;
  int k;				/* whose stack pointer is wanted? */

  k = m_ptr->PROC1;
  if (k < 0 || k >= NR_PROCS) return(E_BAD_PROC);
  rp = proc_addr(k);
  m.STACK_PTR = (char *) rp->p_sp;	/* return sp here */
  return(OK);
}


/*===========================================================================*
 *				do_times				     * 
 *===========================================================================*/
PRIVATE int do_times(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_times().  Retrieve the accounting information. */

  register struct proc *rp;
  int k;

  k = m_ptr->PROC1;		/* k tells whose times are wanted */
  if (k < 0 || k >= NR_PROCS) return(E_BAD_PROC);
  rp = proc_addr(k);

  /* Insert the four times needed by the TIMES system call in the message. */
  m_ptr->USER_TIME   = rp->user_time;
  m_ptr->SYSTEM_TIME = rp->sys_time;
  m_ptr->CHILD_UTIME = rp->child_utime;
  m_ptr->CHILD_STIME = rp->child_stime;
  return(OK);
}


/*===========================================================================*
 *				do_abort				     * 
 *===========================================================================*/
PRIVATE int do_abort(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_abort.  MINIX is unable to continue.  Terminate operation. */

  panic("", NO_NUM);
}


/*===========================================================================*
 *				do_sig					     * 
 *===========================================================================*/
PRIVATE int do_sig(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_sig(). Signal a process.  The stack is known to be big enough. */

  register struct proc *rp;
  phys_bytes src_phys, dst_phys;
  vir_bytes vir_addr, sig_size, new_sp;
  int proc_nr;			/* process number */
  int sig;			/* signal number 1-16 */
  int (*sig_handler)();		/* pointer to the signal handler */

  /* Extract parameters and prepare to build the words that get pushed. */
  proc_nr = m_ptr->PR;		/* process being signalled */
  sig = m_ptr->SIGNUM;		/* signal number, 1 to 16 */
  sig_handler = m_ptr->FUNC;	/* run time system addr for catching sigs */
  if (proc_nr < LOW_USER || proc_nr >= NR_PROCS) return(E_BAD_PROC);
  rp = proc_addr(proc_nr);
  vir_addr = (vir_bytes) sig_stuff;	/* info to be pushed is in 'sig_stuff' */
  new_sp = (vir_bytes) rp->p_sp;

  /* Actually build the block of words to push onto the stack. */
  build_sig(sig_stuff, rp, sig);	/* build up the info to be pushed */

  /* Prepare to do the push, and do it. */
  sig_size = SIG_PUSH_BYTES;
  new_sp -= sig_size;
  src_phys = umap(proc_addr(SYSTASK), D, vir_addr, sig_size);
  dst_phys = umap(rp, S, new_sp, sig_size);
  if (dst_phys == 0) panic("do_sig can't signal; SP bad", NO_NUM);
  phys_copy(src_phys, dst_phys, (phys_bytes) sig_size);	/* push pc, psw */

  /* Change process' sp and pc to reflect the interrupt. */
  rp->p_sp = (int *) new_sp;
  rp->p_pcpsw.pc = sig_handler;
  return(OK);
}


/*===========================================================================*
 *				do_copy					     *
 *===========================================================================*/
PRIVATE int do_copy(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_copy().  Copy data for MM or FS. */

  int src_proc, dst_proc, src_space, dst_space;
  vir_bytes src_vir, dst_vir;
  phys_bytes src_phys, dst_phys, bytes;

  /* Dismember the command message. */
  src_proc = m_ptr->SRC_PROC_NR;
  dst_proc = m_ptr->DST_PROC_NR;
  src_space = m_ptr->SRC_SPACE;
  dst_space = m_ptr->DST_SPACE;
  src_vir = (vir_bytes) m_ptr->SRC_BUFFER;
  dst_vir = (vir_bytes) m_ptr->DST_BUFFER;
  bytes = (phys_bytes) m_ptr->COPY_BYTES;

  /* Compute the source and destination addresses and do the copy. */
  if (src_proc == ABS)
	src_phys = (phys_bytes) m_ptr->SRC_BUFFER;
  else
	src_phys = umap(proc_addr(src_proc),src_space,src_vir,(vir_bytes)bytes);

  if (dst_proc == ABS)
	dst_phys = (phys_bytes) m_ptr->DST_BUFFER;
  else
	dst_phys = umap(proc_addr(dst_proc),dst_space,dst_vir,(vir_bytes)bytes);

  if (src_phys == 0 || dst_phys == 0) return(EFAULT);
  phys_copy(src_phys, dst_phys, bytes);
  return(OK);
}


/*===========================================================================*
 *				cause_sig				     * 
 *===========================================================================*/
PUBLIC cause_sig(proc_nr, sig_nr)
int proc_nr;			/* process to be signalled */
int sig_nr;			/* signal to be sent in range 1 - 16 */
{
/* A task wants to send a signal to a process.   Examples of such tasks are:
 *   TTY wanting to cause SIGINT upon getting a DEL
 *   CLOCK wanting to cause SIGALRM when timer expires
 * Signals are handled by sending a message to MM.  The tasks don't dare do
 * that directly, for fear of what would happen if MM were busy.  Instead they
 * call cause_sig, which sets bits in p_pending, and then carefully checks to
 * see if MM is free.  If so, a message is sent to it.  If not, when it becomes
 * free, a message is sent.  The calling task always gets control back from 
 * cause_sig() immediately.
 */

  register struct proc *rp;

  rp = proc_addr(proc_nr);
  if (rp->p_pending == 0) sig_procs++;	/* incr if a new proc is now pending */
  rp->p_pending |= 1 << (sig_nr - 1);
  inform(MM_PROC_NR);		/* see if MM is free */
}


/*===========================================================================*
 *				inform					     * 
 *===========================================================================*/
PUBLIC inform(proc_nr)
int proc_nr;			/* MM_PROC_NR or FS_PROC_NR */
{
/* When a signal is detected by the kernel (e.g., DEL), or generated by a task
 * (e.g. clock task for SIGALRM), cause_sig() is called to set a bit in the
 * p_pending field of the process to signal.  Then inform() is called to see
 * if MM is idle and can be told about it.  Whenever MM blocks, a check is
 * made to see if 'sig_procs' is nonzero; if so, inform() is called.
 */

  register struct proc *rp, *mmp;

  /* If MM is not waiting for new input, forget it. */
  mmp = proc_addr(proc_nr);
  if ( ((mmp->p_flags & RECEIVING) == 0) || mmp->p_getfrom != ANY) return;

  /* MM is waiting for new input.  Find a process with pending signals. */
  for (rp = proc_addr(0); rp < proc_addr(NR_PROCS); rp++)
	if (rp->p_pending != 0) {
		m.m_type = KSIG;
		m.PROC1 = rp - proc - NR_TASKS;
		m.SIG_MAP = rp->p_pending;
		sig_procs--;
		if (mini_send(HARDWARE, proc_nr, &m) != OK) 
			panic("can't inform MM", NO_NUM);
		rp->p_pending = 0;	/* the ball is now in MM's court */
		return;
	}
}


/*===========================================================================*
 *				umap					     * 
 *===========================================================================*/
PUBLIC phys_bytes umap(rp, seg, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
int seg;			/* T, D, or S segment */
vir_bytes vir_addr;		/* virtual address in bytes within the seg */
vir_bytes bytes;		/* # of bytes to be copied */
{
/* Calculate the physical memory address for a given virtual address. */
  vir_clicks vc;		/* the virtual address in clicks */
  phys_bytes seg_base, pa;	/* intermediate variables as phys_bytes */

  /* If 'seg' is D it could really be S and vice versa.  T really means T.
   * If the virtual address falls in the gap,  it causes a problem. On the
   * 8088 it is probably a legal stack reference, since "stackfaults" are
   * not detected by the hardware.  On 8088s, the gap is called S and
   * accepted, but on other machines it is called D and rejected.
   */
  if (bytes <= 0) return( (phys_bytes) 0);
  vc = (vir_addr + bytes - 1) >> CLICK_SHIFT;	/* last click of data */

#ifdef i8088
  if (seg != T)
	seg = (vc < rp->p_map[D].mem_vir + rp->p_map[D].mem_len ? D : S);
#else
  if (seg != T)
	seg = (vc < rp->p_map[S].mem_vir ? D : S);
#endif

  if((vir_addr>>CLICK_SHIFT) >= rp->p_map[seg].mem_vir + rp->p_map[seg].mem_len)
	return( (phys_bytes) 0 );
  seg_base = (phys_bytes) rp->p_map[seg].mem_phys;
  seg_base = seg_base << CLICK_SHIFT;	/* segment orgin in bytes */
  pa = (phys_bytes) vir_addr;
  pa -= rp->p_map[seg].mem_vir << CLICK_SHIFT;
  return(seg_base + pa);
}

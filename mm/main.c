/* This file contains the main program of the memory manager and some related
 * procedures.  When MINIX starts up, the kernel runs for a little while,
 * initializing itself and its tasks, and then it runs MM.  MM at this point
 * does not know where FS is in memory and how big it is.  By convention, FS
 * must start at the click following MM, so MM can deduce where it starts at
 * least.  Later, when FS runs for the first time, FS makes a pseudo-call,
 * BRK2, to tell MM how big it is.  This allows MM to figure out where INIT
 * is.
 *
 * The entry points into this file are:
 *   main:	starts MM running
 *   reply:	reply to a process making an MM system call
 *   do_brk2:	pseudo-call for FS to report its size
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "glo.h"
#include "mproc.h"
#include "param.h"

#define ENOUGH (phys_clicks) 4096	/* any # > max(FS size, INIT size) */
#define CLICK_TO_K (1024L/CLICK_SIZE)	/* convert clicks to K */

PRIVATE phys_clicks tot_mem;
extern (*call_vec[])();

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC main()
{
/* Main routine of the memory manager. */

  int error;

  mm_init();			/* initialize memory manager tables */

  /* This is MM's main loop-  get work and do it, forever and forever. */
  while (TRUE) {
	/* Wait for message. */
	get_work();		/* wait for an MM system call */
	mp = &mproc[who];

  	/* Set some flags. */
	error = OK;
	dont_reply = FALSE;
	err_code = -999;

	/* If the call number is valid, perform the call. */
	if (mm_call < 0 || mm_call >= NCALLS)
		error = E_BAD_CALL;
	else
		error = (*call_vec[mm_call])();

	/* Send the results back to the user to indicate completion. */
	if (dont_reply) continue;	/* no reply for EXIT and WAIT */
	if (mm_call == EXEC && error == OK) continue;
	reply(who, error, result2, res_ptr);
  }
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE get_work()
{  
/* Wait for the next message and extract useful information from it. */

  if (receive(ANY, &mm_in) != OK) panic("MM receive error", NO_NUM);
  who = mm_in.m_source;		/* who sent the message */
  if (who < HARDWARE || who >= NR_PROCS) panic("MM called by", who);
  mm_call = mm_in.m_type;	/* system call number */
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC reply(proc_nr, result, res2, respt)
int proc_nr;			/* process to reply to */
int result;			/* result of the call (usually OK or error #)*/
int res2;			/* secondary result */
char *respt;			/* result if pointer */
{
/* Send a reply to a user process. */

  register struct mproc *proc_ptr;

  /* To make MM robust, check to see if destination is still alive. */
  proc_ptr = &mproc[proc_nr];
  if ( (proc_ptr->mp_flags&IN_USE) == 0 || (proc_ptr->mp_flags&HANGING)) return;
  reply_type = result;
  reply_i1 = res2;
  reply_p1 = respt;
  if (send(proc_nr, &mm_out) != OK) panic("MM can't reply", NO_NUM);
}


/*===========================================================================*
 *				mm_init					     *
 *===========================================================================*/
PRIVATE mm_init()
{
/* Initialize the memory manager. */

  extern phys_clicks get_tot_mem(), alloc_mem();

  /* Find out how much memory the machine has and set up core map.  MM and FS
   * are part of the map.  Tell the kernel.
   */
  tot_mem = get_tot_mem();	/* # clicks in mem starting at absolute 0 */
  mem_init(tot_mem);		/* initialize tables to all physical mem */

  /* Initialize MM's tables. */
  mproc[MM_PROC_NR].mp_flags |= IN_USE;
  mproc[FS_PROC_NR].mp_flags |= IN_USE;
  mproc[INIT_PROC_NR].mp_flags |= IN_USE;
  procs_in_use = 3;

  /* Set stack limit, which is checked on every procedure call. */
}


/*===========================================================================*
 *				do_brk2	   				     *
 *===========================================================================*/
PUBLIC do_brk2()
{
/* This "call" is made once by FS during system initialization and then never
 * again by anyone.  It contains the origin and size of INIT, and the combined
 * size of the 1536 bytes of unused mem, MINIX and RAM disk.
 *   m1_i1 = size of INIT text in clicks
 *   m1_i2 = size of INIT data in clicks
 *   m1_i3 = number of bytes for MINIX + RAM DISK
 *   m1_p1 = origin of INIT in clicks
 */

  int mem1, mem2, mem3;
  register struct mproc *rmp;
  phys_clicks init_org, init_clicks, ram_base, ram_clicks, tot_clicks;
  phys_clicks init_text_clicks, init_data_clicks;

  if (who != FS_PROC_NR) return(EPERM);	/* only FS make do BRK2 */

  /* Remove the memory used by MINIX and RAM disk from the memory map. */
  init_text_clicks = mm_in.m1_i1;	/* size of INIT in clicks */
  init_data_clicks = mm_in.m1_i2;	/* size of INIT in clicks */
  tot_clicks = mm_in.m1_i3;		/* total size of MINIX + RAM disk */
  init_org = (phys_clicks) mm_in.m1_p1;	/* addr where INIT begins in memory */
  init_clicks = init_text_clicks + init_data_clicks;
  ram_base = init_org + init_clicks;	/* start of RAM disk */
  ram_clicks = tot_clicks - ram_base;	/* size of RAM disk */
  alloc_mem(tot_clicks);		/* remove RAM disk from map */

  /* Print memory information. */
  mem1 = tot_mem/CLICK_TO_K;
  mem2 = (ram_base + 512/CLICK_SIZE)/CLICK_TO_K;	/* MINIX, rounded */
  mem3 = ram_clicks/CLICK_TO_K;
  printf("%c 8%c~0",033, 033);	/* go to top of screen and clear screen */
  printf("Memory size = %dK     ", mem1);
  printf("MINIX = %dK     ", mem2);
  printf("RAM disk = %dK     ", mem3);
  printf("Available = %dK\n\n", mem1 - mem2 - mem3);
  if (mem1 - mem2 - mem3 < 32) {
	printf("\nNot enough memory to run MINIX\n\n", NO_NUM);
	sys_abort();
  }

  /* Initialize INIT's table entry. */
  rmp = &mproc[INIT_PROC_NR];
  rmp->mp_seg[T].mem_phys = init_org;
  rmp->mp_seg[T].mem_len  = init_text_clicks;
  rmp->mp_seg[D].mem_phys = init_org + init_text_clicks;
  rmp->mp_seg[D].mem_len  = init_data_clicks;
  rmp->mp_seg[S].mem_vir  = init_clicks;
  rmp->mp_seg[S].mem_phys = init_org + init_clicks;
  if (init_text_clicks != 0) rmp->mp_flags |= SEPARATE;

  return(OK);
}


/*===========================================================================*
 *				set_map	   				     *
 *===========================================================================*/
PRIVATE set_map(proc_nr, base, clicks)
int proc_nr;			/* whose map to set? */
phys_clicks base;		/* where in memory does the process start? */
phys_clicks clicks;		/* total size in clicks (sep I & D not used) */
{
/* Set up the memory map as part of the system initialization. */

  register struct mproc *rmp;
  vir_clicks vclicks;

  rmp = &mproc[proc_nr];
  vclicks = (vir_clicks) clicks;
  rmp->mp_seg[T].mem_vir = 0;
  rmp->mp_seg[T].mem_len = 0;
  rmp->mp_seg[T].mem_phys = base;
  rmp->mp_seg[D].mem_vir = 0;
  rmp->mp_seg[D].mem_len = vclicks;
  rmp->mp_seg[D].mem_phys = base;
  rmp->mp_seg[S].mem_vir = vclicks;
  rmp->mp_seg[S].mem_len = 0;
  rmp->mp_seg[S].mem_phys = base + vclicks;
  sys_newmap(proc_nr, rmp->mp_seg);
}

/* The MINIX model of memory allocation reserves a fixed amount of memory for
 * the combined text, data, and stack segements.  The amount used for a child
 * process created by FORK is the same as the parent had.  If the child does
 * an EXEC later, the new size is taken from the header of the file EXEC'ed.
 *
 * The layout in memory consists of the text segment, followed by the data
 * segment, followed by a gap (unused memory), followed by the stack segment.
 * The data segment grows upward and the stack grows downward, so each can
 * take memory from the gap.  If they meet, the process must be killed.  The
 * procedures in this file deal with the growth of the data and stack segments.
 *
 * The entry points into this file are:
 *   do_brk:	  BRK/SBRK system calls to grow or shrink the data segment
 *   adjust:	  see if a proposed segment adjustment is allowed
 *   size_ok:	  see if the segment sizes are feasible
 *   stack_fault: grow the stack segment
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/error.h"
#include "../h/signal.h"
#include "const.h"
#include "glo.h"
#include "mproc.h"
#include "param.h"

#define DATA_CHANGED       1	/* flag value when data segment size changed */
#define STACK_CHANGED      2	/* flag value when stack size changed */

/*===========================================================================*
 *				do_brk  				     *
 *===========================================================================*/
PUBLIC int do_brk()
{
/* Perform the brk(addr) system call.
 *
 * The call is complicated by the fact that on some machines (e.g., 8088),
 * the stack pointer can grow beyond the base of the stack segment without
 * anybody noticing it.    For a file not using separate I & D space,
 * the parameter, 'addr' is to the total size, text + data.  For a file using
 * separate text and data spaces, it is just the data size. Files using
 * separate I & D space have the SEPARATE bit in mp_flags set.
 */

  register struct mproc *rmp;
  int r;
  vir_bytes v, new_sp;
  vir_clicks new_clicks;

  rmp = mp;
  v = (vir_bytes) addr;		/* 'addr' is the new data segment size */
  new_clicks = (vir_clicks) ( ((long) v + CLICK_SIZE - 1) >> CLICK_SHIFT);
  sys_getsp(who, &new_sp);	/* ask kernel for current sp value */
  r = adjust(rmp, new_clicks, new_sp);
  res_ptr = (r == OK ? addr : (char *) -1);
  return(r);			/* return new size or -1 */
}


/*===========================================================================*
 *				adjust  				     *
 *===========================================================================*/
PUBLIC int adjust(rmp, data_clicks, sp)
register struct mproc *rmp;	/* whose memory is being adjusted? */
vir_clicks data_clicks;		/* how big is data segment to become? */
vir_bytes sp;			/* new value of sp */
{
/* See if data and stack segments can coexist, adjusting them if need be.
 * Memory is never allocated or freed.  Instead it is added or removed from the
 * gap between data segment and stack segment.  If the gap size becomes
 * negative, the adjustment of data or stack fails and ENOMEM is returned.
 */

  register struct mem_map *mem_sp, *mem_dp;
  vir_clicks sp_click, gap_base, lower, old_clicks;
  int changed, r, ft;
  long base_of_stack, delta;	/* longs avoid certain problems */

  mem_dp = &rmp->mp_seg[D];	/* pointer to data segment map */
  mem_sp = &rmp->mp_seg[S];	/* pointer to stack segment map */
  changed = 0;			/* set when either segment changed */

  /* See if stack size has gone negative (i.e., sp too close to 0xFFFF...) */
  base_of_stack = (long) mem_sp->mem_vir + (long) mem_sp->mem_len;
  sp_click = sp >> CLICK_SHIFT;	/* click containing sp */
  if (sp_click >= base_of_stack) return(ENOMEM);	/* sp too high */

  /* Compute size of gap between stack and data segments. */
  delta = (long) mem_sp->mem_vir - (long) sp_click;
  lower = (delta > 0 ? sp_click : mem_sp->mem_vir);
  gap_base = mem_dp->mem_vir + data_clicks;
  if (lower < gap_base) return(ENOMEM);	/* data and stack collided */

  /* Update data length (but not data orgin) on behalf of brk() system call. */
  old_clicks = mem_dp->mem_len;
  if (data_clicks != mem_dp->mem_len) {
	mem_dp->mem_len = data_clicks;
	changed |= DATA_CHANGED;
  }

  /* Update stack length and origin due to change in stack pointer. */
  if (delta > 0) {
	mem_sp->mem_vir -= delta;
	mem_sp->mem_phys -= delta;
	mem_sp->mem_len += delta;
	changed |= STACK_CHANGED;
  }

  /* Do the new data and stack segment sizes fit in the address space? */
  ft = (rmp->mp_flags & SEPARATE);
  r = size_ok(ft, rmp->mp_seg[T].mem_len, rmp->mp_seg[D].mem_len, 
       rmp->mp_seg[S].mem_len, rmp->mp_seg[D].mem_vir, rmp->mp_seg[S].mem_vir);
  if (r == OK) {
	if (changed) sys_newmap(rmp - mproc, rmp->mp_seg);
	return(OK);
  }

  /* New sizes don't fit or require too many page/segment registers. Restore.*/
  if (changed & DATA_CHANGED) mem_dp->mem_len = old_clicks;
  if (changed & STACK_CHANGED) {
	mem_sp->mem_vir += delta;
	mem_sp->mem_phys += delta;
	mem_sp->mem_len -= delta;
  }
  return(ENOMEM);
}


/*===========================================================================*
 *				size_ok  				     *
 *===========================================================================*/
PUBLIC int size_ok(file_type, tc, dc, sc, dvir, s_vir)
int file_type;			/* SEPARATE or 0 */
vir_clicks tc;			/* text size in clicks */
vir_clicks dc;			/* data size in clicks */
vir_clicks sc;			/* stack size in clicks */
vir_clicks dvir;		/* virtual address for start of data seg */
vir_clicks s_vir;		/* virtual address for start of stack seg */
{
/* Check to see if the sizes are feasible and enough segmentation registers
 * exist.  On a machine with eight 8K pages, text, data, stack sizes of
 * (32K, 16K, 16K) will fit, but (33K, 17K, 13K) will not, even though the
 * former is bigger (64K) than the latter (63K).  Even on the 8088 this test
 * is needed, since the data and stack may not exceed 4096 clicks.
 */

  int pt, pd, ps;		/* segment sizes in pages */

  pt = ( (tc << CLICK_SHIFT) + PAGE_SIZE - 1)/PAGE_SIZE;
  pd = ( (dc << CLICK_SHIFT) + PAGE_SIZE - 1)/PAGE_SIZE;
  ps = ( (sc << CLICK_SHIFT) + PAGE_SIZE - 1)/PAGE_SIZE;

  if (file_type == SEPARATE) {
	if (pt > MAX_PAGES || pd + ps > MAX_PAGES) return(ENOMEM);
  } else {
	if (pt + pd + ps > MAX_PAGES) return(ENOMEM);
  }

  if (dvir + dc > s_vir) return(ENOMEM);

  return(OK);
}


/*===========================================================================*
 *				stack_fault  				     *
 *===========================================================================*/
PUBLIC stack_fault(proc_nr)
int proc_nr;			/* tells who got the stack fault */
{
/* Handle a stack fault by growing the stack segment until sp is inside of it. 
 * If this is impossible because data segment is in the way, kill the process.
 */

  register struct mproc *rmp;
  int r;
  vir_bytes new_sp;

  rmp = &mproc[proc_nr];
  sys_getsp(rmp - mproc, &new_sp);
  r = adjust(rmp, rmp->mp_seg[D].mem_len, new_sp);
  if (r == OK) return;

  /* Stack has bumped into data segment.  Kill the process. */
  rmp->mp_catch = 0;		/* don't catch this signal */
  sig_proc(rmp, SIGSEGV);	/* terminate process */
}

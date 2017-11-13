/* This file contains the main program of MINIX.  The routine main()
 * initializes the system and starts the ball rolling by setting up the proc
 * table, interrupt vectors, and scheduling each task to run to initialize
 * itself.
 * 
 * The entries into this file are:
 *   main:		MINIX main program
 *   unexpected_int:	called when an interrupt to an unused vector < 16 occurs
 *   trap:		called when an unexpected trap to a vector >= 16 occurs
 *   panic:		abort MINIX due to a fatal error
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

#define SAFETY             8	/* margin of safety for stack overflow (ints)*/
#define VERY_BIG       39328	/* must be bigger than kernel size (clicks) */
#define BASE            1536	/* address where MINIX starts in memory */
#define SIZES              8	/* sizes array has 8 entries */
#define CPU_TY1       0xFFFF	/* BIOS segment that tells CPU type */
#define CPU_TY2       0x000E	/* BIOS offset that tells CPU type */
#define PC_AT           0xFC	/* IBM code for PC-AT (in BIOS at 0xFFFFE) */

/*===========================================================================*
 *                                   main                                    * 
 *===========================================================================*/
PUBLIC main()
{
/* Start the ball rolling. */

  register struct proc *rp;
  register int t;
  vir_clicks size;
  phys_clicks base_click, mm_base, previous_base;
  phys_bytes phys_b;
  extern unsigned sizes[8];	/* table filled in by build */
  extern int color, vec_table[], get_chrome(), (*task[])();
  extern int s_call(), disk_int(), tty_int(), clock_int(), disk_int();
  extern int wini_int(), lpr_int(), surprise(), trp(), divide();
  extern phys_bytes umap();

  /* Set up proc table entry for user processes.  Be very careful about
   * sp, since the 3 words prior to it will be clobbered when the kernel pushes
   * pc, cs, and psw onto the USER's stack when starting the user the first
   * time.  This means that with initial sp = 0x10, user programs must leave 
   * the words at 0x000A, 0x000C, and 0x000E free.
   */

  lock();			/* we can't handle interrupts yet */
  base_click = BASE >> CLICK_SHIFT;
  size = sizes[0] + sizes[1];	/* kernel text + data size in clicks */
  mm_base = base_click + size;	/* place where MM starts (in clicks) */

  for (rp = &proc[0]; rp <= &proc[NR_TASKS+LOW_USER]; rp++) {
	for (t=0; t< NR_REGS; t++) rp->p_reg[t] = 0100*t;	/* debugging */
	t = rp - proc - NR_TASKS;	/* task number */
	rp->p_sp = (rp < &proc[NR_TASKS] ? t_stack[NR_TASKS+t+1].stk : INIT_SP);
	rp->p_splimit = rp->p_sp;
	if (rp->p_splimit != INIT_SP)
		rp->p_splimit -= (TASK_STACK_BYTES - SAFETY)/sizeof(int);
	rp->p_pcpsw.pc = task[t + NR_TASKS];
	if (rp->p_pcpsw.pc != 0 || t >= 0) ready(rp);
	rp->p_pcpsw.psw = INIT_PSW;
	rp->p_flags = 0;

	/* Set up memory map for tasks and MM, FS, INIT. */
	if (t < 0) {
		/* I/O tasks. */
		rp->p_map[T].mem_len  = VERY_BIG; 
		rp->p_map[T].mem_phys = base_click;
		rp->p_map[D].mem_len  = VERY_BIG; 
		rp->p_map[D].mem_phys = base_click + sizes[0];
		rp->p_map[S].mem_len  = VERY_BIG; 
		rp->p_map[S].mem_phys = base_click + sizes[0] + sizes[1];
		rp->p_map[S].mem_vir = sizes[0] + sizes[1];
	} else {
		/* MM, FS, and INIT. */
		previous_base = proc[NR_TASKS + t - 1].p_map[S].mem_phys;
		rp->p_map[T].mem_len  = sizes[2*t + 2];
		rp->p_map[T].mem_phys = (t == 0 ? mm_base : previous_base);
		rp->p_map[D].mem_len  = sizes[2*t + 3];
		rp->p_map[D].mem_phys = rp->p_map[T].mem_phys + sizes[2*t + 2];
		rp->p_map[S].mem_vir  = sizes[2*t + 3];
		rp->p_map[S].mem_phys = rp->p_map[D].mem_phys + sizes[2*t + 3];
	}

#ifdef i8088
	rp->p_reg[CS_REG] = rp->p_map[T].mem_phys;
	rp->p_reg[DS_REG] = rp->p_map[D].mem_phys;
	rp->p_reg[SS_REG] = rp->p_map[D].mem_phys;
	rp->p_reg[ES_REG] = rp->p_map[D].mem_phys;
#endif
  }

  proc[NR_TASKS+(HARDWARE)].p_sp = (int *) k_stack;
  proc[NR_TASKS+(HARDWARE)].p_sp += K_STACK_BYTES/2;
  proc[NR_TASKS+(HARDWARE)].p_splimit = (int *) k_stack;
  proc[NR_TASKS+(HARDWARE)].p_splimit += SAFETY/2;

  for (rp = proc_addr(LOW_USER+1); rp < proc_addr(NR_PROCS); rp++)
	rp->p_flags = P_SLOT_FREE;

  /* Determine if display is color or monochrome and CPU type (from BIOS). */
  color = get_chrome();		/* 0 = mono, 1 = color */
  t = get_byte(CPU_TY1, CPU_TY2);	/* is this PC, XT, AT ... ? */
  if (t == PC_AT) pc_at = TRUE;

  /* Save the old interrupt vectors. */
  phys_b = umap(proc_addr(HARDWARE), D, (vir_bytes) vec_table, VECTOR_BYTES);
  phys_copy(0L, phys_b, (long) VECTOR_BYTES);	/* save all the vectors */

  /* Set up the new interrupt vectors. */
  for (t = 0; t < 16; t++) set_vec(t, surprise, base_click);
  for (t = 16; t < 256; t++) set_vec(t, trp, base_click);
  set_vec(DIVIDE_VECTOR, divide, base_click);
  set_vec(SYS_VECTOR, s_call, base_click);
  set_vec(CLOCK_VECTOR, clock_int, base_click);
  set_vec(KEYBOARD_VECTOR, tty_int, base_click);
  set_vec(FLOPPY_VECTOR, disk_int, base_click);
  set_vec(PRINTER_VECTOR, lpr_int, base_click);
  if (pc_at)
	  set_vec(AT_WINI_VECTOR, wini_int, base_click);
  else
	  set_vec(XT_WINI_VECTOR, wini_int, base_click);

  /* Put a ptr to proc table in a known place so it can be found in /dev/mem */
  set_vec( (BASE - 4)/4, proc, (phys_clicks) 0);

  bill_ptr = proc_addr(HARDWARE);	/* it has to point somewhere */
  pick_proc();

  /* Now go to the assembly code to start running the current process. */
  port_out(INT_CTLMASK, 0);	/* do not mask out any interrupts in 8259A */
  port_out(INT2_MASK, 0);	/* same for second interrupt controller */
  restart();
}


/*===========================================================================*
 *                                   unexpected_int                          * 
 *===========================================================================*/
PUBLIC unexpected_int()
{
/* A trap or interrupt has occurred that was not expected. */

  printf("Unexpected trap: vector < 16\n");
  printf("pc = 0x%x    text+data+bss = 0x%x\n",proc_ptr->p_pcpsw.pc,
					proc_ptr->p_map[D].mem_len<<4);
}


/*===========================================================================*
 *                                   trap                                    * 
 *===========================================================================*/
PUBLIC trap()
{
/* A trap (vector >= 16) has occurred.  It was not expected. */

  printf("\nUnexpected trap: vector >= 16 ");
  printf("This may be due to accidentally including\n");
  printf("a non-MINIX library routine that is trying to make a system call.\n");
  printf("pc = 0x%x    text+data+bss = 0x%x\n",proc_ptr->p_pcpsw.pc,
					proc_ptr->p_map[D].mem_len<<4);
}


/*===========================================================================*
 *                                   div_trap                                * 
 *===========================================================================*/
PUBLIC div_trap()
{
/* The divide intruction traps to vector 0 upon overflow. */

  printf("Trap to vector 0: divide overflow.  ");
  printf("pc = 0x%x    text+data+bss = 0x%x\n",proc_ptr->p_pcpsw.pc,
					proc_ptr->p_map[D].mem_len<<4);
}


/*===========================================================================*
 *                                   panic                                   * 
 *===========================================================================*/
PUBLIC panic(s,n)
char *s;
int n; 
{
/* The system has run aground of a fatal error.  Terminate execution.
 * If the panic originated in MM or FS, the string will be empty and the
 * file system already syncked.  If the panic originates in the kernel, we are
 * kind of stuck. 
 */

  if (*s != 0) {
	printf("\nKernel panic: %s",s); 
	if (n != NO_NUM) printf(" %d", n);
	printf("\n");
  }
  printf("\nType space to reboot\n");
  wreboot();

}

#ifdef i8088
/*===========================================================================*
 *                                   set_vec                                 * 
 *===========================================================================*/
PRIVATE set_vec(vec_nr, addr, base_click)
int vec_nr;			/* which vector */
int (*addr)();			/* where to start */
phys_clicks base_click;		/* click where kernel sits in memory */
{
/* Set up an interrupt vector. */

  unsigned vec[2];
  unsigned u;
  phys_bytes phys_b;
  extern unsigned sizes[8];

  /* Build the vector in the array 'vec'. */
  vec[0] = (unsigned) addr;
  vec[1] = (unsigned) base_click;
  u = (unsigned) vec;

  /* Copy the vector into place. */
  phys_b = ( (phys_bytes) base_click + (phys_bytes) sizes[0]) << CLICK_SHIFT;
  phys_b += u;
  phys_copy(phys_b, (phys_bytes) 4*vec_nr, (phys_bytes) 4);
}
#endif

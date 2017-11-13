/* This file contains some dumping routines for debugging. */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"

#define NSIZE 20
phys_bytes aout[NR_PROCS];	/* pointers to the program names */
char nbuff[NSIZE+1];
int vargv;

/*===========================================================================*
 *				DEBUG routines here			     * 
 *===========================================================================*/
p_dmp()
{
/* Proc table dump */

  register struct proc *rp;
  char *np;
  vir_bytes base, limit, first, last;
  phys_bytes ltmp, dst;
  int index;
  extern phys_bytes umap();

  printf(
  "\nproc  -pid- -pc-  -sp-  flag  user  -sys-  base limit recv   command\n");

  dst = umap(proc_addr(SYSTASK), D, nbuff, NSIZE);

  for (rp = &proc[0]; rp < &proc[NR_PROCS+NR_TASKS]; rp++)  {
	if (rp->p_flags & P_SLOT_FREE) continue;
	first = rp->p_map[T].mem_phys;
	last = rp->p_map[S].mem_phys + rp->p_map[S].mem_len;
	ltmp = ((long) first << 4) + 512L;
	base = (vir_bytes) (ltmp/1024L);
	ltmp = (((long) last << 4) + 512L);
	limit = (vir_bytes) (ltmp/1024L);
	prname(rp - proc);
	printf(" %4d %4x %4x %4x %6D %7D  %3dK %3dK  ",
		rp->p_pid, rp->p_pcpsw.pc, rp->p_sp, rp->p_flags,
		rp->user_time, rp->sys_time,
		base, limit);
		if (rp->p_flags == 0)
			printf("      ");
		else
			prname(NR_TASKS + rp->p_getfrom);

	/* Fetch the command string from the user process. */
	index = rp - proc - NR_TASKS;
	if (index > LOW_USER && aout[index] != 0) {
		phys_copy(aout[index], dst, (long) NSIZE);
		aout[NSIZE] = 0;
		for (np = &nbuff[0]; np < &nbuff[NSIZE]; np++)
			if (*np <= ' ' || *np >= 0177) *np = 0;
		if (index == LOW_USER + 1)
			printf("/bin/sh");
		else
			printf("%s", nbuff);
	}
	printf("\n");
  }
  printf("\n");
}



map_dmp()
{
  register struct proc *rp;
  vir_bytes base, limit, first, last;
  phys_bytes ltmp;

  printf("\nPROC   -----TEXT-----  -----DATA-----  ----STACK-----  BASE SIZE\n");
  for (rp = &proc[NR_TASKS]; rp < &proc[NR_TASKS+NR_PROCS]; rp++)  {
	if (rp->p_flags & P_SLOT_FREE) continue;
	first = rp->p_map[T].mem_phys;
	last = rp->p_map[S].mem_phys + rp->p_map[S].mem_len;
	ltmp = ((long) first << 4) + 512L;
	base = (vir_bytes) (ltmp/1024L);
	ltmp = (((long) (last-first) << 4) + 512L);
	limit = (vir_bytes) (ltmp/1024L);
	prname(rp-proc);
	printf(" %4x %4x %4x  %4x %4x %4x  %4x %4x %4x  %3dK %3dK\n", 
	    rp->p_map[T].mem_vir, rp->p_map[T].mem_phys, rp->p_map[T].mem_len,
	    rp->p_map[D].mem_vir, rp->p_map[D].mem_phys, rp->p_map[D].mem_len,
	    rp->p_map[S].mem_vir, rp->p_map[S].mem_phys, rp->p_map[S].mem_len,
	    base, limit);
  }
}


char *nayme[]= {"PRINTR", "TTY   ", "WINCHE", "FLOPPY", "RAMDSK", "CLOCK ", 
		"SYS   ", "HARDWR", "MM    ", "FS    ", "INIT  "};
prname(i)
int i;
{
  if (i == ANY+NR_TASKS)
	printf("ANY   ");
  else if (i >= 0 && i <= NR_TASKS+2)
	printf("%s",nayme[i]);
  else
	printf("%4d  ", i-NR_TASKS);
}

set_name(proc_nr, ptr)
int proc_nr;
char *ptr;
{
/* When an EXEC call is done, the kernel is told about the stack pointer.
 * It uses the stack pointer to find the command line, for dumping
 * purposes.
 */

  extern phys_bytes umap();
  phys_bytes src, dst, count;

  if (ptr == (char *) 0) {
	aout[proc_nr] = (phys_bytes) 0;
	return;
  }

  src = umap(proc_addr(proc_nr), D, ptr + 2, 2);
  if (src == 0) return;
  dst = umap(proc_addr(SYSTASK), D, &vargv, 2);
  phys_copy(src, dst, 2L);

  aout[proc_nr] = umap(proc_addr(proc_nr), D, vargv, NSIZE);
}

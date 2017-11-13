/* This file contains the drivers for four special files:
 *     /dev/null	- null device (data sink)
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/ram		- RAM disk
 * It accepts three messages, for reading, for writing, and for
 * control. All use message format m2 and with these parameters:
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_IOCTL | device  |         |  blocks | ram org |         |
 * ----------------------------------------------------------------
 *  
 *
 * The file contains one entry point:
 *
 *   mem_task:	main entry when system is brought up
 *
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "proc.h"

#define NR_RAMS            4	/* number of RAM-type devices */

PRIVATE message mess;		/* message buffer */
PRIVATE phys_bytes ram_origin[NR_RAMS];	/* origin of each RAM disk  */
PRIVATE phys_bytes ram_limit[NR_RAMS];	/* limit of RAM disk per minor dev. */

/*===========================================================================*
 *				mem_task				     * 
 *===========================================================================*/
PUBLIC mem_task()
{
/* Main program of the disk driver task. */

  int r, caller, proc_nr;
  extern unsigned sizes[8];
  extern phys_clicks get_base();


  /* Initialize this task. */
  ram_origin[KMEM_DEV] = (phys_bytes) get_base() << CLICK_SHIFT;
  ram_limit[KMEM_DEV] = (sizes[0] + sizes[1]) << CLICK_SHIFT;
  ram_limit[MEM_DEV] = MEM_BYTES;

  /* Here is the main loop of the memory task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (TRUE) {
	/* First wait for a request to read or write. */
	receive(ANY, &mess);
	if (mess.m_source < 0)
		panic("mem task got message from ", mess.m_source);
	caller = mess.m_source;
	proc_nr = mess.PROC_NR;

	/* Now carry out the work.  It depends on the opcode. */
	switch(mess.m_type) {
	    case DISK_READ:	r = do_mem(&mess);	break;
	    case DISK_WRITE:	r = do_mem(&mess);	break;
	    case DISK_IOCTL:	r = do_setup(&mess);	break;
	    default:		r = EINVAL;		break;
	}

	/* Finally, prepare and send the reply message. */
	mess.m_type = TASK_REPLY;
	mess.REP_PROC_NR = proc_nr;
	mess.REP_STATUS = r;
	send(caller, &mess);
  }
}


/*===========================================================================*
 *				do_mem					     * 
 *===========================================================================*/
PRIVATE int do_mem(m_ptr)
register message *m_ptr;	/* pointer to read or write message */
{
/* Read or write /dev/null, /dev/mem, /dev/kmem, or /dev/ram. */

  int device, count;
  phys_bytes mem_phys, user_phys;
  struct proc *rp;
  extern phys_clicks get_base();
  extern phys_bytes umap();

  /* Get minor device number and check for /dev/null. */
  device = m_ptr->DEVICE;
  if (device < 0 || device >= NR_RAMS) return(ENXIO);	/* bad minor device */
  if (device==NULL_DEV) return(m_ptr->m_type == DISK_READ ? EOF : m_ptr->COUNT);

  /* Set up 'mem_phys' for /dev/mem, /dev/kmem, or /dev/ram. */
  if (m_ptr->POSITION < 0) return(ENXIO);
  mem_phys = ram_origin[device] + m_ptr->POSITION;
  if (mem_phys >= ram_limit[device]) return(EOF);
  count = m_ptr->COUNT;
  if(mem_phys + count > ram_limit[device]) count = ram_limit[device] - mem_phys;

  /* Determine address where data is to go or to come from. */
  rp = proc_addr(m_ptr->PROC_NR);
  user_phys = umap(rp, D, (vir_bytes) m_ptr->ADDRESS, (vir_bytes) count);
  if (user_phys == 0) return(E_BAD_ADDR);

  /* Copy the data. */
  if (m_ptr->m_type == DISK_READ)
	phys_copy(mem_phys, user_phys, (long) count);
  else
	phys_copy(user_phys, mem_phys, (long) count);
  return(count);
}


/*===========================================================================*
 *				do_setup				     * 
 *===========================================================================*/
PRIVATE int do_setup(m_ptr)
message *m_ptr;			/* pointer to read or write message */
{
/* Set parameters for one of the disk RAMs. */

  int device;

  device = m_ptr->DEVICE;
  if (device < 0 || device >= NR_RAMS) return(ENXIO);	/* bad minor device */
  ram_origin[device] = m_ptr->POSITION;
  ram_limit[device] = m_ptr->POSITION + (long) m_ptr->COUNT * BLOCK_SIZE;
  return(OK);
}

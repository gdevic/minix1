/* This file contains a driver for the IBM-AT winchester controller.
 * It was written by Adri Koppes.
 *
 * The driver supports two operations: read a block and
 * write a block.  It accepts two messages, one for reading and one for
 * writing, both using message format m2 and with the same parameters:
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   winchester_task:	main entry when system is brought up
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

/* I/O Ports used by winchester disk controller. */

#define WIN_REG1       0x1f0
#define WIN_REG2       0x1f1
#define WIN_REG3       0x1f2
#define WIN_REG4       0x1f3
#define WIN_REG5       0x1f4
#define WIN_REG6       0x1f5
#define WIN_REG7       0x1f6
#define WIN_REG8       0x1f7
#define WIN_REG9       0x3f6

/* Winchester disk controller command bytes. */
#define WIN_RECALIBRATE	0x10	/* command for the drive to recalibrate */
#define WIN_READ        0x20	/* command for the drive to read */
#define WIN_WRITE       0x30	/* command for the drive to write */
#define WIN_SPECIFY     0x91	/* command for the controller to accept params */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */

/* Error codes */
#define ERR		  -1	/* general error */

/* Miscellaneous. */
#define MAX_ERRORS         4	/* how often to try rd/wt before quitting */
#define NR_DEVICES        10	/* maximum number of drives */
#define MAX_WIN_RETRY  10000	/* max # times to try to output to WIN */
#define PART_TABLE     0x1C6	/* IBM partition table starts here in sect 0 */
#define DEV_PER_DRIVE      5	/* hd0 + hd1 + hd2 + hd3 + hd4 = 5 */

/* Variables. */
PRIVATE struct wini {		/* main drive struct, one entry per drive */
  int wn_opcode;		/* DISK_READ or DISK_WRITE */
  int wn_procnr;		/* which proc wanted this operation? */
  int wn_drive;			/* drive number addressed */
  int wn_cylinder;		/* cylinder number addressed */
  int wn_sector;		/* sector addressed */
  int wn_head;			/* head number addressed */
  int wn_heads;			/* maximum number of heads */
  int wn_maxsec;		/* maximum number of sectors per track */
  int wn_ctlbyte;		/* control byte (steprate) */
  int wn_precomp;		/* write precompensation cylinder / 4 */
  long wn_low;			/* lowest cylinder of partition */
  long wn_size;			/* size of partition in blocks */
  int wn_count;			/* byte count */
  vir_bytes wn_address;		/* user virtual address */
} wini[NR_DEVICES];

PRIVATE int w_need_reset = FALSE;	 /* set to 1 when controller must be reset */
PRIVATE int nr_drives;		 /* Number of drives */

PRIVATE message w_mess;		/* message buffer for in and out */

PRIVATE int command[8];		/* Common command block */

PRIVATE unsigned char buf[BLOCK_SIZE]; /* Buffer used by the startup routine */

/*===========================================================================*
 *				winchester_task				     * 
 *===========================================================================*/
PUBLIC winchester_task()
{
/* Main program of the winchester disk driver task. */

  int r, caller, proc_nr;

  /* First initialize the controller */
  init_param();

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */

  while (TRUE) {
	/* First wait for a request to read or write a disk block. */
	receive(ANY, &w_mess);	/* get a request to do some work */
	if (w_mess.m_source < 0) {
		printf("winchester task got message from %d ", w_mess.m_source);
		continue;
	}
	caller = w_mess.m_source;
	proc_nr = w_mess.PROC_NR;

	/* Now carry out the work. */
	switch(w_mess.m_type) {
	    case DISK_READ:
	    case DISK_WRITE:	r = w_do_rdwt(&w_mess);	break;
	    default:		r = EINVAL;		break;
	}

	/* Finally, prepare and send the reply message. */
	w_mess.m_type = TASK_REPLY;	
	w_mess.REP_PROC_NR = proc_nr;

	w_mess.REP_STATUS = r;	/* # of bytes transferred or error code */
	send(caller, &w_mess);	/* send reply to caller */
  }
}


/*===========================================================================*
 *				w_do_rdwt					     * 
 *===========================================================================*/
PRIVATE int w_do_rdwt(m_ptr)
message *m_ptr;			/* pointer to read or write w_message */
{
/* Carry out a read or write request from the disk. */
  register struct wini *wn;
  int r, device, errors = 0;
  long sector;

  /* Decode the w_message parameters. */
  device = m_ptr->DEVICE;
  if (device < 0 || device >= NR_DEVICES)
	return(EIO);
  if (m_ptr->COUNT != BLOCK_SIZE)
	return(EINVAL);
  wn = &wini[device];		/* 'wn' points to entry for this drive */
  wn->wn_drive = device/DEV_PER_DRIVE;	/* save drive number */
  if (wn->wn_drive >= nr_drives)
	return(EIO);
  wn->wn_opcode = m_ptr->m_type;	/* DISK_READ or DISK_WRITE */
  if (m_ptr->POSITION % BLOCK_SIZE != 0)
	return(EINVAL);
  sector = m_ptr->POSITION/SECTOR_SIZE;
  if ((sector+BLOCK_SIZE/SECTOR_SIZE) > wn->wn_size)
	return(EOF);
  sector += wn->wn_low;
  wn->wn_cylinder = sector / (wn->wn_heads * wn->wn_maxsec);
  wn->wn_sector =  (sector % wn->wn_maxsec) + 1;
  wn->wn_head = (sector % (wn->wn_heads * wn->wn_maxsec) )/wn->wn_maxsec;
  wn->wn_count = m_ptr->COUNT;
  wn->wn_address = (vir_bytes) m_ptr->ADDRESS;
  wn->wn_procnr = m_ptr->PROC_NR;

  /* This loop allows a failed operation to be repeated. */
  while (errors <= MAX_ERRORS) {
	errors++;		/* increment count once per loop cycle */
	if (errors > MAX_ERRORS)
		return(EIO);

	/* First check to see if a reset is needed. */
	if (w_need_reset) w_reset();

	/* Perform the transfer. */
	r = w_transfer(wn);
	if (r == OK) break;	/* if successful, exit loop */

  }

  return(r == OK ? BLOCK_SIZE : EIO);
}

/*===========================================================================*
 *				w_transfer				     * 
 *===========================================================================*/
PRIVATE int w_transfer(wn)
register struct wini *wn;	/* pointer to the drive struct */
{
  extern phys_bytes umap();
  phys_bytes win_buf = umap(proc_addr(WINCHESTER), D, buf, BLOCK_SIZE);
  phys_bytes usr_buf = umap(proc_addr(wn->wn_procnr), D, wn->wn_address, BLOCK_SIZE);
  register int i,j;
  int r;

  /* The command is issued by outputing 7 bytes to the controller chip. */

  if (win_buf == (phys_bytes)0 || usr_buf == (phys_bytes)0)
	return(ERR);
  command[0] = wn->wn_head & 8;
  command[1] = wn->wn_precomp;
  command[2] = BLOCK_SIZE/SECTOR_SIZE;
  command[3] = wn->wn_sector;
  command[4] = wn->wn_cylinder & 0xFF;
  command[5] = ((wn->wn_cylinder & 0x0300) >> 8);
  command[6] = (wn->wn_drive << 4) | wn->wn_head | 0xA0;
  command[7] = (wn->wn_opcode == DISK_READ ? WIN_READ : WIN_WRITE);

  if (wn->wn_opcode == DISK_WRITE)
	phys_copy(usr_buf, win_buf, (phys_bytes)BLOCK_SIZE);

  if (com_out() != OK)
	return(ERR);

  /* Block, waiting for disk interrupt. */
  if (wn->wn_opcode == DISK_READ) {
	for (i=0; i<BLOCK_SIZE/SECTOR_SIZE; i++) {
		receive(HARDWARE, &w_mess);
		for (j=0; j<256; j++)
			portw_in(WIN_REG1, &buf[i*512+j*2]);
		if (win_results() != OK) {
			w_need_reset = TRUE;
			return(ERR);
		}
	}
	phys_copy(win_buf, usr_buf, (phys_bytes)BLOCK_SIZE);
	r = OK;
  } else {
	for (i=0; i<MAX_WIN_RETRY && (r&8) == 0; i++)
		port_in(WIN_REG8, &r);
	if ((r&8) == 0) {
		w_need_reset = TRUE;
		return(ERR);
	}
	for (i=0; i<BLOCK_SIZE/SECTOR_SIZE; i++) {
		for (j=0; j<256; j++)
			portw_out(WIN_REG1, *(int *)&buf[i*512+j*2]);
		receive(HARDWARE, &w_mess);
		if (win_results() != OK) {
			w_need_reset = TRUE;
			return(ERR);
		}
	}
	r = OK;
  }

  if (r == ERR)
	w_need_reset = TRUE;
  return(r);
}

/*===========================================================================*
 *				w_reset					     * 
 *===========================================================================*/
PRIVATE w_reset()
{
/* Issue a reset to the controller.  This is done after any catastrophe,
 * like the controller refusing to respond.
 */

  int i, r = 4;

  /* Strobe reset bit low. */
  lock();
  port_out(WIN_REG9, r);
  for (i = 0; i < 10; i++) ;
  port_out(WIN_REG9, 0);
  unlock();
  if (drive_busy()) {
	printf("Winchester wouldn't reset, drive busy\n");
	return(ERR);
  }
  port_in(WIN_REG2, &r);
  if (r != 1) {
	printf("Winchester wouldn't reset, drive error\n");
	return(ERR);
  }

  /* Reset succeeded.  Tell WIN drive parameters. */

  w_need_reset = FALSE;
  return(win_init());
}

/*===========================================================================*
 *				win_init				     * 
 *===========================================================================*/
PRIVATE win_init()
{
/* Routine to initialize the drive parameters after boot or reset */

  register int i;

  command[0] = wini[0].wn_heads & 8;
  command[2] = wini[0].wn_maxsec;
  command[4] = 0;
  command[6] = wini[0].wn_heads || 0xA0;
  command[7] = WIN_SPECIFY;		/* Specify some parameters */

  if (com_out() != OK)	/* Output command block */
	return(ERR);

  receive(HARDWARE, &w_mess);
  if (win_results() != OK) {	/* See if controller accepted parameters */
	w_need_reset = TRUE;
	return(ERR);
  }

  if (nr_drives > 1) {
	command[0] = wini[5].wn_heads & 8;
	command[2] = wini[5].wn_maxsec;
	command[4] = 0;
	command[6] = wini[5].wn_heads || 0xA1;
	command[7] = WIN_SPECIFY;		/* Specify some parameters */

	if (com_out() != OK)			/* Output command block */
		return(ERR);
	receive(HARDWARE, &w_mess);
	if (win_results() != OK) {  /* See if controller accepted parameters */
		w_need_reset = TRUE;
		return(ERR);
	}
  }
  for (i=0; i<nr_drives; i++) {
	command[0] = wini[i*5].wn_heads & 8;
	command[6] = i << 4;
	command[7] = WIN_RECALIBRATE | (wini[i*5].wn_ctlbyte & 0x0F);
	if (com_out() != OK)
		return(ERR);
	receive(HARDWARE, &w_mess);
	if (win_results() != OK) {
		w_need_reset = TRUE;
		return(ERR);
	}
  }
  return(OK);
}

/*============================================================================*
 *				win_results				      *
 *============================================================================*/
PRIVATE win_results()
{
/* Routine to check if controller has done the operation succesfully */
  int r;

  port_in(WIN_REG8, &r);
  if ((r&0x80) != 0)
	return(OK);
  if ((r&0x40) == 0 || (r&0x20) != 0 || (r&0x10) == 0 || (r&1) != 0) {
	if ((r&01) != 0)
		port_in(WIN_REG2, &r);
	return(ERR);
  }
  return(OK);
}

/*============================================================================*
 *				drive_busy				      *
 *============================================================================*/
PRIVATE drive_busy()
{
/* Wait until the controller is ready to receive a command or send status */

  register int i = 0;
  int r;

  for (i = 0, r = 255; i<MAX_WIN_RETRY && (r&0x80) != 0; i++)
	port_in(WIN_REG8, &r);
  if ((r&0x80) != 0 || (r&0x40) == 0 || (r&0x10) == 0) {
	w_need_reset = TRUE;
	return(ERR);
  }
  return(OK);
}

/*============================================================================*
 *				com_out					      *
 *============================================================================*/
PRIVATE com_out()
{
/* Output the command block to the winchester controller and return status */

	register int i;
	int r;

	if (drive_busy()) {
		w_need_reset = TRUE;
		return(ERR);
	}
	r = WIN_REG2;
	lock();
	port_out(WIN_REG9, command[0]);
	for (i=1; i<8; i++, r++)
		port_out(r, command[i]);
	unlock();
	return(OK);
}

/*============================================================================*
 *				init_params				      *
 *============================================================================*/
PRIVATE init_params()
{
/* This routine is called at startup to initialize the partition table,
 * the number of drives and the controller
*/
  unsigned int i, segment, offset;
  phys_bytes address;
  extern phys_bytes umap();
  extern int vec_table[];

  /* Copy the parameter vector from the saved vector table */
  offset = vec_table[2 * 0x41];
  segment = vec_table[2 * 0x41 + 1];

  /* Calculate the address off the parameters and copy them to buf */
  address = ((long)segment << 4) + offset;
  phys_copy(address, umap(proc_addr(WINCHESTER), D, buf, 16), 16L);

  /* Copy the parameters to the structures */
  copy_param(buf, &wini[0]);

  /* Copy the parameter vector from the saved vector table */
  offset = vec_table[2 * 0x46];
  segment = vec_table[2 * 0x46 + 1];

  /* Calculate the address off the parameters and copy them to buf */
  address = ((long)segment << 4) + offset;
  phys_copy(address, umap(proc_addr(WINCHESTER), D, buf, 16), 16L);

  /* Copy the parameters to the structures */
  copy_param(buf, &wini[5]);

  /* Get the nummer of drives from the bios */
  phys_copy(0x475L, umap(proc_addr(WINCHESTER), D, buf, 1), 1L);
  nr_drives = (int) *buf;

  /* Set the parameters in the drive structure */
  wini[0].wn_low = wini[5].wn_low = 0L;

  /* Initialize the controller */
  if ((nr_drives > 0) && (win_init() != OK))
		nr_drives = 0;

  /* Read the partition table for each drive and save them */
  for (i = 0; i < nr_drives; i++) {
	w_mess.DEVICE = i * 5;
	w_mess.POSITION = 0L;
	w_mess.COUNT = BLOCK_SIZE;
	w_mess.ADDRESS = (char *) buf;
	w_mess.PROC_NR = WINCHESTER;
	w_mess.m_type = DISK_READ;
	if (w_do_rdwt(&w_mess) != BLOCK_SIZE)
		panic("Can't read partition table of winchester ", i);
	if (buf[510] != 0x55 || buf[511] != 0xAA) {
		printf("Invalid partition table\n");
		continue;
	}
	copy_prt(i*5);
  }
}

/*============================================================================*
 *				copy_params				      *
 *============================================================================*/
PRIVATE copy_params(src, dest)
register unsigned char *src;
register struct wini *dest;
{
/* This routine copies the parameters from src to dest
 * and sets the parameters for partition 0 and 5
*/
  register int i;
  long cyl, heads, sectors;

  for (i=0; i<5; i++) {
	dest[i].wn_heads = (int)src[2];
	dest[i].wn_precomp = *(int *)&src[5] / 4;
	dest[i].wn_ctlbyte = (int)src[10];
	dest[i].wn_maxsec = (int)src[14];
  }
  cyl = (long)(*(int *)src);
  heads = (long)dest[0].wn_heads;
  sectors = (long)dest[0].wn_maxsec;
  dest[0].wn_size = cyl * heads * sectors;
}

/*============================================================================*
 *				copy_prt				      *
 *============================================================================*/
PRIVATE copy_prt(drive)
int drive;
{
/* This routine copies the partition table for the selected drive to
 * the variables wn_low and wn_size
 */

  register int i, offset;
  struct wini *wn;
  long adjust;

  for (i=0; i<4; i++) {
	adjust = 0;
	wn = &wini[i + drive + 1];
	offset = PART_TABLE + i * 0x10;
	wn->wn_low = *(long *)&buf[offset];
	if ((wn->wn_low % (BLOCK_SIZE/SECTOR_SIZE)) != 0) {
		adjust = wn->wn_low;
		wn->wn_low = (wn->wn_low/(BLOCK_SIZE/SECTOR_SIZE)+1)*(BLOCK_SIZE/SECTOR_SIZE);
		adjust = wn->wn_low - adjust;
	}
	wn->wn_size = *(long *)&buf[offset + sizeof(long)] - adjust;
  }
  sort(&wini[drive + 1]);
}

sort(wn)
register struct wini *wn;
{
  register int i,j;

  for (i=0; i<4; i++)
	for (j=0; j<3; j++)
		if ((wn[j].wn_low == 0) && (wn[j+1].wn_low != 0))
			swap(&wn[j], &wn[j+1]);
		else if (wn[j].wn_low > wn[j+1].wn_low && wn[j+1].wn_low != 0)
			swap(&wn[j], &wn[j+1]);
}

swap(first, second)
register struct wini *first, *second;
{
  register struct wini tmp;

  tmp = *first;
  *first = *second;
  *second = tmp;
}

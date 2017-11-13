/* This file contains the printer driver. It is a fairly simple driver,
 * supporting only one printer.  Characters that are written to the driver
 * are written to the printer without any changes at all.
 *
 * The valid messages and their parameters are:
 *
 *   TTY_O_DONE:   output completed
 *   TTY_WRITE:    a process wants to write on a terminal
 *   CANCEL:       terminate a previous incomplete system call immediately
 *
 *    m_type      TTY_LINE   PROC_NR    COUNT    ADDRESS
 * -------------------------------------------------------
 * | TTY_O_DONE  |minor dev|         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | TTY_WRITE   |minor dev| proc nr |  count  | buf ptr |
 * |-------------+---------+---------+---------+---------|
 * | CANCEL      |minor dev| proc nr |         |         |
 * -------------------------------------------------------
 * 
 * Note: since only 1 printer is supported, minor dev is not used at present.
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

#define NORMAL_STATUS   0xDF	/* printer gives this status when idle */
#define BUSY_STATUS     0x5F	/* printer gives this status when busy */
#define ASSERT_STROBE   0x1D	/* strobe a character to the interface */
#define NEGATE_STROBE   0x1C	/* enable interrupt on interface */
#define SELECT          0x0C	/* select printer bit */
#define INIT_PRINTER    0x08	/* init printer bits */
#define NO_PAPER        0x20	/* status bit saying that paper is up */
#define OFF_LINE        0x10	/* status bit saying that printer not online*/
#define PR_ERROR        0x08	/* something is wrong with the printer */
#define PR_COLOR_BASE  0x378	/* printer port when color display used */
#define PR_MONO_BASE   0x3BC	/* printer port when mono display used */
#define LOW_FOUR         0xF	/* mask for low-order 4 bits */
#define CANCELED        -999	/* indicates that command has been killed */
#define DELAY_COUNT      100	/* regulates delay between characters */
#define DELAY_LOOP      1000	/* delay when printer is busy */
#define MAX_REP         1000	/* controls max delay when busy */

PRIVATE int port_base;		/* I/O port for printer: 0x 378 or 0x3BC */
PRIVATE int caller;		/* process to tell when printing done (FS) */
PRIVATE int proc_nr;		/* user requesting the printing */
PRIVATE int orig_count;		/* original byte count */
PRIVATE int es;			/* (es, offset) point to next character to */
PRIVATE int offset;		/* print, i.e., in the user's buffer */
PUBLIC int pcount;		/* number of bytes left to print */
PUBLIC int pr_busy;		/* TRUE when printing, else FALSE */
PUBLIC int cum_count;		/* cumulative # characters printed */
PUBLIC int prev_ct;		/* value of cum_count 100 msec ago */

/*===========================================================================*
 *				printer_task				     *
 *===========================================================================*/
PUBLIC printer_task()
{
/* Main routine of the printer task. */

  message print_mess;		/* buffer for all incoming messages */

  print_init();			/* initialize */

  while (TRUE) {
	receive(ANY, &print_mess);
	switch(print_mess.m_type) {
	    case TTY_WRITE:	do_write(&print_mess);	break;
	    case CANCEL   :	do_cancel(&print_mess);	break;
	    case TTY_O_DONE:	do_done(&print_mess);	break;
    	    default:					break;
	}
  }
}


/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
PRIVATE do_write(m_ptr)
message *m_ptr;			/* pointer to the newly arrived message */
{
/* The printer is used by sending TTY_WRITE messages to it. Process one. */

  int i, j, r, value;
  struct proc *rp;
  phys_bytes phys;
  extern phys_bytes umap();

  r = OK;			/* so far, no errors */

  /* Reject command if printer is busy or count is not positive. */
  if (pr_busy) r = EAGAIN;
  if (m_ptr->COUNT <= 0) r = EINVAL;

  /* Compute the physical address of the data buffer within user space. */
  rp = proc_addr(m_ptr->PROC_NR);
  phys = umap(rp, D, (vir_bytes) m_ptr->ADDRESS, m_ptr->COUNT);
  if (phys == 0) r = E_BAD_ADDR;

  if (r == OK) {
  	/* Save information needed later. */
	lock();			/* no interrupts now please */
  	caller = m_ptr->m_source;
  	proc_nr = m_ptr->PROC_NR;
  	pcount = m_ptr->COUNT;
  	orig_count = m_ptr->COUNT;
  	es = (int) (phys >> CLICK_SHIFT);
  	offset = (int) (phys & LOW_FOUR);

  	/* Start the printer. */
	for (i = 0; i < MAX_REP; i++) {
	  	port_in(port_base + 1, &value);
	  	if (value == NORMAL_STATUS) {
		 	pr_busy = TRUE;
			pr_char();	/* print first character */
			r = SUSPEND;	/* tell FS to suspend user until done */
			break;
		} else {
			if (value == BUSY_STATUS) {
				for (j = 0; j <DELAY_LOOP; j++) /* delay */ ;
				continue;
			}
			pr_error(value);
			r = EIO;
			break;
		}
	}
  }

  /* Reply to FS, no matter what happened. */
  if (value == BUSY_STATUS) r = EAGAIN;
  reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, r);
}


/*===========================================================================*
 *				do_done					     *
 *===========================================================================*/
PRIVATE do_done(m_ptr)
message *m_ptr;			/* pointer to the newly arrived message */
{
/* Printing is finished.  Reply to caller (FS). */

  int status;

  status = (m_ptr->REP_STATUS == OK ? orig_count : EIO);
  if (proc_nr != CANCELED) {
  	reply(REVIVE, caller, proc_nr, status);
  	if (status == EIO) pr_error(m_ptr->REP_STATUS);
  }
  pr_busy = FALSE;
}


/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
PRIVATE do_cancel(m_ptr)
message *m_ptr;			/* pointer to the newly arrived message */
{
/* Cancel a print request that has already started.  Usually this means that
 * the process doing the printing has been killed by a signal.
 */

  if (pr_busy == FALSE) return;	/* this statement avoids race conditions */
  pr_busy = FALSE;		/* mark printer as idle */
  pcount = 0;			/* causes printing to stop at next interrupt*/
  proc_nr = CANCELED;		/* marks process as canceled */
  reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, EINTR);
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE reply(code, replyee, process, status)
int code;			/* TASK_REPLY or REVIVE */
int replyee;			/* destination for message (normally FS) */
int process;			/* which user requested the printing */
int status;			/* number of  chars printed or error code */
{
/* Send a reply telling FS that printing has started or stopped. */

  message pr_mess;

  pr_mess.m_type = code;	/* TASK_REPLY or REVIVE */
  pr_mess.REP_STATUS = status;	/* count or EIO */
  pr_mess.REP_PROC_NR = process;	/* which user does this pertain to */
  send(replyee, &pr_mess);	/* send the message */
}


/*===========================================================================*
 *				pr_error				     *
 *===========================================================================*/
PRIVATE pr_error(status)
int status;			/* printer status byte */
{
/* The printer is not ready.  Display a message on the console telling why. */

  if (status & NO_PAPER) printf("Printer is out of paper\n");
  if ((status & OFF_LINE) == 0) printf("Printer is not on line\n");
  if ((status & PR_ERROR) == 0) printf("Printer error\n");
}


/*===========================================================================*
 *				print_init				     *
 *===========================================================================*/
PRIVATE print_init()
{
/* Color display uses 0x378 for printer; mono display uses 0x3BC. */

  int i;
  extern int color;

  port_base = (color ? PR_COLOR_BASE : PR_MONO_BASE);
  pr_busy = FALSE;
  port_out(port_base + 2, INIT_PRINTER);
  for (i = 0; i < DELAY_COUNT; i++) ;	/* delay loop */
  port_out(port_base + 2, SELECT);
}


/*===========================================================================*
 *				pr_char				     *
 *===========================================================================*/
PUBLIC pr_char()
{
/* This is the interrupt handler.  When a character has been printed, an
 * interrupt occurs, and the assembly code routine trapped to calls pr_char().
 * One annoying problem is that the 8259A controller sometimes generates
 * spurious interrupts to vector 15, which is the printer vector.  Ignore them.
 */

  int value, ch, i;
  char c;
  extern char get_byte();

  if (pcount != orig_count) port_out(INT_CTL, ENABLE);
  if (pr_busy == FALSE) return;	/* spurious 8259A interrupt */

  while (pcount > 0) {
  	port_in(port_base + 1, &value);	/* get printer status */
  	if (value == NORMAL_STATUS) {
		/* Everything is all right.  Output another character. */
		c = get_byte(es, offset);	/* fetch char from user buf */
		ch = c & BYTE;
		port_out(port_base, ch);	/* output character */
		port_out(port_base + 2, ASSERT_STROBE);
		port_out(port_base + 2, NEGATE_STROBE);
		offset++;
		pcount--;
		cum_count++;	/* count characters output */
		for (i = 0; i < DELAY_COUNT; i++) ;	/* delay loop */
	} else if (value == BUSY_STATUS) {
		 	return;		/* printer is busy; wait for interrupt*/
	} else {
		 	break;		/* err: send message to printer task */
	}
  }
  
/* Count is 0 or an error occurred; send message to printer task. */
  int_mess.m_type = TTY_O_DONE;
  int_mess.REP_STATUS = (pcount == 0 ? OK : value);
  interrupt(PRINTER, &int_mess);
}

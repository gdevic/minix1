/* FS must occasionally print some message.  It uses the standard library
 * routine printf(), which calls putc() and flush. Library
 * versions of these routines do printing by sending messages to FS.  Here
 * obviously can't do that, so FS calls the TTY task directly.
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/com.h"

#define STDOUTPUT          1	/* file descriptor for standard output */
#define BUFSIZE          100	/* print buffer size */

PRIVATE int bufcount;		/* # characters in the buffer */
PRIVATE char printbuf [BUFSIZE];	/* output is buffered here */
PRIVATE message putchmsg;	/* used for message to TTY task */

/*===========================================================================*
 *				putc					     *
 *===========================================================================*/
PUBLIC putc(c)
char c;
{

  if (c == 0) {
	flush();
	return;
  }
  printbuf[bufcount++] = c;
  if (bufcount == BUFSIZE) flush();
  if (c == '\n')  flush();
}


/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
PRIVATE flush()
{
/* Flush the print buffer. */


  if (bufcount == 0) return;
  putchmsg.m_type = TTY_WRITE;
  putchmsg.PROC_NR  = 1;
  putchmsg.TTY_LINE = 0;
  putchmsg.ADDRESS  = printbuf;
  putchmsg.COUNT = bufcount;
  sendrec(TTY, &putchmsg);
  bufcount = 0;
}

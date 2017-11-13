/* This file contains the terminal driver, both for the IBM console and regular
 * ASCII terminals.  It is split into two sections, a device-independent part
 * and a device-dependent part.  The device-independent part accepts
 * characters to be printed from programs and queues them in a standard way
 * for device-dependent output.  It also accepts input and queues it for
 * programs. This file contains 2 main entry points: tty_task() and keyboard().
 * When a key is struck on a terminal, an interrupt to an assembly language
 * routine is generated.  This routine saves the machine state and registers
 * and calls keyboard(), which enters the character in an internal table, and
 * then sends a message to the terminal task.  The main program of the terminal
 * task is tty_task(). It accepts not only messages about typed input, but
 * also requests to read and write from terminals, etc. 
 *
 * The device-dependent part interfaces with the IBM console and ASCII
 * terminals.  The IBM keyboard is unusual in that keystrokes yield key numbers
 * rather than ASCII codes, and furthermore, an interrupt is generated when a
 * key is depressed and again when it is released.  The IBM display is memory
 * mapped, so outputting characters such as line feed, backspace and bell are
 * tricky.
 *
 * The valid messages and their parameters are:
 *
 *   TTY_CHAR_INT: a character has been typed on a terminal (input interrupt)
 *   TTY_O_DONE:   a character has been output (output completed interrupt)
 *   TTY_READ:     a process wants to read from a terminal
 *   TTY_WRITE:    a process wants to write on a terminal
 *   TTY_IOCTL:    a process wants to change a terminal's parameters
 *   CANCEL:       terminate a previous incomplete system call immediately
 *
 *    m_type      TTY_LINE   PROC_NR    COUNT   TTY_SPEK  TTY_FLAGS  ADDRESS
 * ---------------------------------------------------------------------------
 * | TTY_CHAR_INT|         |         |         |         |         |array ptr|
 * |-------------+---------+---------+---------+---------+---------+---------|
 * | TTY_O_DONE  |minor dev|         |         |         |         |         |
 * |-------------+---------+---------+---------+---------+---------+---------|
 * | TTY_READ    |minor dev| proc nr |  count  |         |         | buf ptr |
 * |-------------+---------+---------+---------+---------+---------+---------|
 * | TTY_WRITE   |minor dev| proc nr |  count  |         |         | buf ptr |
 * |-------------+---------+---------+---------+---------+---------+---------|
 * | TTY_IOCTL   |minor dev| proc nr |func code|erase etc|  flags  |         |
 * |-------------+---------+---------+---------+---------+---------+---------|
 * | CANCEL      |minor dev| proc nr |         |         |         |         |
 * ---------------------------------------------------------------------------
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "../h/sgtty.h"
#include "../h/signal.h"
#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"

#define NR_TTYS            1	/* how many terminals can system handle */
#define TTY_IN_BYTES     200	/* input queue size */
#define TTY_RAM_WORDS    320	/* ram buffer size */
#define TTY_BUF_SIZE     256	/* unit for copying to/from queues */
#define TAB_SIZE           8	/* distance between tabs */
#define TAB_MASK          07	/* mask for tty_column when tabbing */
#define MAX_OVERRUN       16	/* size of overrun input buffer */

#define ERASE_CHAR      '\b'	/* default erase character */
#define KILL_CHAR        '@'	/* default kill character */
#define INTR_CHAR (char)0177	/* default interrupt character */
#define QUIT_CHAR (char) 034	/* default quit character */
#define XOFF_CHAR (char) 023	/* default x-off character (CTRL-S) */
#define XON_CHAR  (char) 021	/* default x-on character (CTRL-Q) */
#define EOT_CHAR  (char) 004	/* CTRL-D */
#define MARKER    (char) 000	/* non-escaped CTRL-D stored as MARKER */
#define DEL_CODE   (char) 83	/* DEL for use in CTRL-ALT-DEL reboot */
#define AT_SIGN         0220	/* code to yield for CTRL-@ */

#define F1                59	/* scan code for function key F1 */
#define F2                60	/* scan code for function key F2 */
#define F10               68	/* scan code for function key F9 */
#define TOP_ROW           14	/* codes below this are shifted if CTRL */

PRIVATE struct tty_struct {
  /* Input queue.  Typed characters are stored here until read by a program. */
  char tty_inqueue[TTY_IN_BYTES];    /* array used to store the characters */
  char *tty_inhead;		/* pointer to place where next char goes */
  char *tty_intail;		/* pointer to next char to be given to prog */
  int tty_incount;		/* # chars in tty_inqueue */
  int tty_lfct;			/* # line feeds in tty_inqueue */

  /* Output section. */
  int tty_ramqueue[TTY_RAM_WORDS];	/* buffer for video RAM */
  int tty_rwords;		/* number of WORDS (not bytes) in outqueue */
  int tty_org;			/* location in RAM where 6845 base points */
  int tty_vid;			/* current position of cursor in video RAM */
  char tty_esc_state;		/* 0=normal, 1 = ESC seen, 2 = ESC + x seen */
  char tty_echar;		/* first character following an ESC */
  int tty_attribute;		/* current attribute byte << 8 */
  int (*tty_devstart)();	/* routine to start actual device output */

  /* Terminal parameters and status. */
  int tty_mode;			/* terminal mode set by IOCTL */
  int tty_column;		/* current column number (0-origin) */
  int tty_row;			/* current row (0 at bottom of screen) */
  char tty_busy;		/* 1 when output in progress, else 0 */
  char tty_escaped;		/* 1 when '\' just seen, else 0 */
  char tty_inhibited;		/* 1 when CTRL-S just seen (stops output) */
  char tty_makebreak;		/* 1 for terminals that interrupt twice/key */
  char tty_waiting;		/* 1 when output process waiting for reply */

  /* User settable characters: erase, kill, interrupt, quit, x-on; x-off. */
  char tty_erase;		/* char used to erase 1 char (init ^H) */
  char tty_kill;		/* char used to erase a line (init @) */
  char tty_intr;		/* char used to send SIGINT  (init DEL) */
  char tty_quit;		/* char used for core dump   (init CTRL-\) */
  char tty_xon;			/* char used to start output (init CTRL-Q)*/
  char tty_xoff;		/* char used to stop output  (init CTRL-S) */
  char tty_eof;			/* char used to stop output  (init CTRL-D) */

  /* Information about incomplete I/O requests is stored here. */
  char tty_incaller;		/* process that made the call (usually FS) */
  char tty_inproc;		/* process that wants to read from tty */
  char *tty_in_vir;		/* virtual address where data is to go */
  int tty_inleft;		/* how many chars are still needed */
  char tty_otcaller;		/* process that made the call (usually FS) */
  char tty_outproc;		/* process that wants to write to tty */
  char *tty_out_vir;		/* virtual address where data comes from */
  phys_bytes tty_phys;		/* physical address where data comes from */
  int tty_outleft;		/* # chars yet to be copied to tty_outqueue */
  int tty_cum;			/* # chars copied to tty_outqueue so far */

  /* Miscellaneous. */
  int tty_ioport;		/* I/O port number for this terminal */
} tty_struct[NR_TTYS];

/* Values for the fields. */
#define NOT_ESCAPED        0	/* previous character on this line not '\' */
#define ESCAPED            1	/* previous character on this line was '\' */
#define RUNNING            0	/* no CRTL-S has been typed to stop the tty */
#define STOPPED            1	/* CTRL-S has been typed to stop the tty */
#define INACTIVE           0	/* the tty is not printing */
#define BUSY               1	/* the tty is printing */
#define ONE_INT            0	/* regular terminals interrupt once per char */
#define TWO_INTS           1	/* IBM console interrupts two times per char */
#define NOT_WAITING        0	/* no output process is hanging */
#define WAITING            1	/* an output process is waiting for a reply */

PRIVATE char tty_driver_buf[2*MAX_OVERRUN+2]; /* driver collects chars here */
PRIVATE char tty_copy_buf[2*MAX_OVERRUN];  /* copy buf used to avoid races */
PRIVATE char tty_buf[TTY_BUF_SIZE];	/* scratch buffer to/from user space */
PRIVATE int shift1, shift2, capslock, numlock;	/* keep track of shift keys */
PRIVATE int control, alt;	/* keep track of key statii */
PUBLIC  int color;		/* 1 if console is color, 0 if it is mono */
PUBLIC scan_code;		/* scan code for '=' saved by bootstrap */

/* Scan codes to ASCII for unshifted keys */
PRIVATE char unsh[] = {
 0,033,'1','2','3','4','5','6',        '7','8','9','0','-','=','\b','\t',
 'q','w','e','r','t','y','u','i',      'o','p','[',']',015,0202,'a','s',
 'd','f','g','h','j','k','l',';',      047,0140,0200,0134,'z','x','c','v',
 'b','n','m',',','.','/',0201,'*',     0203,' ',0204,0241,0242,0243,0244,0245,
 0246,0247,0250,0251,0252,0205,0210,0267,  0270,0271,0211,0264,0265,0266,0214
,0261,  0262,0263,'0',0177
};

/* Scan codes to ASCII for shifted keys */
PRIVATE char sh[] = {
 0,033,'!','@','#','$','%','^',        '&','*','(',')','_','+','\b','\t',
 'Q','W','E','R','T','Y','U','I',      'O','P','{','}',015,0202,'A','S',
 'D','F','G','H','J','K','L',':',      042,'~',0200,'|','Z','X','C','V',
 'B','N','M','<','>','?',0201,'*',    0203,' ',0204,0221,0222,0223,0224,0225,
 0226,0227,0230,0231,0232,0204,0213,'7',  '8','9',0211,'4','5','6',0214,'1',
 '2','3','0',177
};


/* Scan codes to ASCII for Olivetti M24 for unshifted keys. */
PRIVATE char unm24[] = {
 0,033,'1','2','3','4','5','6',        '7','8','9','0','-','^','\b','\t',
 'q','w','e','r','t','y','u','i',      'o','p','@','[','\r',0202,'a','s',
 'd','f','g','h','j','k','l',';',      ':',']',0200,'\\','z','x','c','v',
 'b','n','m',',','.','/',0201,'*',     0203,' ',0204,0241,0242,0243,0244,0245,
 0246,0247,0250,0251,0252,023,0210,0267,0270,0271,0211,0264,0265,0266,0214,0261,
0262,0263,'0','.',' ',014,0212,'\r',   0264,0262,0266,0270,032,0213,' ','/',
 0253,0254,0255,0256,0257,0215,0216,0217
};

/* Scan codes to ASCII for Olivetti M24 for shifted keys. */
PRIVATE char m24[] = {
 0,033,'!','"','#','$','%','&',        047,'(',')','_','=','~','\b','\t',
 'Q','W','E','R' ,'T','Y','U','I',     'O','P',0140,'{','\r',0202,'A','S',
 'D','F','G','H','J','K','L','+',      '*','}',0200,'|','Z','X','C','V',
 'B','N','M','<','>','?',0201,'*',     0203,' ',0204,0221,0222,0223,0224,0225,
 0226,0227,0230,0231,0232,0270,023,'7', '8','9',0211,'4','5','6',0214,'1',
 '2','3',0207,0177,0271,014,0272,'\r', '\b','\n','\f',036,032,0273,0274,'/',
 0233,0234,0235,0236,0237,0275,0276,0277
};


/*===========================================================================*
 *				tty_task				     *
 *===========================================================================*/
PUBLIC tty_task()
{
/* Main routine of the terminal task. */

  message tty_mess;		/* buffer for all incoming messages */
  register struct tty_struct *tp;

  tty_init();			/* initialize */
  while (TRUE) {
	receive(ANY, &tty_mess);
	tp = &tty_struct[tty_mess.TTY_LINE];
	switch(tty_mess.m_type) {
	    case TTY_CHAR_INT:	do_charint(&tty_mess);		break;
	    case TTY_READ:	do_read(tp, &tty_mess);		break;
	    case TTY_WRITE:	do_write(tp, &tty_mess);	break;
	    case TTY_IOCTL:	do_ioctl(tp, &tty_mess);	break;
	    case CANCEL   :	do_cancel(tp, &tty_mess);	break;
	    case TTY_O_DONE:	/* reserved for future use (RS-232 terminals)*/
	    default:		tty_reply(TASK_REPLY, tty_mess.m_source, 
					tty_mess.PROC_NR, EINVAL, 0L, 0L);
	}
  }
}


/*===========================================================================*
 *				do_charint				     *
 *===========================================================================*/
PRIVATE do_charint(m_ptr)
message *m_ptr;			/* message containing pointer to char(s) */
{
/* A character has been typed.  If a character is typed and the tty task is
 * not able to service it immediately, the character is accumulated within
 * the tty driver.  Thus multiple chars may be accumulated.  A single message
 * to the tty task may have to process several characters.
 */

  int m, n, count, replyee, caller;
  char *ptr, *copy_ptr, ch;
  struct tty_struct *tp;

  lock();			/* prevent races by disabling interrupts */
  ptr = m_ptr->ADDRESS;		/* pointer to accumulated char array */
  copy_ptr = tty_copy_buf;	/* ptr to shadow array where chars copied */
  n = *ptr;			/* how many chars have been accumulated */
  count = n;			/* save the character count */
  n = n + n;			/* each char occupies 2 bytes */
  ptr += 2;			/* skip count field at start of array */
  while (n-- > 0)
	*copy_ptr++ = *ptr++;	/* copy the array to safety */
  ptr = m_ptr->ADDRESS;
  *ptr = 0;			/* accumulation count set to 0 */
  unlock();			/* re-enable interrupts */

  /* Loop on the accumulated characters, processing each in turn. */
  copy_ptr = tty_copy_buf;
  while (count-- > 0) {
	ch = *copy_ptr++;	/* get the character typed */
	n = *copy_ptr++;	/* get the line number it came in on */
	in_char(n, ch);		/* queue the char and echo it */

	/* See if a previously blocked reader can now be satisfied. */
	tp = &tty_struct[n];	/* pointer to struct for this character */
	if (tp->tty_inleft > 0 ) {	/* does anybody want input? */
		m = tp->tty_mode & (CBREAK | RAW);
		if (tp->tty_lfct > 0 || (m != 0 && tp->tty_incount > 0)) {
			m = rd_chars(tp);

			/* Tell hanging reader that chars have arrived. */
			replyee = (int) tp->tty_incaller;
			caller = (int) tp->tty_inproc;
			tty_reply(REVIVE, replyee, caller, m, 0L, 0L);
		}
	}
  }
}


/*===========================================================================*
 *				in_char					     *
 *===========================================================================*/
PRIVATE in_char(line, ch)
int line;			/* line number on which char arrived */
char ch;			/* scan code for character that arrived */
{
/* A character has just been typed in.  Process, save, and echo it. */

  register struct tty_struct *tp;
  int mode, sig;
  char make_break();
  tp = &tty_struct[line];	/* set 'tp' to point to proper struct */
  /* Function keys are temporarily being used for debug dumps. */
  if (ch >= F1 && ch <= F10) {	/* Check for function keys F1, F2, ... F10 */
	func_key(ch);		/* process function key */
	return;
  }
  if (tp->tty_incount >= TTY_IN_BYTES) return;	/* no room, discard char */
  mode = tp->tty_mode & (RAW | CBREAK);
  if (tp->tty_makebreak)
	ch = make_break(ch);	/* console give 2 ints/ch */
  else
	if (mode != RAW) ch &= 0177;	/* 7-bit chars except in raw mode */
  if (ch == 0) return;

  /* Processing for COOKED and CBREAK mode contains special checks. */
  if (mode == COOKED || mode == CBREAK) {
	/* Handle erase, kill and escape processing. */
	if (mode == COOKED) {
		/* First erase processing (rub out of last character). */
		if (ch == tp->tty_erase && tp->tty_escaped == NOT_ESCAPED) {
			chuck(tp);	/* remove last char entered */
			echo(tp, '\b');	/* remove it from the screen */
			echo(tp, ' ');
			echo(tp, '\b');
			return;
		}

		/* Now do kill processing (remove current line). */
		if (ch == tp->tty_kill && tp->tty_escaped == NOT_ESCAPED) {
			while( chuck(tp) == OK) /* keep looping */ ;
			echo(tp, tp->tty_kill);
			echo (tp, '\n');
			return;
		}

		/* Handle EOT and the escape symbol (backslash). */
		if (tp->tty_escaped == NOT_ESCAPED) {
			/* Normal case: previous char was not backslash. */
			if (ch == '\\') {
				/* An escaped symbol has just been typed. */
				tp->tty_escaped = ESCAPED;
				echo(tp, ch);
				return;	/* do not store the '\' */
			}
			/* CTRL-D means end-of-file, unless it is escaped. It
			 * is stored in the text as MARKER, and counts as a
			 * line feed in terms of knowing whether a full line
			 * has been typed already.
			 */
			if (ch == tp->tty_eof) ch = MARKER;
		} else {
			/* Previous character was backslash. */
			tp->tty_escaped = NOT_ESCAPED;	/* turn escaping off */
			if (ch != tp->tty_erase && ch != tp->tty_kill &&
						   ch != tp->tty_eof) {
				/* Store the escape previously skipped over */
				*tp->tty_inhead++ = '\\';
				tp->tty_incount++;
				if (tp->tty_inhead ==
						&tp->tty_inqueue[TTY_IN_BYTES])
					tp->tty_inhead = tp->tty_inqueue;
			}
		}
	}
	/* Both COOKED and CBREAK modes come here; first map CR to LF. */
	if (ch == '\r' && (tp->tty_mode & CRMOD)) ch = '\n';

	/* Check for interrupt and quit characters. */
	if (ch == tp->tty_intr || ch == tp->tty_quit) {
		sig = (ch == tp->tty_intr ? SIGINT : SIGQUIT);
		tp->tty_inhibited = RUNNING;	/* do implied CRTL-Q */
		finish(tp, EINTR);		/* send reply */
		tp->tty_inhead = tp->tty_inqueue;	/* discard input */
		tp->tty_intail = tp->tty_inqueue;
		tp->tty_incount = 0;
		tp->tty_lfct = 0;
		cause_sig(LOW_USER + 1 + line, sig);
		return;
	}

	/* Check for and process CTRL-S (terminal stop). */
	if (ch == tp->tty_xoff) {
		tp->tty_inhibited = STOPPED;
		return;
	}

	/* Check for and process CTRL-Q (terminal start). */
	if (ch == tp->tty_xon) {
		tp->tty_inhibited = RUNNING;
		(*tp->tty_devstart)(tp);	/* resume output */
		return;
	}
  }

  /* All 3 modes come here. */
  if (ch == '\n' || ch == MARKER) tp->tty_lfct++;	/* count line feeds */
  *tp->tty_inhead++ = ch;	/* save the character in the input queue */
  if (tp->tty_inhead == &tp->tty_inqueue[TTY_IN_BYTES])
	tp->tty_inhead = tp->tty_inqueue;	/* handle wraparound */
  tp->tty_incount++;
  echo(tp, ch);
}


#ifdef i8088
/*===========================================================================*
 *				make_break				     *
 *===========================================================================*/
PRIVATE char make_break(ch)
char ch;			/* scan code of key just struck or released */
{
/* This routine can handle keyboards that interrupt only on key depression,
 * as well as keyboards that interrupt on key depression and key release.
 * For efficiency, the interrupt routine filters out most key releases.
 */

  int c, make, code;


  c = ch & 0177;		/* high-order bit set on key release */
  make = (ch & 0200 ? 0 : 1);	/* 1 when key depressed, 0 when key released */
  if (olivetti == FALSE) {
	/* Standard IBM keyboard. */
	code = (shift1 || shift2 || capslock ? sh[c] : unsh[c]);
	if (control && c < TOP_ROW) code = sh[c];	/* CTRL-(top row) */
	if (c > 70 && numlock) code = sh[c];	/* numlock depressed */
  } else {
	/* (Olivetti M24 or AT&T 6300) with Olivetti-style keyboard. */
	code = (shift1 || shift2 || capslock ? m24[c] : unm24[c]);
	if (control && c < TOP_ROW) code = sh[c];	/* CTRL-(top row) */
	if (c > 70 && numlock) code = m24[c];	/* numlock depressed */
  }
  code &= BYTE;
  if (code < 0200 || code >= 0206) {
	/* Ordinary key, i.e. not shift, control, alt, etc. */
	if (alt) code |= 0200;	/* alt key ORs 0200 into code */
	if (control) code &= 037;
	if (code == 0) code = AT_SIGN;	/* @ is 0100, so CTRL-@ = 0 */
	if (make == 0) code = 0;	/* key release */
	return(code);
  }

  /* Table entries 0200 - 0206 denote special actions. */
  switch(code - 0200) {
    case 0:	shift1 = make;		break;	/* shift key on left */
    case 1:	shift2 = make;		break;	/* shift key on right */
    case 2:	control = make;		break;	/* control */
    case 3:	alt = make;		break;	/* alt key */
    case 4:	if (make) capslock = 1 - capslock; break;	/* caps lock */
    case 5:	if (make) numlock  = 1 - numlock;  break;	/* num lock */
  }
  return(0);
}
#endif


/*===========================================================================*
 *				echo					     *
 *===========================================================================*/
PRIVATE echo(tp, c)
register struct tty_struct *tp;	/* terminal on which to echo */
register char c;		/* character to echo */
{
/* Echo a character on the terminal. */

  if ( (tp->tty_mode & ECHO) == 0) return;	/* if no echoing, don't echo */
  if (c != MARKER) out_char(tp, c);
  flush(tp);			/* force character out onto the screen */
}


/*===========================================================================*
 *				chuck					     *
 *===========================================================================*/
PRIVATE int chuck(tp)
register struct tty_struct *tp;	/* from which tty should chars be removed */
{
/* Delete one character from the input queue.  Used for erase and kill. */

  char *prev;

  /* If input queue is empty, don't delete anything. */
  if (tp->tty_incount == 0) return(-1);

  /* Don't delete '\n' or '\r'. */
  prev = (tp->tty_inhead != tp->tty_inqueue ? tp->tty_inhead - 1 :
					     &tp->tty_inqueue[TTY_IN_BYTES-1]);
  if (*prev == '\n' || *prev == '\r') return(-1);
  tp->tty_inhead = prev;
  tp->tty_incount--;
  return(OK);			/* char erasure was possible */
}


/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
PRIVATE do_read(tp, m_ptr)
register struct tty_struct *tp;	/* pointer to tty struct */
message *m_ptr;			/* pointer to message sent to the task */
{
/* A process wants to read from a terminal. */

  int code, caller;

  if (tp->tty_inleft > 0) {	/* if someone else is hanging, give up */
	tty_reply(TASK_REPLY,m_ptr->m_source,m_ptr->PROC_NR, E_TRY_AGAIN,0L,0L);
	return;
  }

  /* Copy information from the message to the tty struct. */
  tp->tty_incaller = m_ptr->m_source;
  tp->tty_inproc = m_ptr->PROC_NR;
  tp->tty_in_vir = m_ptr->ADDRESS;
  tp->tty_inleft = m_ptr->COUNT;

  /* Try to get chars.  This call either gets enough, or gets nothing. */
  code = rd_chars(tp);
  caller = (int) tp->tty_inproc;
  tty_reply(TASK_REPLY, m_ptr->m_source, caller, code, 0L, 0L);
}


/*===========================================================================*
 *				rd_chars				     *
 *===========================================================================*/
PRIVATE int rd_chars(tp)
register struct tty_struct *tp;	/* pointer to terminal to read from */
{
/* A process wants to read from a terminal.  First check if enough data is
 * available. If so, pass it to the user.  If not, send FS a message telling
 * it to suspend the user.  When enough data arrives later, the tty driver
 * copies it to the user space directly and notifies FS with a message.
 */

  int cooked, ct, user_ct, buf_ct, cum, enough, eot_seen;
  vir_bytes in_vir, left;
  phys_bytes user_phys, tty_phys;
  char ch, *tty_ptr;
  struct proc *rp;
  extern phys_bytes umap();

  cooked = ( (tp->tty_mode & (RAW | CBREAK)) ? 0 : 1);	/* 1 iff COOKED mode */
  if (tp->tty_incount == 0 || (cooked && tp->tty_lfct == 0)) return(SUSPEND);
  rp = proc_addr(tp->tty_inproc);
  in_vir = (vir_bytes) tp-> tty_in_vir;
  left = (vir_bytes) tp->tty_inleft;
  if ( (user_phys = umap(rp, D, in_vir, left)) == 0) return(E_BAD_ADDR);
  tty_phys = umap(proc_addr(TTY), D, (vir_bytes) tty_buf, TTY_BUF_SIZE);
  cum = 0;
  enough = 0;
  eot_seen = 0;

  /* The outer loop iterates on buffers, one buffer load per iteration. */
  while (tp->tty_inleft > 0) {
	buf_ct = MIN(tp->tty_inleft, tp->tty_incount);
	buf_ct = MIN(buf_ct, TTY_BUF_SIZE);
	ct = 0;
	tty_ptr = tty_buf;

	/* The inner loop fills one buffer. */
	while(buf_ct-- > 0) {
		ch = *tp->tty_intail++;
		if (tp->tty_intail == &tp->tty_inqueue[TTY_IN_BYTES])
			tp->tty_intail = tp->tty_inqueue;
		*tty_ptr++ = ch;
		ct++;
		if (ch == '\n' || ch == MARKER) {
			tp->tty_lfct--;
			if (cooked && ch == MARKER) eot_seen++;
			enough++;	/* exit loop */
			if (cooked) break;	/* only provide 1 line */
		}
	}

	/* Copy one buffer to user space.  Be careful about CTRL-D.  In cooked
	 * mode it is not transmitted to user programs, and is not counted as
	 * a character as far as the count goes, but it does occupy space in 
	 * the driver's tables and must be counted there.
	 */
	user_ct = (eot_seen ? ct - 1 : ct);	/* bytes to copy to user */
	phys_copy(tty_phys, user_phys, (phys_bytes) user_ct);
	user_phys += user_ct;
	cum += user_ct;
	tp->tty_inleft -= ct;
	tp->tty_incount -= ct;
	if (tp->tty_incount == 0 || enough) break;
  }

  tp->tty_inleft = 0;
  return(cum);
}


/*===========================================================================*
 *				finish					     *
 *===========================================================================*/
PRIVATE finish(tp, code)
register struct tty_struct *tp;	/* pointer to tty struct */
int code;			/* reply code */
{
/* A command has terminated (possibly due to DEL).  Tell caller. */

  int replyee, caller;

  tp->tty_rwords = 0;
  tp->tty_outleft = 0;
  if (tp->tty_waiting == NOT_WAITING) return;
  replyee = (int) tp->tty_otcaller;
  caller = (int) tp->tty_outproc;
  tty_reply(TASK_REPLY, replyee, caller, code, 0L, 0L);
  tp->tty_waiting = NOT_WAITING;
}


/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
PRIVATE do_write(tp, m_ptr)
register struct tty_struct *tp;	/* pointer to tty struct */
message *m_ptr;			/* pointer to message sent to the task */
{
/* A process wants to write on a terminal. */

  vir_bytes out_vir, out_left;
  struct proc *rp;
  extern phys_bytes umap();

  /* Copy message parameters to the tty structure. */
  tp->tty_otcaller = m_ptr->m_source;
  tp->tty_outproc = m_ptr->PROC_NR;
  tp->tty_out_vir = m_ptr->ADDRESS;
  tp->tty_outleft = m_ptr->COUNT;
  tp->tty_waiting = WAITING;
  tp->tty_cum = 0;

  /* Compute the physical address where the data is in user space. */
  rp = proc_addr(tp->tty_outproc);
  out_vir = (vir_bytes) tp->tty_out_vir;
  out_left = (vir_bytes) tp->tty_outleft;
  if ( (tp->tty_phys = umap(rp, D, out_vir, out_left)) == 0) {
	/* Buffer address provided by user is outside its address space. */
	tp->tty_cum = E_BAD_ADDR;
	tp->tty_outleft = 0;
  }

  /* Copy characters from the user process to the terminal. */
  (*tp->tty_devstart)(tp);	/* copy data to queue and start I/O */
}


/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
PRIVATE do_ioctl(tp, m_ptr)
register struct tty_struct *tp;	/* pointer to tty_struct */
message *m_ptr;			/* pointer to message sent to task */
{
/* Perform IOCTL on this terminal. */

  long flags, erki, erase, kill, intr, quit, xon, xoff, eof;
  int r;

  r = OK;
  flags = 0;
  erki = 0;
  switch(m_ptr->TTY_REQUEST) {
     case TIOCSETP:
	/* Set erase, kill, and flags. */
	tp->tty_erase = (char) ((m_ptr->TTY_SPEK >> 8) & BYTE);	/* erase  */
	tp->tty_kill  = (char) ((m_ptr->TTY_SPEK >> 0) & BYTE);	/* kill  */
	tp->tty_mode  = (int) m_ptr->TTY_FLAGS;	/* mode word */
	break;

     case TIOCSETC:
	/* Set intr, quit, xon, xoff, eof (brk not used). */
	tp->tty_intr = (char) ((m_ptr->TTY_SPEK >> 24) & BYTE);	/* interrupt */
	tp->tty_quit = (char) ((m_ptr->TTY_SPEK >> 16) & BYTE);	/* quit */
	tp->tty_xon  = (char) ((m_ptr->TTY_SPEK >>  8) & BYTE);	/* CTRL-S */
	tp->tty_xoff = (char) ((m_ptr->TTY_SPEK >>  0) & BYTE);	/* CTRL-Q */
	tp->tty_eof  = (char) ((m_ptr->TTY_FLAGS >> 8) & BYTE);	/* CTRL-D */
	break;

     case TIOCGETP:
	/* Get erase, kill, and flags. */
	erase = ((long) tp->tty_erase) & BYTE;
	kill  = ((long) tp->tty_kill) & BYTE;
	erki  = (erase << 8) | kill;
	flags = (long) tp->tty_mode;
	break;

     case TIOCGETC:
	/* Get intr, quit, xon, xoff, eof. */
	intr  = ((long) tp->tty_intr) & BYTE;
	quit  = ((long) tp->tty_quit) & BYTE;
	xon   = ((long) tp->tty_xon)  & BYTE;
	xoff  = ((long) tp->tty_xoff) & BYTE;
	eof   = ((long) tp->tty_eof)  & BYTE;
	erki  = (intr << 24) | (quit << 16) | (xon << 8) | (xoff << 0);
	flags = (eof <<8);
	break;

     default:
	r = EINVAL;
  }

  /* Send the reply. */
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, r, flags, erki);
}


/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
PRIVATE do_cancel(tp, m_ptr)
register struct tty_struct *tp;	/* pointer to tty_struct */
message *m_ptr;			/* pointer to message sent to task */
{
/* A signal has been sent to a process that is hanging trying to read or write.
 * The pending read or write must be finished off immediately.
 */

  /* First check to see if the process is indeed hanging.  If it is not, don't
   * reply (to avoid race conditions).
   */
  if (tp->tty_inleft == 0 && tp->tty_outleft == 0) return;

  /* Kill off input and output. */
  tp->tty_inhead = tp->tty_inqueue;	/* discard all input */
  tp->tty_intail = tp->tty_inqueue;
  tp->tty_incount = 0;
  tp->tty_lfct = 0;
  tp->tty_inleft = 0;
  tp->tty_outleft = 0;
  tp->tty_waiting = NOT_WAITING;	/* don't send reply */
  tp->tty_inhibited = RUNNING;
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, EINTR, 0L, 0L);
}


/*===========================================================================*
 *				tty_reply				     *
 *===========================================================================*/
PRIVATE tty_reply(code, replyee, proc_nr, status, extra, other)
int code;			/* TASK_REPLY or REVIVE */
int replyee;			/* destination address for the reply */
int proc_nr;			/* to whom should the reply go? */
int status;			/* reply code */
long extra;			/* extra value */
long other;			/* used for IOCTL replies */
{
/* Send a reply to a process that wanted to read or write data. */

  message tty_mess;

  tty_mess.m_type = code;
  tty_mess.REP_PROC_NR = proc_nr;
  tty_mess.REP_STATUS = status;
  tty_mess.TTY_FLAGS = extra;	/* used by IOCTL for flags (mode) */
  tty_mess.TTY_SPEK = other;	/* used by IOCTL for erase and kill chars */
  send(replyee, &tty_mess);
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

#ifdef i8088
/* Now begins the code and data for the device-dependent tty drivers. */

/* Definitions used by the console driver. */
#define COLOR_BASE    0xB800	/* video ram paragraph for color display */
#define MONO_BASE     0xB000	/* video ram address for mono display */
#define C_VID_MASK    0x3FFF	/* mask for 16K video RAM */
#define M_VID_MASK    0x0FFF	/* mask for  4K video RAM */
#define C_RETRACE     0x0300	/* how many characters to display at once */
#define M_RETRACE     0x7000	/* how many characters to display at once */
#define WORD_MASK     0xFFFF	/* mask for 16 bits */
#define OFF_MASK      0x000F	/* mask for  4 bits */
#define BEEP_FREQ     0x0533	/* value to put into timer to set beep freq */
#define B_TIME        0x2000	/* how long to sound the CTRL-G beep tone */
#define BLANK         0x0700	/* determines  cursor color on blank screen */
#define LINE_WIDTH        80	/* # characters on a line */
#define SCR_LINES         25	/* # lines on the screen */
#define CTRL_S            31	/* scan code for letter S (for CRTL-S) */
#define MONOCHROME         1	/* value for tty_ioport tells color vs. mono */
#define CONSOLE            0	/* line number for console */
#define GO_FORWARD         0	/* scroll forward */
#define GO_BACKWARD        1	/* scroll backward */
#define TIMER2          0x42	/* I/O port for timer channel 2 */
#define TIMER3          0x43	/* I/O port for timer channel 3 */
#define KEYBD           0x60	/* I/O port for keyboard data */
#define PORT_B          0x61	/* I/O port for 8255 port B */
#define KBIT            0x80	/* bit used to ack characters to keyboard */

/* Constants relating to the video RAM and 6845. */
#define M_6845         0x3B0	/* port for 6845 mono */
#define C_6845         0x3D0	/* port for 6845 color */
#define INDEX              4	/* 6845's index register */
#define DATA               5	/* 6845's data register */
#define CUR_SIZE          10	/* 6845's cursor size register */
#define VID_ORG           12	/* 6845's origin register */
#define CURSOR            14	/* 6845's cursor register */

/* Definitions used for determining if the keyboard is IBM or Olivetti type. */
#define KB_STATUS	0x64	/* Olivetti keyboard status port */
#define BYTE_AVAIL	0x01	/* there is something in KEYBD port */
#define KB_BUSY	        0x02	/* KEYBD port ready to accept a command */
#define DELUXE		0x01	/* this bit is set up iff deluxe keyboard */
#define GET_TYPE	   5	/* command to get keyboard type */
#define OLIVETTI_EQUAL    12	/* the '=' key is 12 on olivetti, 13 on IBM */

/* Global variables used by the console driver. */
PUBLIC  message keybd_mess;	/* message used for console input chars */
PRIVATE vid_retrace;		/* how many characters to display per burst */
PRIVATE unsigned vid_base;	/* base of video ram (0xB000 or 0xB800) */
PUBLIC int vid_mask;		/* 037777 for color (16K) or 07777 for mono */
PRIVATE int vid_port;		/* I/O port for accessing 6845 */


/*===========================================================================*
 *				keyboard				     *
 *===========================================================================*/
PUBLIC keyboard()
{
/* A keyboard interrupt has occurred.  Process it. */

  int val, code, k, raw_bit;
  char stopc;

  /* Fetch the character from the keyboard hardware and acknowledge it. */
  port_in(KEYBD, &code);	/* get the scan code for the key struck */
  port_in(PORT_B, &val);	/* strobe the keyboard to ack the char */
  port_out(PORT_B, val | KBIT);	/* strobe the bit high */
  port_out(PORT_B, val);	/* now strobe it low */

  /* The IBM keyboard interrupts twice per key, once when depressed, once when
   * released.  Filter out the latter, ignoring all but the shift-type keys.
   * The shift-type keys, 29, 42, 54, 56, and 69 must be processed normally.
   */
  k = code - 0200;		/* codes > 0200 mean key release */
  if (k > 0) {
	/* A key has been released. */
	if (k != 29 && k != 42 && k != 54 && k != 56 && k != 69) {
		port_out(INT_CTL, ENABLE);	/* re-enable interrupts */
	 	return;		/* don't call tty_task() */
	}
  } else {
	/* Check to see if character is CTRL-S, to stop output. Setting xoff
	 * to anything other than CTRL-S will not be detected here, but will
	 * be detected later, in the driver.  A general routine to detect any
	 * xoff character here would be complicated since we only have the
	 * scan code here, not the ASCII character.
	 */
	raw_bit = tty_struct[CONSOLE].tty_mode & RAW;
	stopc = tty_struct[CONSOLE].tty_xoff;
	if (raw_bit == 0 && control && code == CTRL_S && stopc == XOFF_CHAR) {
		tty_struct[CONSOLE].tty_inhibited = STOPPED;
		port_out(INT_CTL, ENABLE);
		return;
	}
  }

  /* Check for CTRL-ALT-DEL, and if found, reboot the computer. */
  if (control && alt && code == DEL_CODE) reboot();	/* CTRL-ALT-DEL */

  /* Store the character in memory so the task can get at it later. */
  if ( (k = tty_driver_buf[0]) < tty_driver_buf[1]) {
	/* There is room to store this character; do it. */
	k = k + k;			/* each entry contains two bytes */
	tty_driver_buf[k+2] = code;	/* store the scan code */
	tty_driver_buf[k+3] = CONSOLE;	/* tell which line it came from */
	tty_driver_buf[0]++;		/* increment counter */

	/* Build and send the interrupt message. */
	keybd_mess.m_type = TTY_CHAR_INT;
	keybd_mess.ADDRESS = tty_driver_buf;
	interrupt(TTY, &keybd_mess);	/* send a message to the tty task */
  } else {
	/* Too many characters have been buffered.  Discard excess. */
	port_out(INT_CTL, ENABLE);	/* re-enable 8259A controller */
  }
}


/*===========================================================================*
 *				console					     *
 *===========================================================================*/
PRIVATE console(tp)
register struct tty_struct *tp;	/* tells which terminal is to be used */
{
/* Copy as much data as possible to the output queue, then start I/O.  On
 * memory-mapped terminals, such as the IBM console, the I/O will also be
 * finished, and the counts updated.  Keep repeating until all I/O done.
 */

  int count;
  char c;
  unsigned segment, offset, offset1;

  /* Loop over the user bytes one at a time, outputting each one. */
  segment = (tp->tty_phys >> 4) & WORD_MASK;
  offset = tp->tty_phys & OFF_MASK;
  offset1 = offset;
  count = 0;

  while (tp->tty_outleft > 0 && tp->tty_inhibited == RUNNING) {
	c = get_byte(segment, offset);	/* fetch 1 byte from user space */
	out_char(tp, c);	/* write 1 byte to terminal */
	offset++;		/* advance one character in user buffer */
	tp->tty_outleft--;	/* decrement count */
  }
  flush(tp);			/* clear out the pending characters */

  /* Update terminal data structure. */
  count = offset - offset1;	/* # characters printed */
  tp->tty_phys += count;	/* advance physical data pointer */
  tp->tty_cum += count;		/* number of characters printed */

  /* If all data has been copied to the terminal, send the reply. */
  if (tp->tty_outleft == 0) finish(tp, tp->tty_cum);
}


/*===========================================================================*
 *				out_char				     *
 *===========================================================================*/
PRIVATE out_char(tp, c)
register struct tty_struct *tp;	/* pointer to tty struct */
char c;				/* character to be output */
{
/* Output a character on the console. Check for escape sequences, including
 *   ESC 32+x 32+y to move cursor to (x, y)
 *   ESC ~ 0       to clear from cursor to end of screen
 *   ESC ~ 1       to reverse scroll the screen 1 line
 *   ESC z x       to set the attribute byte to x (z is a literal here)
 */

  /* Check to see if we are part way through an escape sequence. */
  if (tp->tty_esc_state == 1) {
	tp->tty_echar = c;
	tp->tty_esc_state = 2;
	return;
  }

  if (tp->tty_esc_state == 2) {
	escape(tp, tp->tty_echar, c);
	tp->tty_esc_state = 0;
	return;
  }

  switch(c) {
	case 007:		/* ring the bell */
		flush(tp);	/* print any chars queued for output */
		beep(BEEP_FREQ);/* BEEP_FREQ gives bell tone */
		return;

	case 013:		/* CTRL-K */
		move_to(tp, tp->tty_column, tp->tty_row + 1);
		return;

	case 014:		/* CTRL-L */
		move_to(tp, tp->tty_column + 1, tp->tty_row);
		return;

	case 016:		/* CTRL-N */
		move_to(tp, tp->tty_column + 1, tp->tty_row);
		return;

	case '\b':		/* backspace */
		move_to(tp, tp->tty_column - 1, tp->tty_row);
		return;

	case '\n':		/* line feed */
		if (tp->tty_mode & CRMOD) out_char(tp, '\r');
		if (tp->tty_row == 0) 
			scroll_screen(tp, GO_FORWARD);
		else
			tp->tty_row--;
		move_to(tp, tp->tty_column, tp->tty_row);
		return;

	case '\r':		/* carriage return */
		move_to(tp, 0, tp->tty_row);
		return;

	case '\t':		/* tab */
		if ( (tp->tty_mode & XTABS) == XTABS) {
			do {
				out_char(tp, ' ');
			} while (tp->tty_column & TAB_MASK);
			return;
		}
		/* Ignore tab is XTABS is off--video RAM has no hardware tab */
		return;

	case 033:		/* ESC - start of an escape sequence */
		flush(tp);	/* print any chars queued for output */
		tp->tty_esc_state = 1;	/* mark ESC as seen */
		return;

	default:		/* printable chars are stored in ramqueue */
		if (tp->tty_column >= LINE_WIDTH) return;	/* long line */
		if (tp->tty_rwords == TTY_RAM_WORDS) flush(tp);
		tp->tty_ramqueue[tp->tty_rwords++] = tp->tty_attribute | c;
		tp->tty_column++;	/* next column */
		return;
  }
}


/*===========================================================================*
 *				scroll_screen				     *
 *===========================================================================*/
PRIVATE scroll_screen(tp, dir)
register struct tty_struct *tp;	/* pointer to tty struct */
int dir;			/* GO_FORWARD or GO_BACKWARD */
{
  int amount, offset;

  amount = (dir == GO_FORWARD ? 2 * LINE_WIDTH : -2 * LINE_WIDTH);
  tp->tty_org = (tp->tty_org + amount) & vid_mask;
  if (dir == GO_FORWARD)
	offset = (tp->tty_org + 2 * (SCR_LINES - 1) * LINE_WIDTH) & vid_mask;
  else
	offset = tp->tty_org;

  /* Blank the new line at top or bottom. */
  vid_copy(NIL_PTR, vid_base, offset, LINE_WIDTH);
  set_6845(VID_ORG, tp->tty_org >> 1);	/* 6845 thinks in words */
}


/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
PRIVATE flush(tp)
register struct tty_struct *tp;	/* pointer to tty struct */
{
/* Have the characters in 'ramqueue' transferred to the screen. */

  if (tp->tty_rwords == 0) return;
  vid_copy(tp->tty_ramqueue, vid_base, tp->tty_vid, tp->tty_rwords);

  /* Update the video parameters and cursor. */
  tp->tty_vid = (tp->tty_vid + 2 * tp->tty_rwords);
  set_6845(CURSOR, tp->tty_vid >> 1);	/* cursor counts in words */
  tp->tty_rwords = 0;
}


/*===========================================================================*
 *				move_to					     *
 *===========================================================================*/
PRIVATE move_to(tp, x, y)
struct tty_struct *tp;		/* pointer to tty struct */
int x;				/* column (0 <= x <= 79) */
int y;				/* row (0 <= y <= 24, 0 at bottom) */
{
/* Move the cursor to (x, y). */

  flush(tp);			/* flush any pending characters */
  if (x < 0 || x >= LINE_WIDTH || y < 0 || y >= SCR_LINES) return;
  tp->tty_column = x;		/* set x co-ordinate */
  tp->tty_row = y;		/* set y co-ordinate */
  tp->tty_vid = (tp->tty_org + 2*(SCR_LINES-1-y)*LINE_WIDTH + 2*x);
  set_6845(CURSOR, tp->tty_vid >> 1);	/* cursor counts in words */
}


/*===========================================================================*
 *				escape					     *
 *===========================================================================*/
PRIVATE escape(tp, x, y)
register struct tty_struct *tp;	/* pointer to tty struct */
char x;				/* escape sequence is ESC x y; this is x */
char y;				/* escape sequence is ESC x y; this is y */
{
/* Handle an escape sequence. */

  int n, ct, vx;


  /* Check for ESC z attribute - used to change attribute byte. */
  if (x == 'z') {
	/* Set attribute byte */
	tp->tty_attribute = y << 8;
	return;
  }
  /* Check for ESC ~ n -  used for clear screen, reverse scroll. */
  if (x == '~') {
	if (y == '0') {
		/* Clear from cursor to end of screen */
		n = 2 * LINE_WIDTH * (tp->tty_row + 1) - 2 * tp->tty_column;
		vx = tp->tty_vid;
		while (n > 0) {
			ct = MIN(n, vid_retrace);
			vid_copy(NIL_PTR, vid_base, vx, ct/2);
			vx += ct;
			n -= ct;
		}
	} else if (y == '1') {
		/* Reverse scroll. */
		scroll_screen(tp, GO_BACKWARD);
	}
	return;
  }

  /* Must be cursor movement (or invalid). */
  move_to(tp, x - 32, y - 32);
}


/*===========================================================================*
 *				set_6845				     *
 *===========================================================================*/
PRIVATE set_6845(reg, val)
int reg;			/* which register pair to set */
int val;			/* 16-bit value to set it to */
{
/* Set a register pair inside the 6845.  
 * Registers 10-11 control the format of the cursor (how high it is, etc).
 * Registers 12-13 tell the 6845 where in video ram to start (in WORDS)
 * Registers 14-15 tell the 6845 where to put the cursor (in WORDS)
 *
 * Note that registers 12-15 work in words, i.e. 0x0000 is the top left
 * character, but 0x0001 (not 0x0002) is the next character.  This addressing
 * is different from the way the 8088 addresses the video ram, where 0x0002
 * is the address of the next character.
 */
  port_out(vid_port + INDEX, reg);	/* set the index register */
  port_out(vid_port + DATA, (val>>8) & BYTE);	/* output high byte */
  port_out(vid_port + INDEX, reg + 1);	/* again */
  port_out(vid_port + DATA, val&BYTE);	/* output low byte */
}


/*===========================================================================*
 *				beep					     *
 *===========================================================================*/
PRIVATE beep(f)
int f;				/* this value determines beep frequency */
{
/* Making a beeping sound on the speaker (output for CRTL-G).  The beep is
 * kept short, because interrupts must be disabled during beeping, and it
 * is undesirable to keep them off too long.  This routine works by turning
 * on the bits in port B of the 8255 chip that drive the speaker.
 */

  int x, k;

  lock();			/* disable interrupts */
  port_out(TIMER3,0xB6);	/* set up timer channel 2 mode */
  port_out(TIMER2, f&BYTE);	/* load low-order bits of frequency in timer */
  port_out(TIMER2,(f>>8)&BYTE);	/* now high-order bits of frequency in timer */
  port_in(PORT_B,&x);		/* acquire status of port B */
  port_out(PORT_B, x|3);	/* turn bits 0 and 1 on to beep */
  for (k = 0; k < B_TIME; k++);	/* delay loop while beeper sounding */
  port_out(PORT_B, x);		/* restore port B the way it was */
  unlock();			/* re-enable interrupts */
}


/*===========================================================================*
 *				tty_init				     *
 *===========================================================================*/
PRIVATE tty_init()
{
/* Initialize the tty tables. */

  register struct tty_struct *tp;
  int i;
  phys_bytes phy1, phy2, vram;

  for (tp = &tty_struct[0]; tp < &tty_struct[NR_TTYS]; tp++) {
	tp->tty_inhead = tp->tty_inqueue;
	tp->tty_intail = tp->tty_inqueue;
	tp->tty_mode = CRMOD | XTABS | ECHO;
	tp->tty_devstart = console;
	tp->tty_erase = ERASE_CHAR;
	tp->tty_kill  = KILL_CHAR;
	tp->tty_intr  = INTR_CHAR;
	tp->tty_quit  = QUIT_CHAR;
	tp->tty_xon   = XON_CHAR;
	tp->tty_xoff  = XOFF_CHAR;
	tp->tty_eof   = EOT_CHAR;
  }

  tty_struct[0].tty_makebreak = TWO_INTS;	/* tty 0 is console */
  if (color) {
	vid_base = COLOR_BASE;
	vid_mask = C_VID_MASK;
	vid_port = C_6845;
	vid_retrace = C_RETRACE;
  } else {
	vid_base = MONO_BASE;
	vid_mask = M_VID_MASK;
	vid_port = M_6845;
	vid_retrace = M_RETRACE;
  }
  tty_struct[0].tty_attribute = BLANK;
  tty_driver_buf[1] = MAX_OVERRUN;	/* set up limit on keyboard buffering */
  set_6845(CUR_SIZE, 31);		/* set cursor shape */
  set_6845(VID_ORG, 0);			/* use page 0 of video ram */
  move_to(&tty_struct[0], 0, 0);	/* move cursor to lower left corner */

  /* Determine which keyboard type is attached.  The bootstrap program asks 
   * the user to type an '='.  The scan codes for '=' differ depending on the
   * keyboard in use.
   */
  if (scan_code == OLIVETTI_EQUAL) olivetti = TRUE;
}


/*===========================================================================*
 *				putc					     *
 *===========================================================================*/
PUBLIC putc(c)
char c;				/* character to print */
{
/* This procedure is used by the version of printf() that is linked with
 * the kernel itself.  The one in the library sends a message to FS, which is
 * not what is needed for printing within the kernel.  This version just queues
 * the character and starts the output.
 */

  out_char(&tty_struct[0], c);
}


/*===========================================================================*
 *				func_key				     *
 *===========================================================================*/
PRIVATE func_key(ch)
char ch;			/* scan code for a function key */
{
/* This procedure traps function keys for debugging purposes.  When MINIX is
 * fully debugged, it should be removed.
 */

  if (ch == F1) p_dmp();	/* print process table */
  if (ch == F2) map_dmp();	/* print memory map */
}
#endif

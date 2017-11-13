/* This file contains a few general purpose utility routines.
 *
 * The entry points into this file are
 *   clock_time:  ask the clock task for the real time
 *   cmp_string:  compare two strings (e.g., while searching directory)
 *   copy:        copy a string
 *   fetch_name:  go get a path name from user space
 *   no_sys:      reject a system call that FS does not handle
 *   panic:       something awful has occurred;  MINIX cannot continue
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"
#include "super.h"

PRIVATE int panicking;		/* inhibits recursive panics during sync */
PRIVATE message clock_mess;

/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
PUBLIC real_time clock_time()
{
/* This routine returns the time in seconds since 1.1.1970. */

  register int k;
  register struct super_block *sp;
  extern struct super_block *get_super();

  clock_mess.m_type = GET_TIME;
  if ( (k = sendrec(CLOCK, &clock_mess)) != OK) panic("clock_time err", k);

  /* Since we now have the time, update the super block.  It is almost free. */
  sp = get_super(ROOT_DEV);
  sp->s_time = clock_mess.NEW_TIME;	/* update super block time */
  if (sp->s_rd_only == FALSE) sp->s_dirt = DIRTY;

  return (real_time) clock_mess.NEW_TIME;
}


/*===========================================================================*
 *				cmp_string				     *
 *===========================================================================*/
PUBLIC int cmp_string(rsp1, rsp2, n)
register char *rsp1, *rsp2;	/* pointers to the two strings */
register int n;			/* string length */
{
/* Compare two strings of length 'n'.  If they are the same, return 1.
 * If they differ, return 0.
 */

  do {
	if (*rsp1++ != *rsp2++) return(0);
  } while (--n);

  /* The strings are identical. */
  return(1);
}



/*===========================================================================*
 *				copy					     *
 *===========================================================================*/
PUBLIC copy(dest, source, bytes)
char *dest;			/* destination pointer */
char *source;			/* source pointer */
int bytes;			/* how much data to move */
{
/* Copy a byte string of length 'bytes' from 'source' to 'dest'.
 * If all three parameters are exactly divisible by the integer size, copy them
 * an integer at a time.  Otherwise copy character-by-character.
 */

  if (bytes <= 0) return;		/* makes test-at-the-end possible */

  if (bytes % sizeof(int) == 0 && (int) dest % sizeof(int) == 0 &&
      						(int) source % sizeof(int) == 0) {
	/* Copy the string an integer at a time. */
	register int n = bytes/sizeof(int);
	register int *dpi = (int *) dest;
	register int *spi = (int *) source;

	do { *dpi++ = *spi++; } while (--n);

  } else {

	/* Copy the string character-by-character. */
	register int n = bytes;
	register char *dpc = (char *) dest;
	register char *spc = (char *) source;

	do { *dpc++ = *spc++; } while (--n);

  }
}


/*===========================================================================*
 *				fetch_name				     *
 *===========================================================================*/
PUBLIC int fetch_name(path, len, flag)
char *path;			/* pointer to the path in user space */
int len;			/* path length, including 0 byte */
int flag;			/* M3 means path may be in message */
{
/* Go get path and put it in 'user_path'.
 * If 'flag' = M3 and 'len' <= M3_STRING, the path is present in 'message'.
 * If it is not, go copy it from user space.
 */

  register char *rpu, *rpm;
  vir_bytes vpath;

  if (flag == M3 && len <= M3_STRING) {
	/* Just copy the path from the message to 'user_path'. */
	rpu = &user_path[0];
	rpm = pathname;		/* contained in input message */
	do { *rpu++ = *rpm++; } while (--len);
	return(OK);
  }

  /* String is not contained in the message.  Go get it from user space. */
  if (len > MAX_PATH) {
	err_code = E_LONG_STRING;
	return(ERROR);
  }
  vpath = (vir_bytes) path;
  err_code = rw_user(D, who, vpath, (vir_bytes) len, user_path, FROM_USER);
  return(err_code);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */

  return(EINVAL);
}


/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC panic(format, num)
char *format;			/* format string */
int num;			/* number to go with format string */
{
/* Something awful has happened.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant.
 */

  if (panicking) return;	/* do not panic during a sync */
  panicking = TRUE;		/* prevent another panic during the sync */
  printf("File system panic: %s ", format);
  if (num != NO_NUM) printf("%d",num); 
  printf("\n");
  do_sync();			/* flush everything to the disk */
  sys_abort();
}

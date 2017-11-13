/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utime:	perform the UTIME system call
 *   do_time:	perform the TIME system call
 *   do_stime:	perform the STIME system call
 *   do_tims:	perform the TIMES system call
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"

PRIVATE message clock_mess;

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
PUBLIC int do_utime()
{
/* Perform the utime(name, timep) system call. */

  register struct inode *rip;
  register int r;
  extern struct inode *eat_path();

  /* Temporarily open the file. */
  if (fetch_name(utime_file, utime_length, M1) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* Only the owner of a file or the super_user can change its time. */
  r = OK;
  if (rip->i_uid != fp->fp_effuid && !super_user) r = EPERM;
  if (r == OK) {
	rip->i_modtime = update_time;
	rip->i_dirt = DIRTY;
  }

  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *				do_time					     *
 *===========================================================================*/
PUBLIC int do_time()

{
/* Perform the time(tp) system call. */

  extern real_time clock_time();

  reply_l1 = clock_time();	/* return time in seconds */
  return(OK);
}


/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
PUBLIC int do_stime()
{
/* Perform the stime(tp) system call. */

  register int k;

  if (!super_user) return(EPERM);
  clock_mess.m_type = SET_TIME;
  clock_mess.NEW_TIME = (long) tp;
  if ( (k = sendrec(CLOCK, &clock_mess)) != OK) panic("do_stime error", k);
  return (OK);
}


/*===========================================================================*
 *				do_tims					     *
 *===========================================================================*/
PUBLIC int do_tims()
{
/* Perform the times(buffer) system call. */

  real_time t[4];

  sys_times(who, t);
  reply_t1 = t[0];
  reply_t2 = t[1];
  reply_t3 = t[2];
  reply_t4 = t[3];
  return(OK);
}

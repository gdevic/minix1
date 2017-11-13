/* This file contains some useful utility routines used by MM.
 *
 * The entries into the file are:
 *   allowed:	see if an access is permitted
 *   mem_copy:	copy data from somewhere in memory to somewhere else
 *   no_sys:	this routine is called for invalid system call numbers
 *   panic:	MM has run aground of a fatal error and cannot continue
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "../h/stat.h"
#include "const.h"
#include "glo.h"
#include "mproc.h"

PRIVATE message copy_mess;

/*===========================================================================*
 *				allowed					     *
 *===========================================================================*/
PUBLIC int allowed(name_buf, s_buf, mask)
char *name_buf;			/* pointer to file name to be EXECed */
struct stat *s_buf;		/* buffer for doing and returning stat struct */
int mask;			/* R_BIT, W_BIT, or X_BIT */
{
/* Check to see if file can be accessed.  Return EACCES or ENOENT if the access
 * is prohibited.  If it is legal open the file and return a file descriptor.
 */

  register int fd, shift;
  int mode;
  extern errno;

  /* Open the file and stat it. */
  if ( (fd = open(name_buf, 0)) < 0) return(-errno);
  if (fstat(fd, s_buf) < 0) panic("allowed: fstat failed", NO_NUM);

  /* Only regular files can be executed. */
  mode = s_buf->st_mode & I_TYPE;
  if (mask == X_BIT && mode != I_REGULAR) {
	close(fd);
	return(EACCES);
  }
  /* Even for superuser, at least 1 X bit must be on. */
  if (mp->mp_effuid == SUPER_USER && mask == X_BIT &&
	(s_buf->st_mode & (X_BIT << 6 | X_BIT << 3 | X_BIT))) return(fd);

  /* Right adjust the relevant set of permission bits. */
  if (mp->mp_effuid == s_buf->st_uid) shift = 6;
  else if (mp->mp_effgid == s_buf->st_gid) shift = 3;
  else shift = 0;

  if (mp->mp_effuid == SUPER_USER && mask != X_BIT) return(fd);
  if (s_buf->st_mode >> shift & mask)	/* test the relevant bits */
	return(fd);		/* permission granted */
  else {
	close(fd);		/* permission denied */
	return(EACCES);
  }
}


/*===========================================================================*
 *				mem_copy				     *
 *===========================================================================*/
PUBLIC int mem_copy(src_proc,src_seg, src_vir, dst_proc,dst_seg, dst_vir, bytes)
int src_proc;			/* source process */
int src_seg;			/* source segment: T, D, or S */
long src_vir;			/* source virtual address (clicks for ABS) */
int dst_proc;			/* dest process */
int dst_seg;			/* dest segment: T, D, or S */
long dst_vir;			/* dest virtual address (clicks for ABS) */
long bytes;			/* how many bytes (clicks for ABS) */
{
/* Transfer a block of data.  The source and destination can each either be a
 * process (including MM) or absolute memory, indicate by setting 'src_proc'
 * or 'dst_proc' to ABS.
 */

  if (bytes == 0L) return(OK);
  copy_mess.SRC_SPACE = (char) src_seg;
  copy_mess.SRC_PROC_NR = src_proc;
  copy_mess.SRC_BUFFER = src_vir;

  copy_mess.DST_SPACE = (char) dst_seg;
  copy_mess.DST_PROC_NR = dst_proc;
  copy_mess.DST_BUFFER = dst_vir;

  copy_mess.COPY_BYTES = bytes;
  sys_copy(&copy_mess);
  return(copy_mess.m_type);
}
/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* A system call number not implemented by MM has been requested. */

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
 * inconsistency is detected, e.g., a programm_ing error or illegal value of a
 * defined constant.
 */

  printf("Memory manager panic: %s ", format);
  if (num != NO_NUM) printf("%d",num); 
  printf("\n");
  tell_fs(SYNC, 0, 0, 0);	/* flush the cache to the disk */
  sys_abort();
}

/* This file contains the code for performing four system calls relating to
 * status and directories.
 *
 * The entry points into this file are
 *   do_chdir:	perform the CHDIR system call
 *   do_chroot:	perform the CHROOT system call
 *   do_stat:	perform the STAT system call
 *   do_fstat:	perform the FSTAT system call
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/error.h"
#include "../h/stat.h"
#include "const.h"
#include "type.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"

/*===========================================================================*
 *				do_chdir				     *
 *===========================================================================*/
PUBLIC int do_chdir()
{
/* Change directory.  This function is  also called by MM to simulate a chdir
 * in order to do EXEC, etc.
 */

  register struct fproc *rfp;

  if (who == MM_PROC_NR) {
	rfp = &fproc[slot1];
	put_inode(fp->fp_workdir);
	fp->fp_workdir = (cd_flag ? fp->fp_rootdir : rfp->fp_workdir);
	dup_inode(fp->fp_workdir);
	fp->fp_effuid = (cd_flag ? SUPER_USER : rfp->fp_effuid);
	return(OK);
  }

/* Perform the chdir(name) system call. */
  return change(&fp->fp_workdir, name, name_length);
}


/*===========================================================================*
 *				do_chroot				     *
 *===========================================================================*/
PUBLIC int do_chroot()
{
/* Perform the chroot(name) system call. */

  register int r;

  if (!super_user) return(EPERM);	/* only su may chroot() */
  r = change(&fp->fp_rootdir, name, name_length);
  return(r);
}


/*===========================================================================*
 *				change					     *
 *===========================================================================*/
PRIVATE int change(iip, name_ptr, len)
struct inode **iip;		/* pointer to the inode pointer for the dir */
char *name_ptr;			/* pointer to the directory name to change to */
int len;			/* length of the directory name string */
{
/* Do the actual work for chdir() and chroot(). */

  struct inode *rip;
  register int r;
  extern struct inode *eat_path();

  /* Try to open the new directory. */
  if (fetch_name(name_ptr, len, M3) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* It must be a directory and also be searchable. */
  if ( (rip->i_mode & I_TYPE) != I_DIRECTORY)
	r = ENOTDIR;
  else
	r = forbidden(rip, X_BIT, 0);	/* check if dir is searchable */

  /* If error, return inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  /* Everything is OK.  Make the change. */
  put_inode(*iip);		/* release the old directory */
  *iip = rip;			/* acquire the new one */
  return(OK);
}


/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
PUBLIC int do_stat()
{
/* Perform the stat(name, buf) system call. */

  register struct inode *rip;
  register int r;
  extern struct inode *eat_path();

  /* Both stat() and fstat() use the same routine to do the real work.  That
   * routine expects an inode, so acquire it temporarily.
   */
  if (fetch_name(name1, name1_length, M1) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);
  r = stat_inode(rip, NIL_FILP, name2);	/* actually do the work.*/
  put_inode(rip);		/* release the inode */
  return(r);
}


/*===========================================================================*
 *				do_fstat				     *
 *===========================================================================*/
PUBLIC int do_fstat()
{
/* Perform the fstat(fd, buf) system call. */

  register struct filp *rfilp;
  extern struct filp *get_filp();

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(fd)) == NIL_FILP) return(err_code);

  return(stat_inode(rfilp->filp_ino, rfilp, buffer));
}


/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
PRIVATE int stat_inode(rip, fil_ptr, user_addr)
register struct inode *rip;	/* pointer to inode to stat */
struct filp *fil_ptr;		/* filp pointer, supplied by 'fstat' */
char *user_addr;			/* user space address where stat buf goes */
{
/* Common code for stat and fstat system calls. */

  register struct stat *stp;
  struct stat statbuf;
  int r;
  vir_bytes v;

  /* Fill in the statbuf struct. */
  stp = &statbuf;		/* set up pointer to the buffer */
  stp->st_dev = (int) rip->i_dev;
  stp->st_ino = rip->i_num;
  stp->st_mode = rip->i_mode;
  stp->st_nlink = rip->i_nlinks & BYTE;
  stp->st_uid = rip->i_uid;
  stp->st_gid = rip->i_gid & BYTE;
  stp->st_rdev = rip->i_zone[0];
  stp->st_size = rip->i_size;
  if (	(rip->i_pipe == I_PIPE) &&	/* IF it is a pipe */
	(fil_ptr != NIL_FILP) &&	/* AND it was fstat */
	(fil_ptr->filp_mode == R_BIT))	/* on the reading end, */
	stp->st_size -= fil_ptr->filp_pos; /* adjust the visible size. */
  stp->st_atime = rip->i_modtime;
  stp->st_mtime = rip->i_modtime;
  stp->st_ctime = rip->i_modtime;

  /* Copy the struct to user space. */
  v = (vir_bytes) user_addr;
  r = rw_user(D, who, v, (vir_bytes) sizeof statbuf, (char *) stp, TO_USER);
  return(r);
}

/* This file deals with protection in the file system.  It contains the code
 * for four system calls that relate to protection.
 *
 * The entry points into this file are
 *   do_chmod:	perform the CHMOD system call
 *   do_chown:	perform the CHOWN system call
 *   do_umask:	perform the UMASK system call
 *   do_access:	perform the ACCESS system call
 *   forbidden:	check to see if a given access is allowed on a given inode
 */

#include "../h/const.h"
#include "../h/type.h"
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

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC int do_chmod()
{
/* Perform the chmod(name, mode) system call. */

  register struct inode *rip;
  register int r;
  extern struct inode *eat_path();

  /* Temporarily open the file. */
  if (fetch_name(name, name_length, M3) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* Only the owner or the super_user may change the mode of a file.
   * No one may change the mode of a file on a read-only file system.
   */
  if (rip->i_uid != fp->fp_effuid && !super_user)
	r = EPERM;
  else
	r = read_only(rip);

  /* If error, return inode. */
  if (r != OK)	{
	put_inode(rip);
	return(r);
  }

  /* Now make the change. */
  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (mode & ALL_MODES);
  rip->i_dirt = DIRTY;

  put_inode(rip);
  return(OK);
}


/*===========================================================================*
 *				do_chown				     *
 *===========================================================================*/
PUBLIC int do_chown()
{
/* Perform the chown(name, owner, group) system call. */

  register struct inode *rip;
  register int r;
  extern struct inode *eat_path();

  /* Only the super_user may perform the chown() call. */
  if (!super_user) return(EPERM);

  /* Temporarily open the file. */
  if (fetch_name(name1, name1_length, M1) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* Not permitted to change the owner of a file on a read-only file sys. */
  r = read_only(rip);
  if (r == OK) {
	rip->i_uid = owner;
	rip->i_gid = group;
	rip->i_dirt = DIRTY;
  }

  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *				do_umask				     *
 *===========================================================================*/
PUBLIC int do_umask()
{
/* Perform the umask(co_mode) system call. */
  register mask_bits r;

  r = ~fp->fp_umask;		/* set 'r' to complement of old mask */
  fp->fp_umask = ~(co_mode & RWX_MODES);
  return(r);			/* return complement of old mask */
}


/*===========================================================================*
 *				do_access				     *
 *===========================================================================*/
PUBLIC int do_access()
{
/* Perform the access(name, mode) system call. */

  struct inode *rip;
  register int r;
  extern struct inode *eat_path();

  /* Temporarily open the file whose access is to be checked. */
  if (fetch_name(name, name_length, M3) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* Now check the permissions. */
  r = forbidden(rip, (mask_bits) mode, 1);
  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
PUBLIC int forbidden(rip, access_desired, real_uid)
register struct inode *rip;	/* pointer to inode to be checked */
mask_bits access_desired;	/* RWX bits */
int real_uid;			/* set iff real uid to be tested */
{
/* Given a pointer to an inode, 'rip', and the accessed desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fproc' table.  If the access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  register mask_bits bits, perm_bits, xmask;
  int r, shift, test_uid, test_gid;

  /* Isolate the relevant rwx bits from the mode. */
  bits = rip->i_mode;
  test_uid = (real_uid ? fp->fp_realuid : fp->fp_effuid);
  test_gid = (real_uid ? fp->fp_realgid : fp->fp_effgid);
  if (super_user) {
	perm_bits = 07;
  } else {
	if (test_uid == rip->i_uid) shift = 6;		/* owner */
	else if (test_gid == rip->i_gid ) shift = 3;	/* group */
	else shift = 0;					/* other */
	perm_bits = (bits >> shift) & 07;
  }

  /* If access desired is not a subset of what is allowed, it is refused. */
  r = OK;
  if ((perm_bits | access_desired) != perm_bits) r = EACCES;

  /* If none of the X bits are on, not even the super-user can execute it. */
  xmask = (X_BIT << 6) | (X_BIT << 3) | X_BIT;	/* all 3 X bits */
  if ( (access_desired & X_BIT) && (bits & xmask) == 0) r = EACCES;

  /* Check to see if someone is trying to write on a file system that is
   * mounted read-only.
   */
  if (r == OK)
	if (access_desired & W_BIT) r = read_only(rip);

  return(r);
}


/*===========================================================================*
 *				read_only				     *
 *===========================================================================*/
PRIVATE int read_only(ip)
struct inode *ip;		/* ptr to inode whose file sys is to be cked */
{
/* Check to see if the file system on which the inode 'ip' resides is mounted
 * read only.  If so, return EROFS, else return OK.
 */

  register struct super_block *sp;
  extern struct super_block *get_super();

  sp = get_super(ip->i_dev);
  return(sp->s_rd_only ? EROFS : OK);
}

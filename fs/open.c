/* This file contains the procedures for creating, opening, closing, and
 * seeking on files.
 *
 * The entry points into this file are
 *   do_creat:	perform the CREAT system call
 *   do_mknod:	perform the MKNOD system call
 *   do_open:	perform the OPEN system call
 *   do_close:	perform the CLOSE system call
 *   do_lseek:  perform the LSEEK system call
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "glo.h"
#include "inode.h"
#include "param.h"

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT|W_BIT, 0};

/*===========================================================================*
 *				do_creat				     *
 *===========================================================================*/
PUBLIC int do_creat()
{
/* Perform the creat(name, mode) system call. */

  register struct inode *rip;
  register int r;
  register mask_bits bits;
  struct filp *fil_ptr;
  int file_d;
  extern struct inode *new_node();

  /* See if name ok and file descriptor and filp slots are available. */
  if (fetch_name(name, name_length, M3) != OK) return(err_code);
  if ( (r = get_fd(W_BIT, &file_d, &fil_ptr)) != OK) return(r);

  /* Create a new inode by calling new_node(). */
  bits = I_REGULAR | (mode & ALL_MODES & fp->fp_umask);
  rip = new_node(user_path, bits, NO_ZONE);
  r = err_code;
  if (r != OK && r != EEXIST) return(r);

  /* At this point two possibilities exist: the given path did not exist
   * and has been created, or it pre-existed.  In the later case, truncate
   * if possible, otherwise return an error.
   */
  if (r == EEXIST) {
	/* File exists already. */
	switch (rip->i_mode & I_TYPE) {
	    case I_REGULAR:		/* truncate regular file */
		if ( (r = forbidden(rip, W_BIT, 0)) == OK) truncate(rip);
		break;

	    case I_DIRECTORY:	/* can't truncate directory */
		r = EISDIR;
		break;

	    case I_CHAR_SPECIAL:	/* special files are special */
	    case I_BLOCK_SPECIAL:
		if ( (r = forbidden(rip, W_BIT, 0)) != OK) break;
		r = dev_open( (dev_nr) rip->i_zone[0], W_BIT);
		break;
	}
  }

  /* If error, return inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  /* Claim the file descriptor and filp slot and fill them in. */
  fp->fp_filp[file_d] = fil_ptr;
  fil_ptr->filp_count = 1;
  fil_ptr->filp_ino = rip;
  return(file_d);
}



/*===========================================================================*
 *				do_mknod				     *
 *===========================================================================*/
PUBLIC int do_mknod()
{
/* Perform the mknod(name, mode, addr) system call. */

  register mask_bits bits;

  if (!super_user) return(EPERM);	/* only super_user may make nodes */
  if (fetch_name(name1, name1_length, M1) != OK) return(err_code);
  bits = (mode & I_TYPE) | (mode  & ALL_MODES & fp->fp_umask);
  put_inode(new_node(user_path, bits, (zone_nr) addr));
  return(err_code);
}


/*===========================================================================*
 *				new_node				     *
 *===========================================================================*/
PRIVATE struct inode *new_node(path, bits, z0)
char *path;			/* pointer to path name */
mask_bits bits;			/* mode of the new inode */
zone_nr z0;			/* zone number 0 for new inode */
{
/* This function is called by do_creat() and do_mknod().  In both cases it
 * allocates a new inode, makes a directory entry for it on the path 'path',
 * and initializes it.  It returns a pointer to the inode if it can do this;
 * err_code is set to OK or EEXIST. If it can't, it returns NIL_INODE and
 * 'err_code' contains the appropriate message.
 */

  register struct inode *rlast_dir_ptr, *rip;
  register int r;
  char string[NAME_SIZE];
  extern struct inode *alloc_inode(), *advance(), *last_dir();

  /* See if the path can be opened down to the last directory. */
  if ((rlast_dir_ptr = last_dir(path, string)) == NIL_INODE) return(NIL_INODE);

  /* The final directory is accessible. Get final component of the path. */
  rip = advance(rlast_dir_ptr, string);
  if ( rip == NIL_INODE && err_code == ENOENT) {
	/* Last path component does not exist.  Make new directory entry. */
	if ( (rip = alloc_inode(rlast_dir_ptr->i_dev, bits)) == NIL_INODE) {
		/* Can't creat new inode: out of inodes. */
		put_inode(rlast_dir_ptr);
		return(NIL_INODE);
	}

	/* Force inode to the disk before making directory entry to make
	 * the system more robust in the face of a crash: an inode with
	 * no directory entry is much better than the opposite.
	 */
	rip->i_nlinks++;
	rip->i_zone[0] = z0;
	rw_inode(rip, WRITING);		/* force inode to disk now */

	/* New inode acquired.  Try to make directory entry. */
	if ((r = search_dir(rlast_dir_ptr, string, &rip->i_num,ENTER)) != OK) {
		put_inode(rlast_dir_ptr);
		rip->i_nlinks--;	/* pity, have to free disk inode */
		rip->i_dirt = DIRTY;	/* dirty inodes are written out */
		put_inode(rip);	/* this call frees the inode */
		err_code = r;
		return(NIL_INODE);
	}

  } else {
	/* Either last component exists, or there is some problem. */
	if (rip != NIL_INODE)
		r = EEXIST;
	else
		r = err_code;
  }

  /* Return the directory inode and exit. */
  put_inode(rlast_dir_ptr);
  err_code = r;
  return(rip);
}


/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
PUBLIC int do_open()
{
/* Perform the open(name, mode) system call. */

  register struct inode *rip;
  struct filp *fil_ptr;
  register int r;
  register mask_bits bits;
  int file_d;
  extern struct inode *eat_path();

  /* See if file descriptor and filp slots are available.  The variable
   * 'mode' is 0 for read, 1 for write, 2 for read+write.  The variable
   * 'bits' needs to be R_BIT, W_BIT, and R_BIT|W_BIT respectively.
   */
  if (mode < 0 || mode > 2) return(EINVAL);
  if (fetch_name(name, name_length, M3) != OK) return(err_code);
  bits = (mask_bits) mode_map[mode];
  if ( (r = get_fd(bits, &file_d, &fil_ptr)) != OK) return(r);

  /* Scan path name. */
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  if ((r = forbidden(rip, bits, 0)) != OK) {
	put_inode(rip);          /* can't open: protection violation */
	return(r);
  }

  /* Opening regular files, directories and special files are different. */
  switch (rip->i_mode & I_TYPE) {
     case I_DIRECTORY:
	if (bits & W_BIT) {
		put_inode(rip);
		return(EISDIR);
	}
	break;

     case I_CHAR_SPECIAL:
	/* Assume that first open of char special file is controlling tty. */
	if (fp->fs_tty == 0) fp->fs_tty = (dev_nr) rip->i_zone[0];
	dev_open((dev_nr) rip->i_zone[0], (int) bits);
	break;

     case I_BLOCK_SPECIAL:
	dev_open((dev_nr) rip->i_zone[0], (int) bits);
	break;
  }

  /* Claim the file descriptor and filp slot and fill them in. */
  fp->fp_filp[file_d] = fil_ptr;
  fil_ptr->filp_count = 1;
  fil_ptr->filp_ino = rip;
  return(file_d);
}


/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
PUBLIC int do_close()
{
/* Perform the close(fd) system call. */

  register struct filp *rfilp;
  register struct inode *rip;
  int rw;
  int mode_word;
  extern struct filp *get_filp();

  /* First locate the inode that belongs to the file descriptor. */
  if ( (rfilp = get_filp(fd)) == NIL_FILP) return(err_code);
  rip = rfilp->filp_ino;	/* 'rip' points to the inode */

  /* Check to see if the file is special. */
  mode_word = rip->i_mode & I_TYPE;
  if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
	if (mode_word == I_BLOCK_SPECIAL)  {
		/* Invalidate cache entries unless special is mounted or ROOT.*/
		do_sync();	/* purge cache */
		if (mounted(rip) == FALSE) invalidate((dev_nr) rip->i_zone[0]);
	}
	dev_close((dev_nr) rip->i_zone[0]);
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (rfilp->filp_ino->i_pipe) {
	rw = (rfilp->filp_mode & R_BIT ? WRITE : READ);
	release(rfilp->filp_ino, rw, NR_PROCS);
  }

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--rfilp->filp_count == 0) put_inode(rfilp->filp_ino);

  fp->fp_filp[fd] = NIL_FILP;
  return(OK);
}


/*===========================================================================*
 *				do_lseek				     *
 *===========================================================================*/
PUBLIC int do_lseek()
{
/* Perform the lseek(ls_fd, offset, whence) system call. */

  register struct filp *rfilp;
  register file_pos pos;
  extern struct filp *get_filp();

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(ls_fd)) == NIL_FILP) return(err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_ino->i_pipe == I_PIPE) return(ESPIPE);

  /* The value of 'whence' determines the algorithm to use. */
  switch(whence) {
	case 0:	pos = offset;	break;
	case 1: pos = rfilp->filp_pos + offset;	break;
	case 2: pos = rfilp->filp_ino->i_size + offset;	break;
	default: return(EINVAL);
  }
  if (pos < (file_pos) 0) return(EINVAL);

  rfilp->filp_ino->i_seek = ISEEK;	/* inhibit read ahead */
  rfilp->filp_pos = pos;

  reply_l1 = pos;		/* insert the long into the output message */
  return(OK);
}

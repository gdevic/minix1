/* This file handles the LINK and UNLINK system calls.  It also deals with
 * deallocating the storage used by a file when the last UNLINK is done to a
 * file and the blocks must be returned to the free block pool.
 *
 * The entry points into this file are
 *   do_link:	perform the LINK system call
 *   do_unlink:	perform the UNLINK system call
 *   truncate:	release all the blocks associated with an inode
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

/*===========================================================================*
 *				do_link					     *
 *===========================================================================*/
PUBLIC int do_link()
{
/* Perform the link(name, name2) system call. */

  register struct inode *ip, *rip;
  register int r;
  char string[NAME_SIZE];
  struct inode *new_ip;
  extern struct inode *advance(), *last_dir(), *eat_path();

  /* See if 'name' (file to be linked) exists. */
  if (fetch_name(name1, name1_length, M1) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* Check to see if the file has maximum number of links already. */
  r = OK;
  if ( (rip->i_nlinks & BYTE) == MAX_LINKS) r = EMLINK;

  /* Only super_user may link to directories. */
  if (r == OK)
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY && !super_user) r = EPERM;

  /* If error with 'name', return the inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  /* Does the final directory of 'name2' exist? */
  if (fetch_name(name2, name2_length, M1) != OK) return(err_code);
  if ( (ip = last_dir(user_path, string)) == NIL_INODE) r = err_code;

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if (r == OK) {
	if ( (new_ip = advance(ip, string)) == NIL_INODE) {
		r = err_code;
		if (r == ENOENT) r = OK;
	} else {
		put_inode(new_ip);
		r = EEXIST;
	}
  }

  /* Check for links across devices. */
  if (r == OK)
	if (rip->i_dev != ip->i_dev) r = EXDEV;

  /* Try to link. */
  if (r == OK)
	r = search_dir(ip, string, &rip->i_num, ENTER);

  /* If success, register the linking. */
  if (r == OK) {
	rip->i_nlinks++;
	rip->i_dirt = DIRTY;
  }

  /* Done.  Release both inodes. */
  put_inode(rip);
  put_inode(ip);
  return(r);
}


/*===========================================================================*
 *				do_unlink				     *
 *===========================================================================*/
PUBLIC int do_unlink()
{
/* Perform the unlink(name) system call. */

  register struct inode *rip, *rlast_dir_ptr;
  register int r;
  inode_nr numb;
  char string[NAME_SIZE];
  extern struct inode *advance(), *last_dir();

  /* Get the last directory in the path. */
  if (fetch_name(name, name_length, M3) != OK) return(err_code);
  if ( (rlast_dir_ptr = last_dir(user_path, string)) == NIL_INODE)
	return(err_code);

  /* The last directory exists.  Does the file also exist? */
  r = OK;
  if ( (rip = advance(rlast_dir_ptr, string)) == NIL_INODE) r = err_code;

  /* If error, return inode. */
  if (r != OK) {
	put_inode(rlast_dir_ptr);
	return(r);
  }

  /* See if the file is a directory. */
  if ( (rip->i_mode & I_TYPE) == I_DIRECTORY && !super_user)
	r = EPERM;		 /* only super_user can unlink directory */
  if (r == OK)
	r = search_dir(rlast_dir_ptr, string, &numb, DELETE);

  if (r == OK) {
	rip->i_nlinks--;
	rip->i_dirt = DIRTY;
  }

  /* If unlink was possible, it has been done, otherwise it has not. */
  put_inode(rip);
  put_inode(rlast_dir_ptr);
  return(r);
}


/*===========================================================================*
 *				truncate				     *
 *===========================================================================*/
PUBLIC truncate(rip)
register struct inode *rip;	/* pointer to inode to be truncated */
{
/* Remove all the zones from the inode 'rip' and mark it dirty. */

  register file_pos position;
  register zone_type zone_size;
  register block_nr b;
  register zone_nr z, *iz;
  register int scale;
  register struct buf *bp;
  register dev_nr dev;
  extern struct buf *get_block();
  extern block_nr read_map();

  dev = rip->i_dev;		/* device on which inode resides */
  scale = scale_factor(rip);
  zone_size = (zone_type) BLOCK_SIZE << scale;
  if (rip->i_pipe == I_PIPE) rip->i_size = PIPE_SIZE;	/* pipes can shrink */

  /* Step through the file a zone at a time, finding and freeing the zones. */
  for (position = 0; position < rip->i_size; position += zone_size) {
	if ( (b = read_map(rip, position)) != NO_BLOCK) {
		z = (zone_nr) b >> scale;
		free_zone(dev, z);
	}
  }

  /* All the data zones have been freed.  Now free the indirect zones. */
  free_zone(dev, rip->i_zone[NR_DZONE_NUM]);	/* single indirect zone */
  if ( (z = rip->i_zone[NR_DZONE_NUM+1]) != NO_ZONE) {
	b = (block_nr) z << scale;
	bp = get_block(dev, b, NORMAL);	/* get double indirect zone */
	for (iz = &bp->b_ind[0]; iz < &bp->b_ind[NR_INDIRECTS]; iz++) {
		free_zone(dev, *iz);
	}

	/* Now free the double indirect zone itself. */
	put_block(bp, INDIRECT_BLOCK);
	free_zone(dev, z);
  }

  /* The inode being truncated might currently be open, so certain fields must
   * be cleared immediately, even though these fields are also cleared by
   * alloc_inode(). The function wipe_inode() does the dirty work in both cases.
   */
  wipe_inode(rip);
}

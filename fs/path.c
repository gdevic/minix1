/* This file contains the procedures that look up path names in the directory
 * system and determine the inode number that goes with a given path name.
 *
 *  The entry points into this file are
 *   eat_path:	 the 'main' routine of the path-to-inode conversion mechanism
 *   last_dir:	 find the final directory on a given path
 *   advance:	 parse one component of a path name
 *   search_dir: search a directory for a string and return its inode number
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
#include "super.h"

/*===========================================================================*
 *				eat_path				     *
 *===========================================================================*/
PUBLIC struct inode *eat_path(path)
char *path;			/* the path name to be parsed */
{
/* Parse the path 'path' and put its inode in the inode table.  If not
 * possible, return NIL_INODE as function value and an error code in 'err_code'.
 */

  register struct inode *ldip, *rip;
  char string[NAME_SIZE];	/* hold 1 path component name here */
  extern struct inode *last_dir(), *advance();

  /* First open the path down to the final directory. */
  if ( (ldip = last_dir(path, string)) == NIL_INODE)
	return(NIL_INODE);	/* we couldn't open final directory */

  /* The path consisting only of "/" is a special case, check for it. */
  if (string[0] == '\0') return(ldip);

  /* Get final component of the path. */
  rip = advance(ldip, string);
  put_inode(ldip);
  return(rip);
}


/*===========================================================================*
 *				last_dir				     *
 *===========================================================================*/
PUBLIC struct inode *last_dir(path, string)
char *path;			/* the path name to be parsed */
char string[NAME_SIZE];		/* the final component is returned here */
{
/* Given a path, 'path', located in the fs address space, parse it as
 * far as the last directory, fetch the inode for the last directory into
 * the inode table, and return a pointer to the inode.  In
 * addition, return the final component of the path in 'string'.
 * If the last directory can't be opened, return NIL_INODE and
 * the reason for failure in 'err_code'.
 */

  register struct inode *rip;
  register char *new_name;
  register struct inode *new_ip;
  extern struct inode *advance();
  extern char *get_name();

  /* Is the path absolute or relative?  Initialize 'rip' accordingly. */
  rip = (*path == '/' ? fp->fp_rootdir : fp->fp_workdir);
  dup_inode(rip);		/* inode will be returned with put_inode */

  /* Scan the path component by component. */
  while (TRUE) {
	/* Extract one component. */
	if ( (new_name = get_name(path, string)) == (char*) 0) {
		put_inode(rip);	/* bad path in user space */
		return(NIL_INODE);
	}
	if (*new_name == '\0') return(rip);	/* normal exit */

	/* There is more path.  Keep parsing. */
	new_ip = advance(rip, string);
	put_inode(rip);		/* rip either obsolete or irrelevant */
	if (new_ip == NIL_INODE) return(NIL_INODE);

	/* The call to advance() succeeded.  Fetch next component. */
	path = new_name;
	rip = new_ip;
  }
}


/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
PRIVATE char *get_name(old_name, string)
char *old_name;			/* path name to parse */
char string[NAME_SIZE];		/* component extracted from 'old_name' */
{
/* Given a pointer to a path name in fs space, 'old_name', copy the next
 * component to 'string' and pad with zeros.  A pointer to that part of
 * the name as yet unparsed is returned.  Roughly speaking,
 * 'get_name' = 'old_name' - 'string'.
 *
 * This routine follows the standard convention that /usr/ast, /usr//ast,
 * //usr///ast and /usr/ast/ are all equivalent.
 */

  register int c;
  register char *np, *rnp;

  np = string;			/* 'np' points to current position */
  rnp = old_name;		/* 'rnp' points to unparsed string */
  while ( (c = *rnp) == '/') rnp++;	/* skip leading slashes */

  /* Copy the unparsed path, 'old_name', to the array, 'string'. */
  while ( rnp < &user_path[MAX_PATH]  &&  c != '/'   &&  c != '\0') {
	if (np < &string[NAME_SIZE]) *np++ = c;
	c = *++rnp;		/* advance to next character */
  }

  /* To make /usr/ast/ equivalent to /usr/ast, skip trailing slashes. */
  while (c == '/' && rnp < &user_path[MAX_PATH]) c = *++rnp;

  /* Pad the component name out to NAME_SIZE chars, using 0 as filler. */
  while (np < &string[NAME_SIZE]) *np++ = '\0';

  if (rnp >= &user_path[MAX_PATH]) {
	err_code = E_LONG_STRING;
	return((char *) 0);
  }
  return(rnp);
}


/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
PUBLIC struct inode *advance(dirp, string)
struct inode *dirp;		/* inode for directory to be searched */
char string[NAME_SIZE];		/* component name to look for */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.  If it can't be done, return NIL_INODE.
 */

  register struct inode *rip;
  struct inode *rip2;
  register struct super_block *sp;
  int r;
  dev_nr mnt_dev;
  inode_nr numb;
  extern struct inode *get_inode();

  /* If 'string' is empty, yield same inode straight away. */
  if (string[0] == '\0') return(get_inode(dirp->i_dev, dirp->i_num));

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir(dirp, string, &numb, LOOK_UP)) != OK) {
	err_code = r;
	return(NIL_INODE);
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_inode(dirp->i_dev, numb)) == NIL_INODE) return(NIL_INODE);

  if (rip->i_num == ROOT_INODE)
	if (dirp->i_num == ROOT_INODE) {
	    if (string[1] == '.') {
		for (sp = &super_block[1]; sp < &super_block[NR_SUPERS]; sp++) {
			if (sp->s_dev == rip->i_dev) {
				/* Release the root inode.  Replace by the
				 * inode mounted on.
				 */
				put_inode(rip);
				mnt_dev = sp->s_imount->i_dev;
				rip2 = get_inode(mnt_dev, sp->s_imount->i_num);
				rip = advance(rip2, string);
				put_inode(rip2);
				break;
			}
		}
	    }
	}
  /* See if the inode is mounted on.  If so, switch to root directory of the
   * mounted file system.  The super_block provides the linkage between the
   * inode mounted on and the root directory of the mounted file system.
   */
  while (rip->i_mount == I_MOUNT) {
	/* The inode is indeed mounted on. */
	for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++) {
		if (sp->s_imount == rip) {
			/* Release the inode mounted on.  Replace by the
			 * inode of the root inode of the mounted device.
			 */
			put_inode(rip);
			rip = get_inode(sp->s_dev, ROOT_INODE);
			break;
		}
	}
  }
  return(rip);		/* return pointer to inode's component */
}


/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
PUBLIC int search_dir(ldir_ptr, string, numb, flag)
register struct inode *ldir_ptr;	/* ptr to inode for dir to search */
char string[NAME_SIZE];		/* component to search for */
inode_nr *numb;			/* pointer to inode number */
int flag;			/* LOOK_UP, ENTER, or DELETE */
{
/* This function searches the directory whose inode is pointed to by 'ldip':
 * if (flag == LOOK_UP) search for 'string' and return inode # in 'numb';
 * if (flag == ENTER)  enter 'string' in the directory with inode # '*numb';
 * if (flag == DELETE) delete 'string' from the directory;
 */

  register dir_struct *dp;
  register struct buf *bp;
  register int r;
  mask_bits bits;
  file_pos pos;
  unsigned new_slots, old_slots;
  block_nr b;
  int e_hit;
  extern struct buf *get_block(), *new_block();
  extern block_nr read_map();
  extern real_time clock_time();

  /* If 'ldir_ptr' is not a pointer to a searchable dir inode, error. */
  if ( (ldir_ptr->i_mode & I_TYPE) != I_DIRECTORY) return(ENOTDIR);
  bits = (flag == LOOK_UP ? X_BIT : W_BIT|X_BIT);
  if ( (r = forbidden(ldir_ptr, bits, 0)) != OK)
	return(r);

  /* Step through the directory one block at a time. */
  old_slots = ldir_ptr->i_size/DIR_ENTRY_SIZE;
  new_slots = 0;
  e_hit = FALSE;
  for (pos = 0; pos < ldir_ptr->i_size; pos += BLOCK_SIZE) {
	b = read_map(ldir_ptr, pos);	/* get block number */

	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(ldir_ptr->i_dev, b, NORMAL);	/* get a dir block */

	/* Search a directory block. */
	for (dp = &bp->b_dir[0]; dp < &bp->b_dir[NR_DIR_ENTRIES]; dp++) {
		if (++new_slots > old_slots) { /* not found, but room left */
			if (flag == ENTER) e_hit = TRUE;
			break;
		}
		if (flag != ENTER && dp->d_inum != 0
				&& cmp_string(dp->d_name, string, NAME_SIZE)) {
			/* LOOK_UP or DELETE found what it wanted. */
			if (flag == DELETE) {
				dp->d_inum = 0;	/* erase entry */
				bp->b_dirt = DIRTY;
				ldir_ptr->i_modtime = clock_time();
			} else
				*numb = dp->d_inum;	/* 'flag' is LOOK_UP */
			put_block(bp, DIRECTORY_BLOCK);
			return(OK);
		}

		/* Check for free slot for the benefit of ENTER. */
		if (flag == ENTER && dp->d_inum == 0) {
			e_hit = TRUE;	/* we found a free slot */
			break;
		}
	}

	/* The whole block has been searched or ENTER has a free slot. */
	if (e_hit) break;	/* e_hit set if ENTER can be performed now */
	put_block(bp, DIRECTORY_BLOCK);	/* otherwise, continue searching dir */
  }

  /* The whole directory has now been searched. */
  if (flag != ENTER) return(ENOENT);

  /* This call is for ENTER.  If no free slot has been found so far, try to
   * extend directory.
   */
  if (e_hit == FALSE) { /* directory is full and no room left in last block */
	new_slots ++;		/* increase directory size by 1 entry */
	if (new_slots == 0) return(EFBIG); /* dir size limited by slot count */
	if ( (bp = new_block(ldir_ptr, ldir_ptr->i_size)) == NIL_BUF)
		return(err_code);
	dp = &bp->b_dir[0];
  }

  /* 'bp' now points to a directory block with space. 'dp' points to slot. */
  copy(dp->d_name, string, NAME_SIZE);
  dp->d_inum = *numb;
  bp->b_dirt = DIRTY;
  put_block(bp, DIRECTORY_BLOCK);
  ldir_ptr->i_modtime = clock_time();
  ldir_ptr->i_dirt = DIRTY;
  if (new_slots > old_slots)
	ldir_ptr->i_size = (file_pos) new_slots * DIR_ENTRY_SIZE;
  return(OK);
}


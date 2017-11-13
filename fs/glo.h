/* File System global variables */
EXTERN struct fproc *fp;	/* pointer to caller's fproc struct */
EXTERN int super_user;		/* 1 if caller is super_user, else 0 */
EXTERN int dont_reply;		/* normally 0; set to 1 to inhibit reply */
EXTERN int susp_count;		/* number of procs suspended on pipe */
EXTERN int reviving;		/* number of pipe processes to be revived */
EXTERN file_pos rdahedpos;	/* position to read ahead */
EXTERN struct inode *rdahed_inode;	/* pointer to inode to read ahead */

/* The parameters of the call are kept here. */
EXTERN message m;		/* the input message itself */
EXTERN message m1;		/* the output message used for reply */
EXTERN int who;			/* caller's proc number */
EXTERN int fs_call;		/* system call number */
EXTERN char user_path[MAX_PATH];/* storage for user path name */

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

EXTERN char fstack[FS_STACK_BYTES];	/* the File System's stack. */

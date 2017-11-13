/* This file handles the EXEC system call.  It performs the work as follows:
 *    - see if the permissions allow the file to be executed
 *    - read the header and extract the sizes
 *    - fetch the initial args and environment from the user space
 *    - allocate the memory for the new process
 *    - copy the initial stack from MM to the process
 *    - read in the text and data segments and copy to the process
 *    - take care of setuid and setgid bits
 *    - fix up 'mproc' table
 *    - tell kernel about EXEC
 *
 *   The only entry point is do_exec.
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/error.h"
#include "../h/stat.h"
#include "const.h"
#include "glo.h"
#include "mproc.h"
#include "param.h"

#define MAGIC    0x04000301L	/* magic number with 2 bits masked off */
#define SEP      0x00200000L	/* value for separate I & D */
#define TEXTB              2	/* location of text size in header */
#define DATAB              3	/* location of data size in header */
#define BSSB               4	/* location of bss size in header */
#define TOTB               6	/* location of total size in header */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec()
{
/* Perform the exece(name, argv, envp) call.  The user library builds a
 * complete stack image, including pointers, args, environ, etc.  The stack
 * is copied to a buffer inside MM, and then to the new core image.
 */

  register struct mproc *rmp;
  int m, r, fd, ft;
  char mbuf[MAX_ISTACK_BYTES];	/* buffer for stack and zeroes */
  union u {
	char name_buf[MAX_PATH];	/* the name of the file to exec */
	char zb[ZEROBUF_SIZE];	/* used to zero bss */
  } u;
  char *new_sp;
  vir_bytes src, dst, text_bytes, data_bytes, bss_bytes, stk_bytes, vsp;
  phys_bytes tot_bytes;		/* total space for program, including gap */
  vir_clicks sc;
  struct stat s_buf;

  /* Do some validity checks. */
  rmp = mp;
  stk_bytes = (vir_bytes) stack_bytes;
  if (stk_bytes > MAX_ISTACK_BYTES) return(ENOMEM);	/* stack too big */
  if (exec_len <= 0 || exec_len > MAX_PATH) return(EINVAL);

  /* Get the exec file name and see if the file is executable. */
  src = (vir_bytes) exec_name;
  dst = (vir_bytes) u.name_buf;
  r = mem_copy(who, D, (long) src, MM_PROC_NR, D, (long) dst, (long) exec_len);
  if (r != OK) return(r);	/* file name not in user data segment */
  tell_fs(CHDIR, who, 0, 0);	/* temporarily switch to user's directory */
  fd = allowed(u.name_buf, &s_buf, X_BIT);	/* is file executable? */
  tell_fs(CHDIR, 0, 1, 0);	/* switch back to MM's own directory */
  if (fd < 0) return(fd);	/* file was not executable */

  /* Read the file header and extract the segment sizes. */
  sc = (stk_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  m = read_header(fd, &ft, &text_bytes, &data_bytes, &bss_bytes, &tot_bytes,sc);
  if (m < 0) {
	close(fd);		/* something wrong with header */
	return(ENOEXEC);
  }

  /* Fetch the stack from the user before destroying the old core image. */
  src = (vir_bytes) stack_ptr;
  dst = (vir_bytes) mbuf;
  r = mem_copy(who, D, (long) src, MM_PROC_NR, D, (long) dst, (long) stk_bytes);
  if (r != OK) {
	close(fd);		/* can't fetch stack (e.g. bad virtual addr) */
	return(EACCES);
  }

  /* Allocate new memory and release old memory.  Fix map and tell kernel. */
  r = new_mem(text_bytes, data_bytes, bss_bytes, stk_bytes, tot_bytes,
							u.zb, ZEROBUF_SIZE);
  if (r != OK) {
	close(fd);		/* insufficient core or program too big */
	return(r);
  }

  /* Patch up stack and copy it from MM to new core image. */
  vsp = (vir_bytes) rmp->mp_seg[S].mem_vir << CLICK_SHIFT;
  patch_ptr(mbuf, vsp);
  src = (vir_bytes) mbuf;
  r = mem_copy(MM_PROC_NR, D, (long) src, who, D, (long) vsp, (long) stk_bytes);
  if (r != OK) panic("do_exec stack copy err", NO_NUM);

  /* Read in text and data segments. */
  load_seg(fd, T, text_bytes);
  load_seg(fd, D, data_bytes);
  close(fd);			/* don't need exec file any more */

  /* Take care of setuid/setgid bits. */
  if (s_buf.st_mode & I_SET_UID_BIT) {
	rmp->mp_effuid = s_buf.st_uid;
	tell_fs(SETUID, who, (int) rmp->mp_realuid, (int) rmp->mp_effuid);
  }
  if (s_buf.st_mode & I_SET_GID_BIT) {
	rmp->mp_effgid = s_buf.st_gid;
	tell_fs(SETGID, who, (int) rmp->mp_realgid, (int) rmp->mp_effgid);
  }

  /* Fix up some 'mproc' fields and tell kernel that exec is done. */
  rmp->mp_catch = 0;		/* reset all caught signals */
  rmp->mp_flags &= ~SEPARATE;	/* turn off SEPARATE bit */
  rmp->mp_flags |= ft;		/* turn it on for separate I & D files */
  new_sp = (char *) vsp;
  sys_exec(who, new_sp);
  return(OK);
}


/*===========================================================================*
 *				read_header				     *
 *===========================================================================*/
PRIVATE int read_header(fd, ft, text_bytes, data_bytes, bss_bytes, tot_bytes,sc)
int fd;				/* file descriptor for reading exec file */
int *ft;			/* place to return ft number */
vir_bytes *text_bytes;		/* place to return text size */
vir_bytes *data_bytes;		/* place to return initialized data size */
vir_bytes *bss_bytes;		/* place to return bss size */
phys_bytes *tot_bytes;		/* place to return total size */
vir_clicks sc;			/* stack size in clicks */
{
/* Read the header and extract the text, data, bss and total sizes from it. */

  int m, ct;
  vir_clicks tc, dc, s_vir, dvir;
  phys_clicks totc;
  long buf[HDR_SIZE/sizeof(long)];

  /* Read the header and check the magic number.  The standard MINIX header 
   * consists of 8 longs, as follows:
   *	0: 0x04100301L (combined I & D space) or 0x04200301L (separate I & D)
   *	1: 0x00000020L 
   *	2: size of text segments in bytes
   *	3: size of initialized data segment in bytes
   *	4: size of bss in bytes
   *	5: 0x00000000L
   *	6: total memory allocated to program (text, data and stack, combined)
   *	7: 0x00000000L
   * The longs are represented low-order byte first and high-order byte last.
   * The first byte of the header is always 0x01, followed by 0x03.
   * The header is followed directly by the text and data segments, whose sizes
   * are given in the header.
   */

  if (read(fd, buf, HDR_SIZE) != HDR_SIZE) return(ENOEXEC);
  if ( (buf[0] & 0xFF0FFFFFL) != MAGIC) return(ENOEXEC);
  *ft = (buf[0] & SEP ? SEPARATE : 0);	/* separate I & D or not */

  /* Get text and data sizes. */
  *text_bytes = (vir_bytes) buf[TEXTB];	/* text size in bytes */
  *data_bytes = (vir_bytes) buf[DATAB];	/* data size in bytes */
  if (*ft != SEPARATE) {
	/* If I & D space is not separated, it is all considered data. Text=0 */
	*data_bytes += *text_bytes;
	*text_bytes = 0;
  }

  /* Get bss and total sizes. */
  *bss_bytes = (vir_bytes) buf[BSSB];	/* bss size in bytes */
  *tot_bytes = buf[TOTB];	/* total bytes to allocate for program */
  if (*tot_bytes == 0) return(ENOEXEC);

  /* Check to see if segment sizes are feasible. */
  tc = (*text_bytes + CLICK_SHIFT - 1) >> CLICK_SHIFT;
  dc = (*data_bytes + *bss_bytes + CLICK_SHIFT - 1) >> CLICK_SHIFT;
  totc = (*tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  if (dc >= totc) return(ENOEXEC);	/* stack must be at least 1 click */
  dvir = (*ft == SEPARATE ? 0 : tc);
  s_vir = dvir + (totc - sc);
  m = size_ok(*ft, tc, dc, sc, dvir, s_vir);
  ct = buf[1] & BYTE;		/* header length */
  if (ct > HDR_SIZE) read(fd, buf, ct - HDR_SIZE);	/* skip unused hdr */
  return(m);
}


/*===========================================================================*
 *				new_mem					     *
 *===========================================================================*/
PRIVATE int new_mem(text_bytes, data_bytes, bss_bytes,stk_bytes,tot_bytes,bf,zs)
vir_bytes text_bytes;		/* text segment size in bytes */
vir_bytes data_bytes;		/* size of initialized data in bytes */
vir_bytes bss_bytes;		/* size of bss in bytes */
vir_bytes stk_bytes;		/* size of initial stack segment in bytes */
phys_bytes tot_bytes;		/* total memory to allocate, including gap */
char bf[ZEROBUF_SIZE];		/* buffer to use for zeroing data segment */
int zs;				/* true size of 'bf' */
{
/* Allocate new memory and release the old memory.  Change the map and report
 * the new map to the kernel.  Zero the new core image's bss, gap and stack.
 */

  register struct mproc *rmp;
  char *rzp;
  vir_bytes vzb;
  vir_clicks text_clicks, data_clicks, gap_clicks, stack_clicks, tot_clicks;
  phys_clicks new_base, old_clicks;
  phys_bytes bytes, base, count, bss_offset;
  extern phys_clicks alloc_mem();
  extern phys_clicks max_hole();

  /* Acquire the new memory.  Each of the 4 parts: text, (data+bss), gap,
   * and stack occupies an integral number of clicks, starting at click
   * boundary.  The data and bss parts are run together with no space.
   */

  text_clicks = (text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  data_clicks = (data_bytes + bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  stack_clicks = (stk_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  tot_clicks = (tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  gap_clicks = tot_clicks - data_clicks - stack_clicks;
  if ( (int) gap_clicks < 0) return(ENOMEM);

  /* Check to see if there is a hole big enough.  If so, we can risk first
   * releasing the old core image before allocating the new one, since we
   * know it will succeed.  If there is not enough, return failure.
   */
  if (text_clicks + tot_clicks > max_hole()) return(EAGAIN);

  /* There is enough memory for the new core image.  Release the old one. */
  rmp = mp;
  old_clicks = (phys_clicks) rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len;
  if (rmp->mp_flags & SEPARATE) old_clicks += rmp->mp_seg[T].mem_len;
  free_mem(rmp->mp_seg[T].mem_phys, old_clicks);	/* free the memory */

  /* We have now passed the point of no return.  The old core image has been
   * forever lost.  The call must go through now.  Set up and report new map.
   */
  new_base = alloc_mem(text_clicks + tot_clicks);	/* new core image */
  if (new_base == NO_MEM) panic("MM hole list is inconsistent", NO_NUM);
  rmp->mp_seg[T].mem_vir = 0;
  rmp->mp_seg[T].mem_len = text_clicks;
  rmp->mp_seg[T].mem_phys = new_base;
  rmp->mp_seg[D].mem_vir = 0;
  rmp->mp_seg[D].mem_len = data_clicks;
  rmp->mp_seg[D].mem_phys = new_base + text_clicks;
  rmp->mp_seg[S].mem_vir = rmp->mp_seg[D].mem_vir + data_clicks + gap_clicks;
  rmp->mp_seg[S].mem_len = stack_clicks;
  rmp->mp_seg[S].mem_phys = rmp->mp_seg[D].mem_phys + data_clicks + gap_clicks;
  sys_newmap(who, rmp->mp_seg);	/* report new map to the kernel */

  /* Zero the bss, gap, and stack segment. Start just above text.  */
  for (rzp = &bf[0]; rzp < &bf[zs]; rzp++) *rzp = 0;	/* clear buffer */
  bytes = (phys_bytes) (data_clicks + gap_clicks + stack_clicks) << CLICK_SHIFT;
  vzb = (vir_bytes) bf;
  base = (long) rmp->mp_seg[T].mem_phys + rmp->mp_seg[T].mem_len;
  base = base << CLICK_SHIFT;
  bss_offset = (data_bytes >> CLICK_SHIFT) << CLICK_SHIFT;
  base += bss_offset;
  bytes -= bss_offset;

  while (bytes > 0) {
	count = (long) MIN(bytes, (phys_bytes) zs);
	if (mem_copy(MM_PROC_NR, D, (long) vzb, ABS, 0, base, count) != OK)
		panic("new_mem can't zero", NO_NUM);
	base += count;
	bytes -= count;
  }
  return(OK);
}


/*===========================================================================*
 *				patch_ptr				     *
 *===========================================================================*/
PRIVATE patch_ptr(stack, base)
char stack[MAX_ISTACK_BYTES];	/* pointer to stack image within MM */
vir_bytes base;			/* virtual address of stack base inside user */
{
/* When doing an exec(name, argv, envp) call, the user builds up a stack
 * image with arg and env pointers relative to the start of the stack.  Now
 * these pointers must be relocated, since the stack is not positioned at
 * address 0 in the user's address space.
 */

  char **ap, flag;
  vir_bytes v;

  flag = 0;			/* counts number of 0-pointers seen */
  ap = (char **) stack;		/* points initially to 'nargs' */
  ap++;				/* now points to argv[0] */
  while (flag < 2) {
	if (ap >= (char **) &stack[MAX_ISTACK_BYTES]) return;	/* too bad */
	if (*ap != NIL_PTR) {
		v = (vir_bytes) *ap;	/* v is relative pointer */
		v += base;		/* relocate it */
		*ap = (char *) v;	/* put it back */
	} else {
		flag++;
	}
	ap++;
  }
}


/*===========================================================================*
 *				load_seg				     *
 *===========================================================================*/
PRIVATE load_seg(fd, seg, seg_bytes)
int fd;				/* file descriptor to read from */
int seg;			/* T or D */
vir_bytes seg_bytes;		/* how big is the segment */
{
/* Read in text or data from the exec file and copy to the new core image.
 * This procedure is a little bit tricky.  The logical way to load a segment
 * would be to read it block by block and copy each block to the user space
 * one at a time.  This is too slow, so we do something dirty here, namely
 * send the user space and virtual address to the file system in the upper
 * 10 bits of the file descriptor, and pass it the user virtual address
 * instead of a MM address.  The file system copies the whole segment
 * directly to user space, bypassing MM completely.
 */

  int new_fd, bytes;
  char *ubuf_ptr;

  if (seg_bytes == 0) return;	/* text size for combined I & D is 0 */
  new_fd = (who << 8) | (seg << 6) | fd;
  ubuf_ptr = (char *) (mp->mp_seg[seg].mem_vir << CLICK_SHIFT);
  bytes = (int) seg_bytes;
  read(new_fd, ubuf_ptr, bytes);
}

/* Constants used by the Memory Manager. */

#define ZEROBUF_SIZE	1024	/* buffer size for erasing memory */

/* Size of MM's stack depends mostly on do_exec(). */
#if ZEROBUF_SIZE > MAX_PATH
#define MM_STACK_BYTES	MAX_ISTACK_BYTES + ZEROBUF_SIZE + 384
#else
#define MM_STACK_BYTES	MAX_ISTACK_BYTES + MAX_PATH + 384
#endif

#define NO_MEM (phys_clicks)0	/* returned by alloc_mem() with mem is up */

#ifdef i8088
#define PAGE_SIZE	  16	/* how many bytes in a page */
#define MAX_PAGES       4096	/* how many pages in the virtual addr space */
#define HDR_SIZE	  32	/* # bytes in the exec file header */
#endif

#define printf        printk

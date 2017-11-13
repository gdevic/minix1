/* The 'pc_psw' struct is machine dependent.  It must contain the information
 * pushed onto the stack by an interrupt, in the same format as the hardware
 * creates and expects.  It is used for storing the interrupt status after a
 * trap or interrupt, as well as for causing interrupts for signals.
 */


#ifdef i8088
struct pc_psw {
  int (*pc)();			/* storage for program counter */
  phys_clicks cs;		/* code segment register */
  unsigned psw;			/* program status word */
};

/* This struct is used to build data structure pushed by kernel upon signal. */
struct sig_info {
  int signo;			/* sig number at end of stack */
  struct pc_psw sigpcpsw;
};
#endif

#include "../include/lib.h"

char *nullptr[1];		/* the EXEC calls need a zero pointer */

PUBLIC int execl(name, arg0)
char *name;
char *arg0;
{
  return execve(name, &arg0, nullptr);
}

PUBLIC int execle(name, argv)
char *name, *argv;
{
  char **p;
  p = (char **) &argv;
  while (*p++) /* null statement */ ;
  return execve(name, &argv, *p);
}

PUBLIC int execv(name, argv)
char *name, *argv[];
{
  return execve(name, argv, nullptr);
}


PUBLIC int execve(name, argv, envp)
char *name;			/* pointer to name of file to be executed */
char *argv[];			/* pointer to argument array */
char *envp[];			/* pointer to environment */
{
  char stack[MAX_ISTACK_BYTES];
  char **argorg, **envorg, *hp, **ap, *p;
  int i, nargs, nenvps, stackbytes, ptrsize, offset;

  /* Count the argument pointers and environment pointers. */
  nargs = 0;
  nenvps = 0;
  argorg = argv;
  envorg = envp;
  while (*argorg++ != NIL_PTR) nargs++;
  while (*envorg++ != NIL_PTR) nenvps++;
  ptrsize = sizeof(NIL_PTR);

  /* Prepare to set up the initial stack. */
  hp = &stack[(nargs + nenvps + 3) * ptrsize];
  if (hp + nargs + nenvps >= &stack[MAX_ISTACK_BYTES]) return(E2BIG);
  ap = (char **) stack;
  *ap++ = (char *) nargs;

  /* Prepare the argument pointers and strings. */
  for (i = 0; i < nargs; i++) {
	offset = hp - stack;
	*ap++ = (char *) offset;
	p = *argv++;
	while (*p) {
		*hp++ = *p++;
		if (hp >= &stack[MAX_ISTACK_BYTES]) return(E2BIG);
	}
	*hp++ = (char) 0;
  }
  *ap++ = NIL_PTR;

  /* Prepare the environment pointers and strings. */
  for (i = 0; i < nenvps; i++) {
	offset = hp - stack;
	*ap++ = (char *) offset;
	p = *envp++;
	while (*p) {
		*hp++ = *p++;
		if (hp >= &stack[MAX_ISTACK_BYTES]) return(E2BIG);
	}
	*hp++ = (char) 0;
  }
  *ap++ = NIL_PTR;
  stackbytes = ( ( (hp - stack) + ptrsize - 1)/ptrsize) * ptrsize;
  return callm1(MM_PROC_NR, EXEC, len(name), stackbytes, 0,name, stack,NIL_PTR);
}


PUBLIC execn(name)
char *name;			/* pointer to file to be exec'd */
{
/* Special version used when there are no args and no environment.  This call
 * is principally used by INIT, to avoid having to allocate MAX_ISTACK_BYTES.
 */

  char stack[4];

  stack[0] = 0;
  stack[1] = 0;
  stack[2] = 0;
  stack[3] = 0;
  return callm1(MM_PROC_NR, EXEC, len(name), 4, 0, name, stack, NIL_PTR);
}

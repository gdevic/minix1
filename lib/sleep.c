#include "../include/lib.h"
#include "../include/signal.h"

PRIVATE alfun(){}		/* used with sleep() below */
PUBLIC sleep(n)
int n;
{
/* sleep(n) pauses for 'n' seconds by scheduling an alarm interrupt. */
  signal(SIGALRM, alfun);
  alarm(n);
  pause();
}

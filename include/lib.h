#include "../h/const.h"
#include "../h/type.h"
#include "../h/error.h"
#include "../h/callnr.h"

extern message M;

#define MM                 0
#define FS                 1

extern int callm1(), callm3(), callx(), len();
extern int errno;
extern int begsig();		/* interrupts all vector here */

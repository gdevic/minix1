/* The object file of "table.c" contains all the data.  In the *.h files, 
 * declared variables appear with EXTERN in front of them, as in
 *
 *    EXTERN int x;
 *
 * Normally EXTERN is defined as extern, so when they are included in another
 * file, no storage is allocated.  If the EXTERN were not present, but just
 * say,
 *
 *    int x;
 *
 * then including this file in several source files would cause 'x' to be
 * declared several times.  While some linkers accept this, others do not,
 * so they are declared extern when included normally.  However, it must
 * be declared for real somewhere.  That is done here, but redefining
 * EXTERN as the null string, so the inclusion of all the *.h files in
 * table.c actually generates storage for them.  All the initialized
 * variables are also declared here, since
 *
 * extern int x = 4;
 *
 * is not allowed.  If such variables are shared, they must also be declared
 * in one of the *.h files without the initialization.
 */

#include "../h/const.h"
#include "../h/type.h"
#include "const.h"
#include "type.h"
#undef   EXTERN
#define  EXTERN
#include "glo.h"
#include "proc.h"

extern int sys_task(), clock_task(), mem_task(), floppy_task(),
           winchester_task(), tty_task(), printer_task();

/* The startup routine of each task is given below, from -NR_TASKS upwards.
 * The order of the names here MUST agree with the numerical values assigned to
 * the tasks in ../h/com.h.
 */
int (*task[NR_TASKS+INIT_PROC_NR+1])() = {
 printer_task, tty_task, winchester_task, floppy_task, mem_task,
 clock_task, sys_task, 0, 0, 0, 0
};

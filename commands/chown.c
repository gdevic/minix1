/*
 * chown username file ...
 * 
 * By Patrick van Kleef
 *
 */

#include "pwd.h"
#include "../h/type.h"
#include "stat.h"
#include "stdio.h"

main (argc, argv)
int   argc;
char *argv[];
{
	int     i,
	status = 0;
	struct passwd  *pwd, *getpwnam ();
	struct stat stbuf;

	if (argc < 3) {
		fprintf (stderr,"Usage: chown uid file ...\n");
		exit (1);
	}

	if ((pwd = getpwnam (argv[1])) == 0) {
		fprintf (stderr,"Unknown user id: %s\n", argv[1]);
		exit (4);
	}

	for (i = 2; i < argc; i++) {
		if (stat (argv[i], &stbuf) < 0) {
			perror (argv[i]);
			status++;
		}
		else
			if (chown (argv[i], pwd -> pw_uid, stbuf.st_gid) < 0) {
				fprintf (stderr,"%s: not changed\n", argv[i]);
				status++;
			}
	}
	exit (status);
}

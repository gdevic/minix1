/* mv - move files		Author: Adri Koppes */

#include "signal.h"
#include "stat.h"

int     error = 0;
struct stat st;

main (argc, argv)
int     argc;
char  **argv;
{
    char   *destdir;

    if (argc < 3) {
	std_err ("Usage: mv file1 file2 or mv dir1 dir2 or mv file1 file2 ... dir\n");
	exit (1);
    }
    if (argc == 3) {
	if (stat (argv[1], &st)) {
	    std_err ("mv: ");
	    std_err (argv[1]);
	    std_err (" doesn't exist\n");
	    exit (1);
	}
	if ((st.st_mode & S_IFMT) == S_IFDIR) {
	    if (!stat(argv[2], &st) && (st.st_mode & S_IFMT) == S_IFDIR) {
		std_err ("mv: target ");
		std_err (argv[2]);
		std_err (" exists\n");
		exit (1);
	    }
	}
	else {
	    setgid (getgid ());
	    setuid (getuid ());
	}
	move (argv[1], argv[2]);
    }
    else {
	setgid (getgid ());
	setuid (getuid ());
	destdir = argv[--argc];
	if (stat (destdir, &st)) {
	    std_err ("mv: target directory ");
	    std_err (destdir);
	    std_err (" doesn't exist\n");
	    exit(1);
	}
	if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    std_err ("mv: target ");
	    std_err (destdir);
	    std_err (" not a directory\n");
	    exit (1);
	}
	while (--argc)
	    move (*++argv, destdir);
    }
    if (error) exit (1);
    exit(0);
}

move (old, new)
char   *old,
       *new;
{
    int     retval;

    if (!stat (new, &st))
	if((st.st_mode & S_IFMT) != S_IFDIR)
	    unlink (new);
    else {
	char name[64], *p, *rindex();

	if ((strlen(old) + strlen(new) + 2) > 64) {
		cant(old);
		error++;
		return;
	}
	strcpy(name, new);
	strcat(name, "/");
	p = rindex(old, '/');
	strcat(name, p ? p : old);
	new = name;
    }
    stat (old, &st);
    if (link (old, new))
	if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    switch (fork ()) {
		case 0: 
		    execl ("/bin/cp", "cp", old, new, 0);
		    cant(old);
		case -1: 
		    std_err ("mv: can't fork\n");
		    exit (1);
		default:
		    wait (&retval);
		    if (retval)
			cant(old);
	    }
	} else
	    cant(old);
    utime (new, &st.st_atime);
    unlink(old);
}

cant(name)
char *name;
{
	std_err("mv: can't move ");
	std_err (name);
	std_err ("\n");
	exit (1);
}

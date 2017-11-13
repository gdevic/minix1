/* rmdir - remove a directory		Author: Adri Koppes */

#include "../include/signal.h"
#include "../include/stat.h"

struct direct {
    unsigned short  d_ino;
    char    d_name[14];
};
int     error = 0;

main (argc, argv)
register int argc;
register char  **argv;
{
    if (argc < 2) {
	prints ("Usage: rmdir dir ...\n");
	exit (1);
    }
    signal (SIGHUP, SIG_IGN);
    signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTERM, SIG_IGN);
    while (--argc)
	remove (*++argv);
    if (error)
	exit (1);
}

remove (dirname)
char   *dirname;
{
    struct direct   d;
    struct stat s,
                cwd;
    register int fd = 0, sl = 0;
    char    dots[128];

    if (stat (dirname, &s)) {
	stderr2(dirname, " doesn't exist\n");
	error++;
	return;
    }
    if ((s.st_mode & S_IFMT) != S_IFDIR) {
	stderr2(dirname, " not a directory\n");
	error++;
	return;
    }
    strcpy (dots, dirname);
    while (dirname[fd])
	if (dirname[fd++] == '/')
	    sl = fd;
    dots[sl] = '\0';
    if (access (dots, 2)) {
	stderr2(dirname, " no permission\n");
	error++;
	return;
    }
    stat ("", &cwd);
    if ((s.st_ino == cwd.st_ino) && (s.st_dev == cwd.st_dev)) {
	std_err ("rmdir: can't remove current directory\n");
	error++;
	return;
    }
    if ((fd = open (dirname, 0)) < 0) {
	stderr2("can't read ", dirname);
	std_err("\n");
	error++;
	return;
    }
    while (read (fd, (char *) & d, sizeof (struct direct)) == sizeof (struct direct))
    if (d.d_ino != 0)
	if (strcmp (d.d_name, ".") && strcmp (d.d_name, "..")) {
            stderr2(dirname, " not empty\n");
	    close(fd);
	    error++;
	    return;
	}
    close (fd);
    strcpy (dots, dirname);
    strcat (dots, "/.");
    unlink (dots);
    strcat (dots, ".");
    unlink (dots);
    if (unlink (dirname)) {
	stderr2("can't remove ", dirname);
	std_err("\n");
	error++;
	return;
    }
}

stderr2(s1, s2)
char *s1, *s2;
{
	std_err("rmdir: ");
	std_err(s1);
	std_err(s2);
}

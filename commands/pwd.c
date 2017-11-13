/* pwd - print working directory		Author: Adri Koppes */

#include "stat.h"

struct direct {
    unsigned short d_ino;
    char d_name[14];
}
main()
{
	register int fd;
	register char name[128], *n;
	char *last_index();
	struct stat s, st;
	struct direct d;

	*name = 0;
	stat(".", &s);
	do {
		if ((fd = open("..",0)) < 0) {
			prints("Can't open ..\n");
			exit(1);
		}
		st.st_dev = s.st_dev;
		st.st_ino = s.st_ino;
		st.st_mode = s.st_mode;
		st.st_nlink = s.st_nlink;
		st.st_uid = s.st_uid;
		st.st_gid = s.st_gid;
		st.st_rdev = s.st_rdev;
		st.st_size = s.st_size;
		st.st_atime = s.st_atime;
		st.st_mtime = s.st_mtime;
		st.st_ctime = s.st_ctime;
		stat("..", &s);
		chdir("..");
		if (s.st_dev == st.st_dev)
			do
				if (read(fd, (char *)&d, sizeof(struct direct)) < sizeof(struct direct)) {
					prints("Can't read ..\n");
					exit(1);
				}
			while(d.d_ino != st.st_ino);
		else
			do {
				if (read(fd, (char *)&d, sizeof(struct direct)) < sizeof(struct direct)) {
					prints("Can't read ..\n");
					exit(1);
				}
				stat(d.d_name, &s);
			} while ((s.st_dev != st.st_dev) || (s.st_ino != st.st_ino));
		close(fd);
		if (strcmp(".",d.d_name)) {
			strcat(name,"/");
			strcat(name,d.d_name);
		}
	} while ((s.st_ino != st.st_ino) || (s.st_dev != st.st_dev));
	if (!*name)
		prints("/");
	else
		while (n = last_index(name, '/')) {
			prints(n);
			*n = 0;
		}
	prints(name);
	prints("\n");
	exit(0);
}


char   *
last_index (string, ch)
register char  *string;
register char   ch;
{
    register char  *retval = 0;

    for (; *string; *string++)
	if (*string == ch)
	    retval = string;
    return (retval);
}

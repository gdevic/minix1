/* login - log into the system		Author: Patrick van Kleef */

#include "signal.h"
#include "sgtty.h"
#include "pwd.h"

main() 
{
	char    buf[30],
		buf1[30],
	       *crypt();
	int     n, n1, bad;
	struct sgttyb args;
	struct passwd *pwd, *getpwnam();

	args.sg_kill = '@';
	args.sg_erase = '\b';
	args.sg_flags = 06030;
	ioctl (0, TIOCSETP, &args);


	/* Get login name and passwd. */
	for (;;) {
		bad = 0;
		do {
			write(1,"login: ",7);
			n = read (0, buf, 30);
		} while (n < 2);
		buf[n - 1] = 0;

		/* Look up login/passwd. */
		if ((pwd = getpwnam (buf)) == 0)
			bad++;

		if (bad || strlen (pwd->pw_passwd) != 0) {
			args.sg_flags = 06020;
			ioctl (0, TIOCSETP, &args);
			write(1,"Password: ",10);
			n1 = read (0, buf1, 30);
			buf1[n1 - 1] = 0;
			write(1,"\n",1);
			args.sg_flags = 06030;
			ioctl (0, TIOCSETP, &args);
			if (bad || strcmp (pwd->pw_passwd, crypt(buf1, pwd->pw_passwd))) {
				write (1,"Login incorrect\n",16);
				continue;
			}
		}

		/* Successful login. */
		setgid (pwd->pw_gid);
		setuid (pwd->pw_uid);
		chdir (pwd->pw_dir);
		if (pwd->pw_shell) {
			execl(pwd->pw_shell, "-", 0);
		}
		execl("/bin/sh", "-", 0);
		write(1,"exec failure\n",13);
	}
}

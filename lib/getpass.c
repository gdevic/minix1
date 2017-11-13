#include "../h/sgtty.h"

static char pwdbuf[9];

char * getpass(prompt)
char *prompt;
{
	int i = 0;
	struct sgttyb tty;

	prints(prompt);
	ioctl(0, TIOCGETP, &tty);
	tty.sg_flags = 06020;
	ioctl(0, TIOCSETP, &tty);
	i = read(0, pwdbuf, 9);
	while (pwdbuf[i - 1] != '\n')
		read(0, &pwdbuf[i - 1], 1);
	pwdbuf[i - 1] = '\0';
	tty.sg_flags = 06030;
	ioctl(0, TIOCSETP, &tty);
	prints("\n");
	return(pwdbuf);
}

/*
 * chmod [mode] files
 *  change mode of files
 * 
 *  by Patrick van Kleef
 */

main (argc, argv)
int   argc;
char *argv[];
{
	int i;
	int status = 0;
	int newmode;

	if (argc < 3) {
		Usage ();
	}

	newmode = oatoi (argv[1]);

	for (i = 2; i < argc; i++) {
		if (access (argv[i], 0)) {
			prints ("chmod: can't access %s\n", argv[i]);
			status++;
		}
		else
			if (chmod (argv[i], newmode) < 0) {
				prints ("chmod: can't change %s\n", argv[i]);
				status++;
			}
	}
	exit (status);
}

oatoi (arg)
char   *arg;
{
	register    c,
	i;

	i = 0;
	while ((c = *arg++) >= '0' && c <= '7')
		i = (i << 3) + (c - '0');
	if (c != '\0')
		Usage ();
	return (i);
}

Usage () {
	prints ("Usage: chmod [mode] file ...\n");
	exit (255);
}

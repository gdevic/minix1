/* cmp - compare two files	Authors: Paul Polderman & Michiel Huisjes */

#define BLOCK_SIZE 8192
typedef unsigned short unshort;


char *file_1, *file_2;
char buf[2][BLOCK_SIZE];
unshort lflag, sflag;

main(argc, argv)
int argc;
char *argv[];
{
  int fd1, fd2, i, exit_status;

  if (argc < 3 || argc > 4) usage();
  lflag = 0; sflag = 0;
  fd1 = -1; fd2 = -1;
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
  	switch (argv[i][1]) {
  		case 'l' :
  			lflag++;
  			break;
  		case 's' :
  			sflag++;
  			break;
  		case '\0' :
  			fd1 = 0;	/* First file is stdin. */
  			break;
  		default :
  			usage();
  	}
  }

  if (fd1 == -1) {					/* Open first file. */
  	if (i == argc || (fd1 = open(argv[i], 0)) < 0) 
		cantopen(argv[i]);
  	 else
  		file_1 = argv[i++];
  }
  else
  	file_1 = "stdin";

  if (i == argc || (fd2 = open(argv[i], 0)) < 0)	/* Open second file. */
  	cantopen(argv[i]);
  file_2 = argv[i];

  exit_status = cmp(fd1, fd2);

  close (fd1);
  close (fd2);

  exit (exit_status);
}




#define	ONE	0
#define TWO	1

cmp(fd1, fd2)
int fd1, fd2;
{
  register long char_cnt, line_cnt;
  register unshort i;
  unshort n1, n2, exit_status;
  int c1, c2;

  char_cnt = 1L; line_cnt = 1L;
  exit_status = 0;
  do {
	n1 = read(fd1, buf[ONE], BLOCK_SIZE);
	n2 = read(fd2, buf[TWO], BLOCK_SIZE);
	i = 0;
	while (i < n1 && i < n2) {	/* Check buffers on equality */
		if (buf[ONE][i] != buf[TWO][i]) {
			if (sflag)	/* Exit silently */
				return(1);
			if (!lflag) {
				printf("%s %s differ: char %D, line %D\n",
					file_1, file_2, char_cnt, line_cnt);
				return(1);
			}
			c1 = buf[ONE][i];
			c2 = buf[TWO][i];
			printf("\t%D %3o %3o\n",
					char_cnt, c1&0377, c2&0377);
			exit_status = 1;
		}
		if (buf[ONE][i] == '\n')
			line_cnt++;
		i++;
		char_cnt++;
	}
	if (n1 != n2) {		/* EOF on one of the input files. */
		if (n1 < n2)
			prints("cmp: EOF on %s\n", file_1);
		else
			prints("cmp: EOF on %s\n", file_2);
		return(1);
	}
  } while (n1 > 0 && n2 > 0);	/* While not EOF on any file */
  return(exit_status);
}



usage()
{
  std_err("Usage: cmp [-ls] file1 file2\n");
  exit(2);
}


cantopen(s)
char *s;
{
  std_err("cmp: cannot open ");
  std_err(s);
  std_err("\n");
  exit(1);
}

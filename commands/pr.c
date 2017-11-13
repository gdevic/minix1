/*
 * Pr - print files
 * 
 * Author: Michiel Huisjes.
 * 
 * Usage: pr [+page] [-columns] [-h header] [-w with] [-l length] [-nt] [files]
 *        -t : Do not print the 5 line header and trailer at the page.
 *	  -n : Turn on line numbering.
 *        +page    : Start printing at page n.
 *        -columns : Print files in n-colums.
 *        -l length: Take the length of the page to be n instead of 66
 *        -h header: Take next argument as page header.
 *        -w with  : Take the width of the page to be n instead of default 72
 */

#include "stdio.h"

char *colbuf;

#define DEF_LENGTH	66
#define DEF_WIDTH	72

typedef char BOOL;

#define FALSE		0
#define TRUE		1

#define NIL_PTR		((char *) 0)

char *header;
BOOL no_header;
BOOL number;
short columns;
short cwidth;
short start_page = 1;
short width = DEF_WIDTH;
short length = DEF_LENGTH;

char output[1024];
FILE *fopen();

main(argc, argv)
int argc;
char *argv[];
{
  FILE *file;
  char *ptr;
  int index = 1;

  while (argc > index) {
  	ptr = argv[index++];
  	if (*ptr == '+') {
  		start_page = atoi(++ptr);
  		continue;
  	}
  	if (*ptr != '-') {
  		index--;
  		break;
  	}
  	if (*++ptr >= '0' && *ptr <= '9') {
  		columns = atoi(ptr);
  		continue;
  	}
  	while (*ptr)
  		switch (*ptr++) {
  			case 't':
  				no_header = TRUE;
  				break;
  			case 'n':
  				number = TRUE;
  				break;
  			case 'h':
  				header = argv[index++];
  				break;
  			case 'w':
  				width = atoi(ptr);
				*ptr = '\0';
  				break;
  			case 'l':
  				length = atoi(ptr);
				*ptr = '\0';
  				break;
  			default:
  				fprintf(stderr, "Usage: %s [+page] [-columns] [-h header] [-w<with>] [-l<length>] [-nt] [files]\n", argv[0]);
  				exit(1);
  		}
  }

  if (!no_header)
  	length -= 10;

  if (length <= 0)
  	length = DEF_LENGTH;

  if (columns) {
  	cwidth = width / columns + 1;
  	if (columns > width) {
  		fprintf(stderr, "Too many columns for pagewidth.\n");
  		exit(1);
  	}
  	if ((colbuf = (char *) sbrk(cwidth * columns * length)) < 0) {
  		fprintf(stderr, "No memory available for a page of %d x %d. Use chmem to allocate more.\n",
			length, cwidth);
  		exit(1);
  	}
  }

  setbuf(stdout, output);

  if (argc == index) {
  	header = "";
  	print(stdin);
  }

  while (index != argc) {
  	if ((file = fopen(argv[index], "r")) == (FILE *) 0) {
  		fprintf(stderr, "Cannot open %s\n", argv[index++]);
  		continue;
  	}
  	header = argv[index];
  	if (columns)
  		format(file);
  	else
  		print(file);
  	fclose(file);
  	index++;
  }

  if (columns) {
  	if (brk(colbuf) < 0) {
  		fprintf(stderr, "Can't reset memory!\n");
  		exit(1);
  	}
  }

  (void) fflush(stdout);
  exit(0);
}

char skip_page(lines, filep)
int lines;
FILE *filep;
{
  short c;

  do {
  	while ((c = getc(filep)) != '\n' && c != EOF);
  	lines--;
  } while (lines && c != EOF);

  return c;
}

format(filep)
FILE *filep;
{
  short c = '\0';
  short index, lines, i;
  short page_number = 0;
  short maxcol = columns;

  do {
/* Check printing of page */
  	page_number++;

  	if (page_number < start_page && c != EOF) {
  		c = skip_page(columns * length, filep);
  		continue;
  	}

  	if (c == EOF)
  		return;

  	lines = columns * length;
  	index = 0;
  	do {
  		for (i = 0; i < cwidth - 1; i++) {
  			if ((c = getc(filep)) == '\n' || c == EOF)
  				break;
  			colbuf[index++] = c;
  		}
							/* First char is EOF */
		if (i == 0 && lines == columns * length && c == EOF)
			return;
		while (c != '\n' && c != EOF)
			c = getc(filep);
  		colbuf[index++] = '\0';
  		while (++i < cwidth)
  			index++;
  		lines--;
  		if (c == EOF) {
  			maxcol = columns - lines / length;
  			while (lines--)
  				for (i = 0; i < cwidth; i++)
  					colbuf[index++] = '\0';
  		}
  	} while (c != EOF && lines);
  	print_page(colbuf, page_number, maxcol);
  } while (c != EOF);
}

print_page(buf, pagenr, maxcol)
char buf[];
short pagenr, maxcol;
{
  short linenr = (pagenr - 1) * length + 1;
  short pad, i, j, start;

  if (!no_header)
  	out_header(pagenr);
  for (i = 0; i < length; i++) {
  	if (number)
  		printf("%d\t", linenr++);
  	for (j = 0; j < maxcol; j++) {
		start = (i + j * length) * cwidth;
		for (pad = 0; pad < cwidth - 1 && buf[start + pad]; pad++)
			putchar (buf[start + pad]);
		if (j < maxcol - 1)		/* Do not pad last column */
			while (pad++ < cwidth - 1)
				putchar (' ');
	}
  	putchar('\n');
  }
  if (!no_header)
  	printf("\n\n\n\n\n");
}

print(filep)
FILE *filep;
{
  short c = '\0';
  short page_number = 0;
  short linenr = 1;
  short lines;

  do {
/* Check printing of page */
  	page_number++;
  	if (page_number < start_page && c != EOF) {
  		c = skip_page(length, filep);
  		continue;
  	}

  	if (c == EOF)
  		return;

	if (page_number == start_page)
		c = getc(filep);

/* Print the page */
  	lines = length;
  	if (!no_header)
  		out_header(page_number);
  	while (lines && c != EOF) {
  		if (number)
  			printf("%d\t", linenr++);
  		do {
  			putchar(c);
  		} while ((c = getc(filep)) != '\n' && c != EOF);
  		putchar('\n');
  		lines--;
		c = getc(filep);
	}
	if (lines == length)
		return;
  	if (!no_header)
  		printf("\n\n\n\n\n");
/* End of file */
  } while (c != EOF);

/* Fill last page */
  if (page_number >= start_page) {
  	while (lines--)
  		putchar('\n');
  }
}

out_header(page)
short page;
{
	extern long time();
	long t;

	(void) time (&t);
	print_time (t);
  	printf("  %s   Page %d\n\n\n", header, page);
}

#define MINUTE	60L
#define HOUR	(60L * MINUTE)
#define DAY	(24L * HOUR)
#define YEAR	(365L * DAY)
#define LYEAR	(366L * DAY)

int mo[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

char *moname[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Print the date.  This only works from 1970 to 2099. */
print_time(t)
long t;
{
  int i, year, day, month, hour, minute;
  long length, time(), original;

  year = 1970;
  original = t;
  while (t > 0) {
	length = (year % 4 == 0 ? LYEAR : YEAR);
	if (t < length)
		break;
	t -= length;
	year++;
  }

 /* Year has now been determined.  Now the rest. */
  day = (int) (t / DAY);
  t -= (long) day * DAY;
  hour = (int) (t / HOUR);
  t -= (long) hour * HOUR;
  minute = (int) (t / MINUTE);

 /* Determine the month and day of the month. */
  mo[1] = (year % 4 == 0 ? 29 : 28);
  month = 0;
  i = 0;
  while (day >= mo[i]) {
	month++;
	day -= mo[i];
	i++;
  }

 /* At this point, 'year', 'month', 'day', 'hour', 'minute'  ok */
  printf("\n\n%s %d %0d:%0d %d", moname[month], day + 1, hour + 1, minute, year);
}

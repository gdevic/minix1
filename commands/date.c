/* date - print or set the date		Author: Adri Koppes */

#include "stdio.h"

int qflag;

int days_per_month[] =
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
char *months[] =
  { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char *days[] =
  { "Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed" };

struct {
	int year, month, day, hour, min, sec;
} tm;

long s_p_min;
long s_p_hour;
long s_p_day;
long s_p_year;

main(argc, argv)
int argc;
char **argv;
{
  long t, time();

  if (argc  > 2) usage();
  s_p_min = 60;
  s_p_hour = 60 * s_p_min;
  s_p_day = 24 * s_p_hour;
  s_p_year = 365 * s_p_day;
  if (argc == 2) {
	if (*argv[1] == '-' && (argv[1][1] | 0x60) == 'q') {
		/* Query option. */
		char time_buf[15];

		qflag = 1;
		freopen(stdin, "/dev/tty0", "r");
		printf("\nPlease enter date (MMDDYYhhmmss).  Then hit RETURN.\n");
		gets(time_buf);
		set_time(time_buf);
		exit(0);
	}
	set_time(argv[1]);
  } else {
	time(&t);
	cv_time(t);
	printf("%s %s %d %02d:%02d:%02d %d\n", days[(t / s_p_day) % 7], months[tm.month], tm.day, tm.hour, tm.min, tm.sec, tm.year); 
  }
  exit(0);
}

cv_time(t)
long t;
{
  tm.year = 0;
  tm.month = 0;
  tm.day = 1;
  tm.hour = 0;
  tm.min = 0;
  tm.sec = 0;
  while (t >= s_p_year) {
	if (((tm.year + 2) % 4) == 0)
		t -= s_p_day;
	tm.year += 1;
	t -= s_p_year;
  }
  if (((tm.year + 2) % 4) == 0)
	days_per_month[1]++;
  tm.year += 1970;
  while ( t >= (days_per_month[tm.month] * s_p_day))
	t -= days_per_month[tm.month++] * s_p_day;
  while (t >= s_p_day) {
	t -= s_p_day;
	tm.day++;
  }
  while (t >= s_p_hour) {
	t -= s_p_hour;
	tm.hour++;
  }
  while (t >= s_p_min) {
	t -= s_p_min;
	tm.min++;
  }
  tm.sec = (int) t;
}

set_time(t)
char *t;
{
  char *tp;
  long ct, time();
  int len;

  time(&ct);
  cv_time(ct);
  tm.year -= 1970;
  tm.month++;
  len = strlen(t);
  if (len != 12 && len != 10 && len != 6 && len != 4) usage();
  tp = t;
  while (*tp)
	if (!isdigit(*tp++))
		bad();
  if (len == 6 || len == 12) 
  	tm.sec = conv(&tp, 59);
  tm.min = conv(&tp, 59);
  tm.hour = conv(&tp, 23);
  if (len == 12 || len == 10) {
  	tm.year = conv(&tp, 99);
  	tm.day = conv(&tp, 31);
  	tm.month = conv(&tp, 12);
  	tm.year -= 70;
  }
  ct = tm.year * s_p_year;
  ct += ((tm.year + 1) / 4) * s_p_day;
  if (((tm.year + 2) % 4) == 0)
	days_per_month[1]++;
  len = 0;
  tm.month--;
  while (len < tm.month)
	ct += days_per_month[len++] * s_p_day;
  ct += --tm.day * s_p_day;
  ct += tm.hour * s_p_hour;
  ct += tm.min * s_p_min;
  ct += tm.sec;
  if (days_per_month[1] > 28)
	days_per_month[1] = 28;
  if (stime(&ct)) {
	fprintf(stderr, "Set date not allowed\n");
  } else {
	cv_time(ct);
  }
}

conv(ptr, max)
char **ptr;
int max;
{
  int buf;

  *ptr -=2;
  buf = atoi(*ptr);
  **ptr = 0;
  if (buf < 0 || buf > max)
	bad();
  return(buf);
}

bad()
{
  fprintf(stderr, "Date: bad conversion\n");
  exit(1);
}

usage()
{
  if (qflag==0) fprintf(stderr, "Usage: date [-q] [[MMDDYY]hhmm[ss]]\n");
  exit(1);
}

isdigit(c)
char c;
{
  if (c >= '0' && c <= '9')
	return(1);
  else
	return(0);
}

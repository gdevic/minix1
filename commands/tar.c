/* tar - tape archiver			Author: Michiel Huisjes */

/* Usage: tar [cxt][v] tapefile [files]
 *
 * Bugs:
 *	This tape archiver should only be used as a program to read or make
 *	simple tape archives. Its basic goal is to read (or build) UNIX V7 tape
 *	archives and extract the named files. It is not wise to use it on
 *	raw magnetic tapes. It doesn't know anything about linked files,
 *	except when the involved fields are filled in.
 */

#include "stat.h"

struct direct {
  unsigned short d_ino;
  char d_name[14];
};

typedef char BOOL;
#define TRUE	1
#define FALSE	0

#define HEADER_SIZE	512
#define NAME_SIZE	100
#define BLOCK_BOUNDARY	 20

typedef union {
  char hdr_block[HEADER_SIZE];
  struct m {
	char m_name[NAME_SIZE];
	char m_mode[8];
	char m_uid[8];
	char m_gid[8];
	char m_size[12];
	char m_time[12];
	char m_checksum[8];
	char m_linked;
	char m_link[NAME_SIZE];
  } member;
} HEADER;

HEADER header;

#define INT_TYPE	(sizeof(header.member.m_uid))
#define LONG_TYPE	(sizeof(header.member.m_size))

#define MKDIR		"/bin/mkdir"

#define NIL_HEADER	((HEADER *) 0)
#define NIL_PTR		((char *) 0)
#define BLOCK_SIZE	512

#define flush()		print(NIL_PTR)

BOOL show_fl, creat_fl, ext_fl;

int tar_fd;
char usage[] = "Usage: tar [cxt] tarfile [files].";
char io_buffer[BLOCK_SIZE];

int total_blocks;
long convert();

#define block_size()	(int) ((convert(header.member.m_size, LONG_TYPE) \
			+ (long) BLOCK_SIZE - 1) / (long) BLOCK_SIZE)

error(s1, s2)
char *s1, *s2;
{
  string_print(NIL_PTR, "%s %s\n", s1, s2 ? s2 : "");
  flush();
  exit(1);
}

main(argc, argv)
int argc;
register char *argv[];
{
  register char *ptr;
  int i;

  if (argc < 3)
	error(usage, NIL_PTR);
  
  for (ptr = argv[1]; *ptr; ptr++) {
	switch (*ptr) {
		case 'c' :
			creat_fl = TRUE;
			break;
		case 'x' :
			ext_fl = TRUE;
			break;
		case 't' :
			show_fl = TRUE;
			break;
		default :
			error(usage, NIL_PTR);
	}
  }

  if (creat_fl + ext_fl + show_fl != 1) 
	error(usage, NIL_PTR);
  
  tar_fd = creat_fl ? creat(argv[2], 0644) : open(argv[2], 0);

  if (tar_fd < 0)
	error("Cannot open ", argv[2]);

  if (creat_fl) {
	for (i = 3; i < argc; i++)
		add_file(argv[i]);
	adjust_boundary();
  }
  else
	tarfile();

  flush();
  exit(0);
}

BOOL get_header()
{
  register int check;

  mread(tar_fd, &header, sizeof(header));
  if (header.member.m_name[0] == '\0')
	return FALSE;

  check = (int) convert(header.member.m_checksum, INT_TYPE);
  
  if (check != checksum())
	error("Tar: header checksum error.", NIL_PTR);

  return TRUE;
}

tarfile()
{
  register char *ptr;
  register char *mem_name;

  while (get_header()) {
	mem_name = header.member.m_name;
	if (ext_fl) {
		if (is_dir(mem_name)) {
			for (ptr = mem_name; *ptr; ptr++)
				;
			*(ptr - 1) = '\0';
			mkdir(mem_name);
		}
		else
			extract(mem_name);
	}
	else  {
		string_print(NIL_PTR, "%s ", mem_name);
		if (header.member.m_linked == '1')
			string_print(NIL_PTR, "linked to %s",
							  header.member.m_link);
		else
			string_print(NIL_PTR, "%d tape blocks", block_size());
		print("\n");
		skip_entry();
	}
	flush();
  }
}

skip_entry()
{
  register int blocks = block_size();

  while (blocks--)
	(void) read(tar_fd, io_buffer, BLOCK_SIZE);
}

extract(file)
register char *file;
{
  register int fd;

  if (header.member.m_linked == '1') {
	if (link(header.member.m_link, file) < 0)
		string_print(NIL_PTR, "Cannot link %s to %s\n",
						    header.member.m_link, file);
	else
		string_print(NIL_PTR, "Linked %s to %s\n",
						    header.member.m_link, file);
	return;
  }

  if ((fd = creat(file, 0644)) < 0) {
	string_print(NIL_PTR, "Cannot create %s\n", file);
	return;
  }

  copy(file, tar_fd, fd, convert(header.member.m_size, LONG_TYPE));
  (void) close(fd);

  chmod(file, convert(header.member.m_mode, INT_TYPE));
  flush();
}

copy(file, from, to, bytes)
char *file;
int from, to;
register long bytes;
{
  register int rest;
  int blocks = (int) ((bytes + (long) BLOCK_SIZE - 1) / (long) BLOCK_SIZE);

  string_print(NIL_PTR, "%s, %d tape blocks\n", file, blocks);

  while (blocks--) {
	(void) read(from, io_buffer, BLOCK_SIZE);
	rest = (bytes > (long) BLOCK_SIZE) ? BLOCK_SIZE : (int) bytes;
	mwrite(to, io_buffer, (to == tar_fd) ? BLOCK_SIZE : rest);
	bytes -= (long) rest;
  }
}

long convert(str, type)
char str[];
int type;
{
  register long ac = 0L;
  register int i;

  for (i = 0; i < type; i++) {
	if (str[i] >= '0' && str[i] <= '7') {
		ac <<= 3;
		ac += (long) (str[i] - '0');
	}
  }

  return ac;
}

mkdir(dir_name)
char *dir_name;
{
  register int pid, w;

  if ((pid = fork()) < 0)
	error("Cannot fork().", NIL_PTR);
  
  if (pid == 0) {
	execl(MKDIR, "mkdir", dir_name, 0);
	error("Cannot find mkdir.", NIL_PTR);
  }

  do {
	w = wait((int *) 0);
  } while (w != -1 && w != pid);
}

checksum()
{
  register char *ptr = header.member.m_checksum;
  register int ac = 0;

  while (ptr < &header.member.m_checksum[INT_TYPE])
	*ptr++ = ' ';

  ptr = header.hdr_block;
  while (ptr < &header.hdr_block[BLOCK_SIZE])
	ac += *ptr++;

  return ac;
}

is_dir(file)
register char *file;
{
  while (*file++ != '\0')
	;

  return (*(file - 2) == '/');
}

char path[NAME_SIZE];

char pathname[NAME_SIZE];
char *path_name(file)
register char *file;
{

  string_print(pathname, "%s%s", path, file);
  return pathname;
}

add_path(name)
register char *name;
{
  register char *path_ptr = path;

  while (*path_ptr)
	path_ptr++;
  
  if (name == NIL_PTR) {
	while (*path_ptr-- != '/')
		;
	while (*path_ptr != '/' && path_ptr != path)
		path_ptr--;
	if (*path_ptr == '/')
		path_ptr++;
	*path_ptr = '\0';
  }
  else {
	while (*name) {
		if (path_ptr == &path[NAME_SIZE])
			error("Pathname too long", NIL_PTR);
		*path_ptr++ = *name++;
	}
	*path_ptr++ = '/';
	*path_ptr = '\0';
  }
}

add_file(file)
register char *file;
{
  struct stat st;
  struct direct dir;
  register int fd;

  if (stat(file, &st) < 0) {
	string_print(NIL_PTR, "Cannot find %s\n", file);
	return;
  }
  if ((fd = open(file, 0)) < 0) {
	string_print(NIL_PTR, "Cannot open %s\n", file);
	return;
  }

  make_header(path_name(file), &st);
  mwrite(tar_fd, &header, sizeof(header));
  if (st.st_mode & S_IFREG)
	copy(path_name(file), fd, tar_fd, st.st_size);
  else if (st.st_mode & S_IFDIR) {
	if (chdir(file) < 0)
		string_print(NIL_PTR, "Cannot chdir to %s\n", file);
	else {
		add_path(file);
		mread(fd, &dir, sizeof(dir));		/* "." */
		mread(fd, &dir, sizeof(dir));		/* ".." */
		while (read(fd, &dir, sizeof(dir)) == sizeof(dir))
			if (dir.d_ino)
				add_file(dir.d_name);
		chdir("..");
		add_path(NIL_PTR);
	}
  }
  else
	print(" Tar: unknown file type. Not added.\n");

  (void) close(fd);
}

make_header(file, st)
char *file;
register struct stat *st;
{
  register char *ptr = header.member.m_name;

  clear_header();

  while (*ptr++ = *file++)
	;

  if (st->st_mode & S_IFDIR) {
	*(ptr - 1) = '/';
	st->st_size = 0L;
  }
  
  string_print(header.member.m_mode, "%I ", st->st_mode & 07777);
  string_print(header.member.m_uid, "%I ", st->st_uid);
  string_print(header.member.m_gid, "%I ", st->st_gid);
  string_print(header.member.m_size, "%L ", st->st_size);
  string_print(header.member.m_time, "%L ", st->st_mtime);
  header.member.m_linked = ' ';
  string_print(header.member.m_checksum, "%I", checksum());
}

clear_header()
{
  register char *ptr = header.hdr_block;

  while (ptr < &header.hdr_block[BLOCK_SIZE])
	*ptr++ = '\0';
}

adjust_boundary()
{
  clear_header();
  mwrite(tar_fd, &header, sizeof(header));

  while (total_blocks++ < BLOCK_BOUNDARY)
	mwrite(tar_fd, &header, sizeof(header));
  (void) close(tar_fd);
}

mread(fd, address, bytes)
int fd, bytes;
char *address;
{
  if (read(fd, address, bytes) != bytes)
	error("Tar: read error.", NIL_PTR);
}

mwrite(fd, address, bytes)
int fd, bytes;
char *address;
{
  if (write(fd, address, bytes) != bytes)
	error("Tar: write error.", NIL_PTR);

  total_blocks++;
}

char output[BLOCK_SIZE];
print(str)
register char *str;
{
  static int index = 0;

  if (str == NIL_PTR) {
	write(1, output, index);
	index = 0;
	return;
  }

  while (*str) {
	output[index++] = *str++;
	if (index == BLOCK_SIZE) {
		write(1, output, BLOCK_SIZE);
		index = 0;
	}
  }
}

char *num_out(number)
register long number;
{
  static char num_buf[13];
  char temp[13];
  register int i;

  for (i = 0; i < 11; i++) {
	temp[i] = (number & 07) + '0';
	number >>= 3;
  }

  for (i = 0; i < 11; i++)
	num_buf[i] = temp[10 - i];
  
  return num_buf;
}

/* VARARGS */
string_print(buffer, fmt, args)
char *buffer;
register char *fmt;
int args;
{
  register char *buf_ptr;
  char *scan_ptr;
  char buf[NAME_SIZE];
  int *argptr = &args;
  BOOL pr_fl, i;

  if (pr_fl = (buffer == NIL_PTR))
	buffer = buf;

  buf_ptr = buffer;
  while (*fmt) {
	if (*fmt == '%') {
		fmt++;
		switch (*fmt++) {
			case 's': 
				scan_ptr = (char *) *argptr;
				break;
			case 'I': 
				scan_ptr = num_out((long) *argptr);
				for (i = 0; i < 5; i++)
					scan_ptr++;
				break;
			case 'L': 
				scan_ptr = num_out(*((long *) argptr));
				argptr++;
				break;
			case 'd' :
				scan_ptr = num_out((long) *argptr);
				while (*scan_ptr == '0')
					scan_ptr++;
				scan_ptr--;
				break;
			default: 
				scan_ptr = "";
		}
		while (*buf_ptr++ = *scan_ptr++)
			;
		buf_ptr--;
		argptr++;
	}
	else
		*buf_ptr++ = *fmt++;
  }
  *buf_ptr = '\0';

  if (pr_fl)
	print(buffer);
}

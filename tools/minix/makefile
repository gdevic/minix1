l=/usr/lib
CFLAGS = -F

all:
	make init
	make bootblok
	make build
	make mkfs
	make fsck

init:	$l/libc.a init.s $l/head.s
	asld -o init  $l/head.s init.s $l/libc.a  $l/end.s
	@echo init done.

# bootblok.s is the source of the MINIX boot block.  The bootblock is the
# first 512 bytes on the image file and on the boot diskette.  When bootblok.s
# is assembled, it generates a short binary file (less than 400 bytes) that
# must be stripped of its header and copied to the file bootblok.  The dd
# command below does this.  If everything has been done right, the bootblok
# file should begin with the following 8 words, in hex:
# c0b8 8e07 33d8 b8f6 2fe0 c08e ff33 00b9
# The exact procedure for going from bootblok.s to the binary file stripped of
# its header is somewhat operating system dependent.  Some assemblers make
# an object (.s) file; others make an a.sut file directly. If your boot 
# diskette does not start out by printing 'Booting MINIX 1.0' the problem is
# probably that you have not made a good boot block.
bootblok:	bootblok.s
	@asld  bootblok.s 
	@dd if=a.out of=bootblok bs=16w skip=1 count=16 2>/dev/null
	@rm a.out
	@echo bootblok done.

build:	build.s
	cc -o build build.s
	@echo build done.

mkfs:	mkfs.s
	cc -o mkfs mkfs.s
	@echo mkfs done.
mkfs.s:	mkfs.c
	cc -c -DUNIX mkfs.c

fsck:	fsck.s fsck1.s
	@echo "Start linking fsck.  /lib/cem will be removed to make space on RAM disk"
	asld -o fsck  fsck1.s fsck.s $l/libc.a $l/end.s
	@echo fsck done. Please restore /lib/cem manually.
fsck.s: fsck.c
	cc -c -Di8088 -w -F -T. fsck.c

# 'make image'  combines the bootblock, kernel, memory manager, file 
# system, init and fsck into a single file, called image.  Each of these pieces
# appears in the image file just as the original does on the disk, except that
# the header at the front is removed, and each piece is padded out to an
# integral multiple of 16 bytes.  Build also prints a message telling how big
# the various pieces (except fsck) are.
image:	build bootblok kernel mm fs init fsck
	/etc/umount /dev/fd0
	@echo "Insert blank disk in drive 0 and hit return"
	@getlf
	build bootblok kernel mm fs init fsck /dev/fd0

# 'make usr' builds a file system on a file called 'usr', and then
# copies this file to the diskette (9 sectors per track).  It builds the file
# system using the program mkfs, which is functionally the same as the standard
# UNIX mkfs program, except that sizes (in blocks) for block special files 
# must be provided.
usr:	mkfs proto.usr
	/etc/umount /dev/fd0
	@echo insert blank disk in drive 0 and hit return
	@getlf
	mkfs -l /dev/fd0 proto.usr >log.usr

user:	mkfs proto.user
	/etc/umount /dev/fd0
	@echo insert blank disk in drive 0 and hit return
	@getlf
	mkfs -l /dev/fd0 proto.user >log.user


# 'make ram' builds the root file system on a file called 'ram'.  This file
# will be copied to the boot diskette when 'image' is made.  During system boot
# it is copied from 128K on the boot diskette to the RAM disk.
ram:	mkfs proto.ram rc 
	/etc/umount /dev/fd0
	@echo "insert blank disk in drive 0 and hit return"
	mkfs -l /dev/fd0 proto.ram >log.ram
	getlf

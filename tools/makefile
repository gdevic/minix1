l=../lib
CFLAGS = -O -DPCIX

all:
	make init
	make bootblok
	make build
	make mkfs
	make fsck

init:	$l/libc.a init.o $l/head.o
	ld -o init  $l/head.o init.o $l/libc.a  $l/end.o
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
# an object (.o) file; others make an a.out file directly. If your boot 
# diskette does not start out by printing 'Booting MINIX 1.0' the problem is
# probably that you have not made a good boot block.
bootblok:	bootblok.s
	as bootblok.s
	@strip a.out
	@dd if=a.out of=bootblok bs=16w skip=1 count=16 2>/dev/null
	@rm a.out
	@echo bootblok done.

build:	build.o
	cc -o build build.o
	@echo build done.

mkfs:	mkfs.o
	cc -o mkfs mkfs.o
	@echo mkfs done.
mkfs.o:	mkfs.c
	cc -c -O -DUNIX mkfs.c

fsck:	fsck.o fsck1.o
	@ld -o fsck  fsck1.o fsck.o $l/libc.a
	@echo fsck done.
fsck1.o:	fsck1.s
	as -o fsck1.o fsck1.s

# 'make image'  combines the bootblock, kernel, memory manager, file 
# system, init and fsck into a single file, called image.  Each of these pieces
# appears in the image file just as the original does on the disk, except that
# the header at the front is removed, and each piece is padded out to an
# integral multiple of 16 bytes.  Build also prints a message telling how big
# the various pieces (except fsck) are.
image:	 build bootblok ../kernel/kernel ../mm/mm ../fs/fs init fsck 
	@build bootblok ../kernel/kernel ../mm/mm ../fs/fs init fsck  image
	cp image /dev/fd0

# 'make usr' builds a file system on a file called 'usr', and then
# copies this file to the diskette (9 sectors per track).  It builds the file
# system using the program mkfs, which is functionally the same as the standard
# UNIX mkfs program, except that sizes (in blocks) for block special files 
# must be provided.
usr:	mkfs proto.usr
	mkfs -l usr proto.usr >log.usr
	cp usr /dev/fd0

user:	mkfs proto.user
	mkfs -l user proto.user >log.user
	cp user /dev/fd0

# 'make ram' builds the root file system on a file called 'ram'.  This file
# will be copied to the boot diskette when 'image' is made.  During system boot
# it is copied from 128K on the boot diskette to the RAM disk.
ram:	mkfs proto.ram rc 
	mkfs -l ram proto.ram >log.ram
	cp ram /dev/fd0

disk5:	mkfs proto.disk5
	mkfs -l disk5 proto.disk5 >log.disk5
	cp disk5 /dev/fd0

disk6:	mkfs proto.disk6
	mkfs -l disk6 proto.disk6 >log.disk6
	cp disk6 /dev/fd0

disk7:	mkfs proto.disk7
	strip ../kernel/kernel ../mm/mm ../fs/fs init fsck
	mkfs -l disk7 proto.disk7 >log.disk7
	cp disk7 /dev/fd0

disk8:	mkfs proto.disk8
	mkfs -l disk8 proto.disk8 >log.disk8
	cp disk8 /dev/fd0

256.ram:	mkfs proto.256.ram rc 
	mkfs -l 256.ram proto.256.ram >log.256.ram
	cp 256.ram /dev/fd0

256.usr:	mkfs proto.256.usr
	mkfs -l 256.usr proto.256.usr >log.256.usr
	cp 256.usr /dev/fd0

at.ram:	mkfs proto.at.ram rc 
	mkfs -l at.ram proto.at.ram >log.at.ram
	cp at.ram /dev/fd0

at.setup:	mkfs proto.at.setup rc 
	mkfs -l at.setup proto.at.setup >log.at.setup
	cp at.setup /dev/fd0


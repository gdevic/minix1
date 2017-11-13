CFLAGS= -Di8088 -w -F -T.
h=../h
l=/usr/lib

obj =	main.s open.s read.s write.s pipe.s device.s \
	path.s mount.s link.s super.s inode.s cache.s filedes.s \
	stadir.s protect.s time.s misc.s utility.s table.s putc.s

fs:	makefile   $l/head.s $(obj) $l/libc.a $l/end.s
	@echo "Start linking FS.  /lib/cem will be removed to make space on RAM disk"
	@rm -f /lib/cem /tmp/*
	asld -o fs $l/head.s $(obj) $l/libc.a $l/end.s
	@echo "FS done.  Please restore /lib/cem manually"

cache.s:	const.h type.h $h/const.h $h/type.h
cache.s:	$h/error.h
cache.s:	buf.h
cache.s:	file.h
cache.s:	fproc.h
cache.s:	glo.h
cache.s:	inode.h
cache.s:	super.h

device.s:	const.h type.h $h/const.h $h/type.h
device.s:	$h/com.h
device.s:	$h/error.h
device.s:	dev.h
device.s:	file.h
device.s:	fproc.h
device.s:	glo.h
device.s:	inode.h
device.s:	param.h

filedes.s:	const.h type.h $h/const.h $h/type.h
filedes.s:	$h/error.h
filedes.s:	file.h
filedes.s:	fproc.h
filedes.s:	glo.h
filedes.s:	inode.h

inode.s:	const.h type.h $h/const.h $h/type.h
inode.s:	$h/error.h
inode.s:	buf.h
inode.s:	file.h
inode.s:	fproc.h
inode.s:	glo.h
inode.s:	inode.h
inode.s:	super.h

link.s:		const.h type.h $h/const.h $h/type.h
link.s:		$h/error.h
link.s:		buf.h
link.s:		file.h
link.s:		fproc.h
link.s:		glo.h
link.s:		inode.h
link.s:		param.h

main.s:		const.h type.h $h/const.h $h/type.h
main.s:		$h/callnr.h
main.s:		$h/com.h
main.s:		$h/error.h
main.s:		buf.h
main.s:		file.h
main.s:		fproc.h
main.s:		glo.h
main.s:		inode.h
main.s:		param.h
main.s:		super.h

misc.s:		const.h type.h $h/const.h $h/type.h
misc.s:		$h/callnr.h
misc.s:		$h/com.h
misc.s:		$h/error.h
misc.s:		buf.h
misc.s:		file.h
misc.s:		fproc.h
misc.s:		glo.h
misc.s:		inode.h
misc.s:		param.h
misc.s:		super.h

mount.s:	const.h type.h $h/const.h $h/type.h
mount.s:	$h/error.h
mount.s:	buf.h
mount.s:	file.h
mount.s:	fproc.h
mount.s:	glo.h
mount.s:	inode.h
mount.s:	param.h
mount.s:	super.h

open.s:		const.h type.h $h/const.h $h/type.h
open.s:		$h/callnr.h
open.s:		$h/error.h
open.s:		buf.h
open.s:		file.h
open.s:		fproc.h
open.s:		glo.h
open.s:		inode.h
open.s:		param.h

path.s:		const.h type.h $h/const.h $h/type.h
path.s:		$h/error.h
path.s:		buf.h
path.s:		file.h
path.s:		fproc.h
path.s:		glo.h
path.s:		inode.h
path.s:		super.h

pipe.s:		const.h type.h $h/const.h $h/type.h
pipe.s:		$h/callnr.h
pipe.s:		$h/com.h
pipe.s:		$h/error.h
pipe.s:		$h/signal.h
pipe.s:		file.h
pipe.s:		fproc.h
pipe.s:		glo.h
pipe.s:		inode.h
pipe.s:		param.h

protect.s:	const.h type.h $h/const.h $h/type.h
protect.s:	$h/error.h
protect.s:	buf.h
protect.s:	file.h
protect.s:	fproc.h
protect.s:	glo.h
protect.s:	inode.h
protect.s:	param.h
protect.s:	super.h

putc.s:		const.h type.h $h/const.h $h/type.h
putc.s:		$h/com.h

read.s:		const.h type.h $h/const.h $h/type.h
read.s:		$h/com.h
read.s:		$h/error.h
read.s:		buf.h
read.s:		file.h
read.s:		fproc.h
read.s:		glo.h
read.s:		inode.h
read.s:		param.h
read.s:		super.h

stadir.s:	const.h type.h $h/const.h $h/type.h
stadir.s:	$h/error.h
stadir.s:	$h/stat.h
stadir.s:	file.h
stadir.s:	fproc.h
stadir.s:	glo.h
stadir.s:	inode.h
stadir.s:	param.h

super.s:	const.h type.h $h/const.h $h/type.h
super.s:	$h/error.h
super.s:	buf.h
super.s:	inode.h
super.s:	super.h

table.s:	const.h type.h $h/const.h $h/type.h
table.s:	$h/com.h
table.s:	$h/callnr.h
table.s:	$h/error.h
table.s:	$h/stat.h
table.s:	buf.h
table.s:	dev.h
table.s:	file.h
table.s:	fproc.h
table.s:	glo.h
table.s:	inode.h
table.s:	super.h

time.s:		const.h type.h $h/const.h $h/type.h
time.s:		$h/callnr.h
time.s:		$h/com.h
time.s:		$h/error.h
time.s:		file.h
time.s:		fproc.h
time.s:		glo.h
time.s:		inode.h
time.s:		param.h

utility.s:	const.h type.h $h/const.h $h/type.h
utility.s:	$h/com.h
utility.s:	$h/error.h
utility.s:	buf.h
utility.s:	file.h
utility.s:	fproc.h
utility.s:	glo.h
utility.s:	inode.h
utility.s:	param.h
utility.s:	super.h

write.s:	const.h type.h $h/const.h $h/type.h
write.s:	$h/error.h
write.s:	buf.h
write.s:	file.h
write.s:	fproc.h
write.s:	glo.h
write.s:	inode.h
write.s:	super.h

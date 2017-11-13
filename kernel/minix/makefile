# The kernel directory contains files xt_wini.c and at_wini.c.  Before running
# make you must copy one of these to wini.c, depending on whether you have a
# PC or an AT.  You must do this even if you do not have a hard disk..
CFLAGS= -Di8088 -w -F -T.
h=../h
l=/usr/lib

obj = mpx88.s main.s proc.s system.s tty.s clock.s memory.s floppy.s wini.s  \
      printer.s table.s klib88.s dmp.s 

kernel:	makefile $(obj) $l/libc.a
	@echo "Start linking Kernel.  /lib/cem will be removed to make space on RAM disk"
	@rm -f /lib/cem /tmp/*
	@asld -o kernel  $(obj) $l/libc.a $l/end.s
	@echo "Kernel done.  Please restore /lib/cem manually"

clock.s:	const.h type.h $h/const.h $h/type.h
clock.s:	$h/callnr.h
clock.s:	$h/com.h
clock.s:	$h/error.h
clock.s:	$h/signal.h
clock.s:	glo.h
clock.s:	proc.h

floppy.s:	const.h type.h $h/const.h $h/type.h
floppy.s:	$h/callnr.h
floppy.s:	$h/com.h
floppy.s:	$h/error.h
floppy.s:	glo.h
floppy.s:	proc.h


dmp.s:		const.h type.h $h/const.h $h/type.h
dmp.s:		$h/callnr.h
dmp.s:		$h/com.h
dmp.s:		$h/error.h
dmp.s:		glo.h
dmp.s:		proc.h

main.s:		const.h type.h $h/const.h $h/type.h
main.s:		$h/callnr.h
main.s:		$h/com.h
main.s:		$h/error.h
main.s:		glo.h
main.s:		proc.h

memory.s:	const.h type.h $h/const.h $h/type.h
memory.s:	$h/callnr.h
memory.s:	$h/com.h
memory.s:	$h/error.h
memory.s:	proc.h

printer.s:	const.h type.h $h/const.h $h/type.h
printer.s:	$h/callnr.h
printer.s:	$h/com.h
printer.s:	$h/error.h

proc.s:		const.h type.h $h/const.h $h/type.h
proc.s:		$h/callnr.h
proc.s:		$h/com.h
proc.s:		$h/error.h
proc.s:		glo.h
proc.s:		proc.h

system.s:	const.h type.h $h/const.h $h/type.h
system.s:	$h/callnr.h
system.s:	$h/com.h
system.s:	$h/error.h
system.s:	$h/signal.h
system.s:	glo.h
system.s:	proc.h

table.s:	const.h type.h $h/const.h $h/type.h
table.s:	glo.h
table.s:	proc.h

tty.s:	const.h type.h $h/const.h $h/type.h
tty.s:	$h/callnr.h
tty.s:	$h/com.h
tty.s:	$h/error.h
tty.s:	$h/sgtty.h
tty.s:	$h/signal.h
tty.s:	glo.h
tty.s:	proc.h

wini.s:	const.h type.h $h/const.h $h/type.h
wini.s:	$h/callnr.h
wini.s:	$h/com.h
wini.s:	$h/error.h
wini.s:	proc.h

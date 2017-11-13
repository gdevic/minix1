# The kernel directory contains files xt_wini.c and at_wini.c.  Before running
# make you must copy one of these to wini.c, depending on whether you have a
# PC or an AT.  You must do this even if you do not have a hard disk..
CFLAGS = -Di8088 -O
h=../h
l=../lib

obj = mpx88.o main.o proc.o system.o tty.o clock.o memory.o floppy.o wini.o  \
      printer.o table.o klib88.o dmp.o 

kernel:	makefile $(obj) $l/libc.a
	@echo "Start linking Kernel"
	@ld -o kernel  $(obj) $l/libc.a $l/end.o
	@echo "Kernel done"

klib88.o:	klib88.s
	as -o klib88.o klib88.s

mpx88.o:	mpx88.s
	as -o mpx88.o mpx88.s

clock.o:	const.h type.h $h/const.h $h/type.h
clock.o:	$h/callnr.h
clock.o:	$h/com.h
clock.o:	$h/error.h
clock.o:	$h/signal.h
clock.o:	glo.h
clock.o:	proc.h

floppy.o:	const.h type.h $h/const.h $h/type.h
floppy.o:	$h/callnr.h
floppy.o:	$h/com.h
floppy.o:	$h/error.h
floppy.o:	glo.h
floppy.o:	proc.h


dmp.o:		const.h type.h $h/const.h $h/type.h
dmp.o:		$h/callnr.h
dmp.o:		$h/com.h
dmp.o:		$h/error.h
dmp.o:		glo.h
dmp.o:		proc.h

main.o:		const.h type.h $h/const.h $h/type.h
main.o:		$h/callnr.h
main.o:		$h/com.h
main.o:		$h/error.h
main.o:		glo.h
main.o:		proc.h

memory.o:	const.h type.h $h/const.h $h/type.h
memory.o:	$h/callnr.h
memory.o:	$h/com.h
memory.o:	$h/error.h
memory.o:	proc.h

printer.o:	const.h type.h $h/const.h $h/type.h
printer.o:	$h/callnr.h
printer.o:	$h/com.h
printer.o:	$h/error.h

proc.o:		const.h type.h $h/const.h $h/type.h
proc.o:		$h/callnr.h
proc.o:		$h/com.h
proc.o:		$h/error.h
proc.o:		glo.h
proc.o:		proc.h

system.o:	const.h type.h $h/const.h $h/type.h
system.o:	$h/callnr.h
system.o:	$h/com.h
system.o:	$h/error.h
system.o:	$h/signal.h
system.o:	glo.h
system.o:	proc.h

table.o:	const.h type.h $h/const.h $h/type.h
table.o:	glo.h
table.o:	proc.h

tty.o:	const.h type.h $h/const.h $h/type.h
tty.o:	$h/callnr.h
tty.o:	$h/com.h
tty.o:	$h/error.h
tty.o:	$h/sgtty.h
tty.o:	$h/signal.h
tty.o:	glo.h
tty.o:	proc.h

wini.o:	const.h type.h $h/const.h $h/type.h
wini.o:	$h/callnr.h
wini.o:	$h/com.h
wini.o:	$h/error.h
wini.o:	proc.h

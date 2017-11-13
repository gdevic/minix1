CFLAGS = -Di8088 -w -F -T.
h=../h
l=/usr/lib

obj =	main.s forkexit.s break.s exec.s signal.s getset.s  \
	alloc.s utility.s table.s putc.s 

mm:	makefile    $l/head.s $(obj) $l/libc.a $l/end.s
	@echo "Start linking MM.  /lib/cem will be removed to make space on RAM disk"
	@rm -f /lib/cem /tmp/*
	@asld -o mm $l/head.s $(obj) $l/libc.a $l/end.s
	@echo "MM done.  Please restore /lib/cem manually"


alloc.s:	const.h $h/const.h $h/type.h

break.s:	const.h $h/const.h $h/type.h
break.s:	$h/error.h
break.s:	$h/signal.h
break.s:	glo.h
break.s:	mproc.h
break.s:	param.h

exec.s:		const.h $h/const.h $h/type.h
exec.s:		$h/callnr.h
exec.s:		$h/error.h
exec.s:		$h/stat.h
exec.s:		glo.h
exec.s:		mproc.h
exec.s:		param.h

forkexit.s:	const.h $h/const.h $h/type.h
forkexit.s:	$h/callnr.h
forkexit.s:	$h/error.h
forkexit.s:	glo.h
forkexit.s:	mproc.h
forkexit.s:	param.h

getset.s:	const.h $h/const.h $h/type.h
getset.s:	$h/callnr.h
getset.s:	$h/error.h
getset.s:	glo.h
getset.s:	mproc.h
getset.s:	param.h

main.s:		const.h $h/const.h $h/type.h
main.s:		$h/callnr.h
main.s:		$h/com.h
main.s:		$h/error.h
main.s:		glo.h
main.s:		mproc.h
main.s:		param.h

putc.s:		$h/const.h $h/type.h
putc.s:		$h/com.h

signal.s:	const.h $h/const.h $h/type.h
signal.s:	$h/callnr.h
signal.s:	$h/com.h
signal.s:	$h/error.h
signal.s:	$h/signal.h
signal.s:	$h/stat.h
signal.s:	glo.h
signal.s:	mproc.h
signal.s:	param.h

table.s:	const.h $h/const.h $h/type.h
table.s:	$h/callnr.h
table.s:	glo.h
table.s:	mproc.h
table.s:	param.h

utility.s:	const.h $h/const.h $h/type.h
utility.s:	$h/callnr.h
utility.s:	$h/com.h
utility.s:	$h/error.h
utility.s:	$h/stat.h
utility.s:	glo.h
utility.s:	mproc.h

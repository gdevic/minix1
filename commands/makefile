# To make  'cp', type:   make f=cp
# To make  'ls', type:   make f=ls
# To make  'cat', type:   make f=cat
# Get the idea?

l=/usr/lib
i=/usr/include
CFLAGS=  -I/usr/include -F

file:	$l/libc.a $f.s
	@cc -o bin/$f $f.s
	@chmem =2048 bin/$f >/dev/null
	@echo "$f done ."

mined:	mined1.s mined2.s $i/mined.h
	@asld -o bin/mined $l/crtso.s mined1.s mined2.s $l/libc.a $l/end.s

shobj = sh1.s sh2.s sh3.s sh4.s sh5.s
sh:	$(shobj) $i/sh.h
	@asld -o bin/sh $l/crtso.s $(shobj) $l/libc.a $l/end.s


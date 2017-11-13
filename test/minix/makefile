# To make  'test0', type:   make f=test0
# To make  'test1', type:   make f=test1
# To make  'test2', type:   make f=test2
# Get the idea?

l=/usr/lib

CFLAGS = -I/usr/include -F

file:	$l/libc.a $f.s
	@asld -o $f $l/crtso.s  $f.s $l/libc.a $l/end.s


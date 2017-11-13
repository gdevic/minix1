# To make  'test0', type:   make f=test0
# To make  'test1', type:   make f=test1
# To make  'test2', type:   make f=test2
# Get the idea?

CFLAGS = -O -I../include
h=../h
l=../lib

file:	$l/libc.a $f.o
	@ld -s -o $f $l/crtso.o  $f.o $l/libc.a $l/end.o


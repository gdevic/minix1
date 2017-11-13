WRITE = 4
FS = 1

|*===========================================================================*
|*				csv & cret				     *
|*===========================================================================*
| This version of csv replaces the standard one.   It checks sp.
.globl csv, cret, _sp_limit, _sendrec, _M
csv:	pop bx			| bx = return address
	push bp			| stack old frame pointer
	mov bp,sp		| set new frame pointer to sp
	push di			| save di
	push si			| save si
	sub sp,ax		| ax = # bytes of local variables
	cmp sp,_sp_limit	| has stack overflowed?
	jb csv.err		| jump if stack overflow
	jmp (bx)		| normal return: copy bx to pc

csv.err:			| come here if stack overflow
	mov _M+2,#WRITE		| m_type
	mov _M+4,#2		| file descriptor 2 is std error
	mov _M+6,#15		| prepare to print error message
	mov _M+10,#stkovmsg	| error message
	mov ax,#_M		| prepare to call sendrec(FS, &M);
	push ax			| push second parameter
	mov ax,#FS		| prepare to push first parameter
	push ax			| push first parameter
	call _sendrec		| write(fd, stkovmsg, 15);
	add sp,#4		| clean up stack
L0:	jmp L0			| hang forever

cret:	lea	sp,*-4(bp)
	pop	si
	pop	di
	pop	bp
	ret

.data
_sp_limit: .word 0		| stack limit default is 0
stkovmsg: .asciz "Stack overflow\n"

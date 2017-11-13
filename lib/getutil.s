.globl _get_base, _get_size, _get_tot_mem
.globl endbss

|*========================================================================*
|                           utilities                                     *
|*========================================================================*
_get_base:			| return click at which prog starts
	mov ax,ds
	ret

_get_size:			| return prog size in bytes (text+data+bss)
	mov ax,#endbss		| end is compiler label at end of bss
	ret

| Find out how much memory the machine has, including vectors, kernel MM, etc.
_get_tot_mem:
	cli
	push es
	push di
	mov ax,#8192		| start search at 128K (8192 clicks)
	sub di,di
L1:	mov es,ax
	seg es
	mov (di),#0xA5A4	| write random bit pattern to memory
	xor bx,bx
	seg es
	mov bx,(di)		| read back pattern just written
	cmp bx,#0xA5A4		| compare with expected value
	jne L2			| if different, no memory present
	add ax,#4096		| advance counter by 64K
	cmp ax,#0xA000		| stop seaching at 640K
	jne L1
L2:	pop di
	pop es
	sti
	ret

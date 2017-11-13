title GetUtil  -  C86 version
page,132


	INCLUDE ..\lib\prologue.h

	PUBLIC get_base, get_size, get_tot_
	EXTRN  brksize:WORD, @end:byte


@CODE	SEGMENT
	ASSUME	CS:@CODE,DS:DGROUP

;========================================================================
;                           utilities                                     
;========================================================================

get_base:			; return click at which prog starts
	mov ax,ds
	ret

get_size:			   ; return prog size in bytes [text+data+bss]
	mov ax,offset dgroup:@end  ; end is label at end of bss
	ret

; Find out how much memory the machine has, including vectors, kernel MM, etc.
get_tot_:
	cli
	push es
	push di
	mov ax,8192		; start search at 128K [8192 clicks]
	sub di,di
y1:	mov es,ax
	mov es:[di],0A5A4h	; write random bit pattern to memory
	xor bx,bx
	mov bx,es:[di]		; read back pattern just written
	cmp bx,0A5A4h		; compare with expected value
	jne y2			; if different, no memory present
	add ax,4096		; advance counter by 64K
	cmp ax,0A000h		; stop seaching at 640K
	jne y1
y2:	pop di
	pop es
	sti
	ret

@CODE	ENDS

	END	; end of assembly-file

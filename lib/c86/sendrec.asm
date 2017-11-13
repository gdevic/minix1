title Send/Receive  -  C86 version
page,132

	INCLUDE ..\lib\prologue.h

	PUBLIC send, receive, send_rec, sendrec


; The following definitions are from  ../h/com.h

	_SEND	= 1
	_RECEIVE= 2
	_BOTH	= 3
	_SYSVEC	= 32


@CODE	SEGMENT
	ASSUME	CS:@CODE,DS:DGROUP

;========================================================================
;                           send and receive                              
;========================================================================
; send(), receive(), send_rec() all save bp, but destroy ax, bx, and cx.

send:	mov cx,_SEND		; send(dest, ptr)
	jmp L0

receive:
	mov cx,_RECEIVE		; receive(src, ptr)
	jmp L0

sendrec:
send_rec:
	mov cx,_BOTH		; send_rec(srcdest, ptr)
	jmp L0

  L0:	push bp			; save bp
	mov bp,sp		; can't index off sp
	mov ax,4[bp]		; ax = dest-src
	mov bx,6[bp]		; bx = message pointer
	int _SYSVEC		; trap to the kernel
	pop bp			; restore bp
	ret			; return

@CODE	ENDS

	END	; end of assembly-file

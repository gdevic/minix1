Title $MAIN    C-86 run-time start-off for Minix
page,132



PUBLIC	@end, brksize, edata, $main, environ, kamikaze
EXTRN   main:NEAR, exit:NEAR

INCLUDE ..\lib\prologue.h

STACKSIZE EQU 2560	; default stack is 5 Kb (2K words)



@CODE	SEGMENT
	ASSUME	CS:@CODE,DS:DGROUP

; This is the C run-time start-off routine.  It's job is to take the
; arguments as put on the stack by EXEC, and to parse them and set them up the
; way main expects them.
;

$main:
	mov	ax,DGROUP	; force relocation of data & bss
	mov	bx,sp		; set stackframe pointer (ds=ss)
	mov	cx,[bx]		; get argc
	add	bx,2		; point at next parameter
	mov	ax,cx
	inc	ax		; calculate envp
	shl	ax,1
	add	ax,bx
	mov	environ,ax	; save envp
	push	ax		; stack envp
	push	bx		; stack argv
	push	cx		; stack argc

	call	main		; call main (arc,argv,envp)

	add	sp,6
	push	ax		; stack program-termination status
	call	exit		; this will never return

	; DEBUG from here on

kamikaze: 	int 3
		ret

@CODE	ENDS


@DATAC	SEGMENT
brksize	DW	offset dgroup:@END+2  	; dynamic changeable end of bss
environ	DW	0			; save environment pointer here
@DATAC	ENDS

@DATAT	SEGMENT				; DATAT holds nothing. The label just
edata	label byte			; tells us where the initialized data
@DATAT	ENDS				; (.data) ends.

@DATAV	SEGMENT				; DATAV holds nothing. The label in
@end	label byte			; the segment just tells us where
@DATAV	ENDS				; the data+bss ends.

@STACK	SEGMENT	BYTE STACK 'STACK'
	DW	STACKSIZE dup(?)	; add stack segment to bss
@STACK	ENDS


	END	$main	; program entry-point (could be anywhere)

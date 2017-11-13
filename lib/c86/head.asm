title Head  -  Start-off for initt, mm and fs (C86-compiler)
page ,132

EXTRN main:NEAR
PUBLIC $main, data_org,	brksize, sp_limit, @end
EXTRN  stackpt:word

INCLUDE	..\lib\prologue.h


@CODE	SEGMENT
	assume	cs:@code,ds:dgroup

$main:	jmp	short L0
	ORG 10h			; kernel uses this area	as stack for inital IRET
     L0:mov	sp,dgroup:stackpt
	call	main
     L1:jmp L1			; this will never be executed
	mov	ax,DGROUP	; force	relocation for dos2out (never executed)

@CODE	ENDS


@DATAB	SEGMENT			; fs needs to know where build stuffed table
data_org DW 0DADAh		; 0xDADA is magic number for build
	 DW 7 dup(0)		; first 8 words of MM, FS, INIT are for stack
brksize	 DW	offset dgroup:@END  ; first free memory
sp_limit DW	0
@DATAB	ENDS

@DATAV	SEGMENT				; DATAV	holds nothing. The label in
	@END	label	byte		; the segment just tells us where
@DATAV	ENDS				; the data+bss ends.

@STACK	SEGMENT	BYTE STACK 'STACK'
@STACK	ENDS				; Add stack to satisfy DOS-linker

	END	$main			; end of assembly & entry-point

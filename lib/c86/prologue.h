
;	prologue.h	
;	standard prologue for c86 assembly code
;	This file defines the correct ordering of segments

@CODE	SEGMENT BYTE PUBLIC 'CODE'	; text segment
@CODE	ENDS
@DATAB	SEGMENT PARA PUBLIC 'DATAB'	; data segment
@DATAB	ENDS
@DATAC	SEGMENT BYTE PUBLIC 'DATAC'	; data segment
@DATAC	ENDS
@DATAI	SEGMENT BYTE PUBLIC 'DATAI'	; data segment
@DATAI	ENDS
@DATAT	SEGMENT BYTE PUBLIC 'DATAT'	; bss segment
@DATAT	ENDS
@DATAU	SEGMENT BYTE PUBLIC 'DATAU'	; bss segment
@DATAU	ENDS
@DATAV	SEGMENT BYTE PUBLIC 'DATAV'	; bss segment
@DATAV	ENDS


DGROUP	GROUP	@DATAB,@DATAC,@DATAI,@DATAT,@DATAU,@DATAV

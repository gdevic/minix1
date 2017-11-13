Title  end  -  end of bss for C86

; Note:	This static symbol is the address of the end of the unitialized data
;	In C86 this symbol is (and must be) declarfed in crtso.asm, which
;	defines the load-model of a program. Its name there is '@END' because
;	'END' is a reserved assembler symbol. C86 knows that these symbols
;	must be prepended by the @-sign (so in a C-program you must just
;	csall it 'end').

END

Title brksize  -  dynamic end of bss

; Note:	as opposed to the symbol 'end' which statically sets the end
;	of the unitialized (allocated) data, brksize can set this
;	dynamically. It is used by the memory-allocator to allocate
;	free memory on top of the bss. Thus stack and heap grow
;	towards eachother.
;
;	This symbol is (must be) declared in crtso.asm which defines
;	the load-model for C86-programs under Minix.

END

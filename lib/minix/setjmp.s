.globl _setjmp, _longjmp
.text
_setjmp:	mov	bx,sp
		mov	ax,(bx)
		mov	bx,*2(bx)
		mov	(bx),bp
		mov	*2(bx),sp
		mov	*4(bx),ax
		xor	ax,ax
		ret

_longjmp:	xor	ax,ax
		push	bp
		mov	bp,sp
		mov	bx,*4(bp)
		mov	ax,*6(bp)
		or	ax,ax
		jne	L1
		inc	ax
L1:		mov	cx,(bx)
L2:		cmp	cx,*0(bp)
		je	L3
		mov	bp,*0(bp)
		or	bp,bp
		jne	L2
		hlt
L3:
		mov	bp,*0(bp)
		mov	sp,*2(bx)
		mov	cx,*4(bx)
		mov	bx,sp
		mov	(bx),cx
		ret



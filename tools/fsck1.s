STACKSIZE = 8192

.globl _main, _exit, _edata, _end, _putc, _getc, _reset_diskette, _diskio
.globl csv, cret, begtext, begdata, begbss
.globl _cylsiz, _tracksiz, _drive

.text
begtext:
start:
	mov	dx,bx		| bootblok puts # sectors/track in bx
	xor	ax,ax
	mov	bx,#_edata	| prepare to clear bss
	mov	cx,#_end
	sub	cx,bx
	sar	cx,*1
st.1:	mov	(bx),ax		| clear bss
	add	bx,#2
	loop	st.1

	mov	_tracksiz,dx	| dx (was bx) is # sectors/track
	add	dx,dx
	mov	_cylsiz,dx	| # sectors/cylinder
	mov	sp,#kerstack+STACKSIZE
	call	_main
	mov	bx,ax		| put scan code for '=' in bx
	cli
	mov	dx,#0x60
	mov	ds,dx
	mov	es,dx
	mov	ss,dx
	jmpi	0,0x60		| jmp to kernel

_exit:	mov	bx,_tracksiz
	jmp	start


_putc:
	xor	ax,ax
	call	csv
	movb	al,4(bp)	| al contains char to be printed
	movb	ah,#14		| 14 = print char
	movb	bl,*1		| foreground color
	push	bp		| not preserved
	int	0x10		| call BIOS VIDEO_IO
	pop	bp
	jmp	cret

_getc:
	xorb	ah,ah
	int	0x16
	ret

_reset_diskette:
	xor	ax,ax
	call	csv
	push	es		| not preserved
	int	0x13		| call BIOS DISKETTE_IO
	pop	es
	jmp	cret


| handle diskio(RW, sector_number, buffer, sector_count) call
| Do not issue a BIOS call that crosses a track boundary
_diskio:
	xor	ax,ax
	call	csv
	mov	tmp1,#0		| tmp1 = # sectors actually transferred
	mov	di,10(bp)	| di = # sectors to transfer
	mov	tmp2,di		| di = # sectors to transfer
d0:	mov	ax,6(bp)	| ax = sector number to start at
	xor	dx,dx		| dx:ax is dividend
	div	_cylsiz		| ax = cylinder, dx = sector within cylinder
	movb	cl,ah		| cl = hi-order bits of cylinder
	rorb	cl,#1		| BIOS expects hi bits in a funny place
	rorb	cl,#1		| ditto
	movb	ch,al		| cx = sector # in BIOS format
	mov	ax,dx		| ax = sector offset within cylinder
	xor	dx,dx		| dx:ax is dividend
	div	_tracksiz	| ax = head, dx = sector
	movb	dh,al		| dh = head
	orb	cl,dl		| cl = 2 high-order cyl bits || sector
	incb	cl		| BIOS counts sectors starting at 1
	movb	dl,_drive	| dl = drive code (0-3 or 0x80 - 0x81)
	mov	bx,8(bp)	| bx = address of buffer
	movb	al,cl		| al = sector #
	addb	al,10(bp)	| compute last sector
	decb	al		| al = last sector to transfer
	cmpb	al,_tracksiz	| see if last sector is on next track
	jle	d1		| jump if last sector is on this track
	mov	10(bp),#1	| transfer 1 sector at a time
d1:	movb	ah,4(bp)	| ah = READING or WRITING
	addb	ah,*2		| BIOS codes are 2 and 3, not 0 and 1
	movb	al,10(bp)	| al = # sectors to transfer
	mov	tmp,ax		| al is # sectors to read/write
	push	es		| BIOS ruins es
	int	0x13		| issue BIOS call
	pop	es		| restore es
	cmpb	ah,*0		| ah != 0 means BIOS detected error
	jne	d2		| exit with error
	mov	ax,tmp		| fetch count of sectors transferred
	xorb	ah,ah		| count is in ax
	add	tmp1,ax		| tmp1 accumulates sectors transferred
	mov	si,tmp1		| are we done yet?
	cmp	si,tmp2		| ditto
	je	d2		| jump if done
	inc	6(bp)		| next time around, start 1 sector higher
	add	8(bp),#0x200	| move up in buffer by 512 bytes
	jmp	d0
d2:	jmp	cret

csv:
	pop	bx
	push	bp
	mov	bp,sp
	push	di
	push	si
	sub	sp,ax
	jmp	(bx)

cret:
	lea	sp,*-4(bp)
	pop	si
	pop	di
	pop	bp
	ret

.data
begdata:
tmp:	.word 0
tmp1:	.word 0
tmp2:	.word 0
.bss
begbss:
kerstack:	.zerow STACKSIZE/2	| kernel stack

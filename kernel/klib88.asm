title klib88
page,132

; This file contains a number of assembly code utility routines	needed by the
; kernel.  They	are:
;
;   phys_copy:	copies data from anywhere to anywhere in memory
;   cp_mess:	copies messages	from source to destination
;   port_out:	outputs	data on	an I/O port
;   port_in:	inputs data from an I/O	port
;   lock:	disable	interrupts
;   unlock:	enable interrupts
;   restore:	restore	interrupts [enable/disabled] as	they were before lock[]
;   build_sig:	build 4	word structure pushed onto stack for signals
;   csv:	procedure prolog to save the registers
;   cret:	procedure epilog to restore the	registers
;   get_chrome:	returns	0 is display is	monochrome, 1 if it is color
;   vid_copy:	copy data to video ram [on color display during	retrace	only]
;   get_byte:	reads a	byte from a user program and returns it	as value

; The following	procedures are defined in this file and	called from outside it.
PUBLIC phys_cop, cp_mess, port_out, port_in, lock, unlock, restore
PUBLIC build_si, csv, cret, get_chro, vid_copy,	get_byte
PUBLIC reboot, wreboot

; The following	external procedure is called in	this file.
EXTRN panic:NEAR

; Variables and	data structures
EXTRN color:WORD, cur_proc:WORD, proc_ptr:WORD,	splimit:WORD
PUBLIC vec_tabl

INCLUDE	..\lib\prologue.h


@CODE	SEGMENT
	assume	cs:@code,ds:dgroup

;===========================================================================
;				phys_copy
;===========================================================================
; This routine copies a	block of physical memory.  It is called	by:
;    phys_copy(	(long) source, (long) destination, (long) bytecount)

phys_cop:
	pushf			; save flags
	cli			; disable interrupts
	push bp			; save the registers
	push ax			; save ax
	push bx			; save bx
	push cx			; save cx
	push dx			; save dx
	push si			; save si
	push di			; save di
	push ds			; save ds
	push es			; save es
	mov bp,sp		; set bp to point to saved es

  L0:	mov ax,28[bp]		; ax = high-order word of 32-bit destination
	mov di,26[bp]		; di = low-order word of 32-bit	destination
	mov cx,4		; start	extracting click number	from dest
  L1:	rcr ax,1		; click	number is destination address /	16
	rcr di,1		; it is	used in	segment	register for copy
	loop L1			; 4 bits of high-order word are	used
	mov es,di		; es = destination click

	mov ax,24[bp]		; ax = high-order word of 32-bit source
	mov si,22[bp]		; si = low-order word of 32-bit	source
	mov cx,4		; start	extracting click number	from source
  L2:	rcr ax,1		; click	number is source address / 16
	rcr si,1		; it is	used in	segment	register for copy
	loop L2			; 4 bits of high-order word are	used
	mov ds,si		; ds = source click

	mov di,26[bp]		; di = low-order word of dest address
	and di,000Fh		; di = offset from paragraph  in es
	mov si,22[bp]		; si = low-order word of source	address
	and si,000Fh		; si = offset from paragraph  in ds

	mov dx,32[bp]		; dx = high-order word of byte count
	mov cx,30[bp]		; cx = low-order word of byte count

	test cx,8000h		; if bytes >= 32768, only do 32768
	jnz L3			; per iteration
	test dx,0FFFFh		; check	high-order 17 bits to see if bytes
	jnz L3			; if bytes >= 32768 then go to L3
	jmp short L4		; if bytes < 32768 then	go to L4
  L3:	mov cx,8000h		; 0x8000 is unsigned 32768
  L4:	mov ax,cx		; save actual count used in ax;	needed later

	test cx,0001h		; should we copy a byte	or a word at a time?
	jz L5			; jump if even
	rep movsb		; copy 1 byte at a time
	jmp short L6		; check	for more bytes

  L5:	shr cx,1		; word copy
	rep movsw		; copy 1 word at a time

  L6:	mov dx,32[bp]		; decr count, incr src & dst, iterate if needed
	mov cx,30[bp]		; dx ||	cx is 32-bit byte count
	xor bx,bx		; bx ||	ax is 32-bit actual count used
	sub cx,ax		; compute bytes	- actual count
	sbb dx,bx		; dx ;;	cx is  bytes not yet processed
	or cx,cx		; see if it is 0
	jnz L7			; if more bytes	then go	to L7
	or dx,dx		; keep testing
	jnz L7			; if loop done,	fall through

	pop es			; restore all the saved	registers
	pop ds			; restore ds
	pop di			; restore di
	pop si			; restore si
	pop dx			; restore dx
	pop cx			; restore cx
	pop bx			; restore bx
	pop ax			; restore ax
	pop bp			; restore bp
	popf			; restore flags
	ret			; return to caller

L7:	mov 32[bp],dx		; store	decremented byte count back in mem
	mov 30[bp],cx		; as a long
	add 26[bp],ax		; increment destination
	adc 28[bp],bx		; carry	from low-order word
	add 22[bp],ax		; increment source
	adc 24[bp],bx		; carry	from low-order word
	jmp L0			; start	next iteration


;===========================================================================
;				cp_mess
;===========================================================================
; This routine is makes	a fast copy of a message from anywhere in the address
; space	to anywhere else.  It also copies the source address provided as a
; parameter to the call	into the first word of the destination message.
; It is	called by:
;    cp_mess[src, src_clicks, src_offset, dst_clicks, dst_offset]
; where	all 5 parameters are shorts [16-bits].
;
; Note that the	message	size, 'Msize' is in WORDS [not bytes] and must be set
; correctly.  Changing the definition of message the type file and not changing
; it here will lead to total disaster.
; This routine destroys	ax.  It	preserves the other registers.

Msize =	12			; size of a message in 16-bit words
cp_mess:
	push bp			; save bp
	push es			; save es
	push ds			; save ds
	mov bp,sp		; index	off bp because machine can't use sp
	pushf			; save flags
	cli			; disable interrupts
	push cx			; save cx
	push si			; save si
	push di			; save di

	mov ax,8[bp]		; ax = process number of sender
	mov di,16[bp]		; di = offset of destination buffer
	mov es,14[bp]		; es = clicks of destination
	mov si,12[bp]		; si = offset of source	message
	mov ds,10[bp]		; ds = clicks of source	message
	mov es:[di],ax		; copy sender's	process	number to dest message
	add si,2		; don't	copy first word
	add di,2		; don't	copy first word
	mov cx,Msize-1		; remember, first word doesn't count
	rep movsw		; iterate cx times to copy the message

	pop di			; restore di
	pop si			; restore si
	pop cx			; restore cs
	popf			; restore flags
	pop ds			; restore ds
	pop es			; restore es
	pop bp			; restore bp
	ret			; that's all folks!


;===========================================================================
;				port_out
;===========================================================================
; port_out(port, value)	writes 'value' on the I/O port 'port'.

port_out:
	push bx			; save bx
	mov bx,sp		; index	off bx
	push ax			; save ax
	push dx			; save dx
	mov dx,4[bx]		; dx = port
	mov ax,6[bx]		; ax = value
	out dx,al		; output 1 byte
	pop dx			; restore dx
	pop ax			; restore ax
	pop bx			; restore bx
	ret			; return to caller


;===========================================================================
;				port_in
;===========================================================================
; port_in(port,	&value)	reads from port	'port' and puts	the result in 'value'.
port_in:
	push bx			; save bx
	mov bx,sp		; index	off bx
	push ax			; save ax
	push dx			; save dx
	mov dx,4[bx]		; dx = port
	in  al,dx		; input	1 byte
	xor ah,ah		; clear	ah
	mov bx,6[bx]		; fetch	address	where byte is to go
	mov [bx],ax		; return byte to caller	in param
	pop dx			; restore dx
	pop ax			; restore ax
	pop bx			; restore bx
	ret			; return to caller


;===========================================================================
;				lock
;===========================================================================
; Disable CPU interrupts.
lock:
	pushf			; save flags on	stack
	cli			; disable interrupts
	pop lockvar		; save flags for possible restoration later
	ret			; return to caller


;===========================================================================
;				unlock
;===========================================================================
; Enable CPU interrupts.
unlock:
	sti			; enable interrupts
	ret			; return to caller


;===========================================================================
;				restore
;===========================================================================
; Restore enable/disable bit to	the value it had before	last lock.
restore:
	push lockvar		; push flags as	they were before previous lock
	popf			; restore flags
	ret			; return to caller


;===========================================================================
;				build_sig
;===========================================================================
; Build	a structure that is pushed onto	the stack for signals.	It contains
; pc, psw, etc., and is	machine	dependent. The format is the same as generated
; by hardware interrupts, except that after the	"interrupt", the signal	number
; is also pushed.  The signal processing routine within	the user space first
; pops the signal number, to see which function	to call.  Then it calls	the
; function.  Finally, when the function	returns	to the low-level signal
; handling routine, control is passed back to where it was prior to the	signal
; by executing a return-from-interrupt instruction, hence the need for using
; the hardware generated interrupt format on the stack.	 The call is:
;     build_sig(sig_stuff, rp, sig)

; Offsets within proc table
PC    =	24
csreg =	18
PSW   =	28

build_si:
	push bp			; save bp
	mov bp,sp		; set bp to sp for accessing params
	push bx			; save bx
	push si			; save si
	mov bx,4[bp]		; bx points to sig_stuff
	mov si,6[bp]		; si points to proc table entry
	mov ax,8[bp]		; ax = signal number
	mov [bx],ax		; put signal number in sig_stuff
	mov ax,PC[si]		; ax = signalled process' PC
	mov 2[bx],ax		; put pc in sig_stuff
	mov ax,csreg[si]	; ax = signalled process' cs
	mov 4[bx],ax		; put cs in sig_stuff
	mov ax,PSW[si]		; ax = signalled process' PSW
	mov 6[bx],ax		; put psw in sig_stuff
	pop si			; restore si
	pop bx			; restore bx
	pop bp			; restore bp
	ret			; return to caller


;===========================================================================
;		csv & cret  (compiler generated	symbols)
;===========================================================================
; This version of csv replaces the standard one.  It checks for	stack overflow
; within the kernel in a simpler way than is usually done. cret	is standard.
csv:
	pop bx			; bx = return address
	push bp			; stack	old frame pointer
	mov bp,sp		; set new frame	pointer	to sp
	push di			; save di
	push si			; save si
	sub sp,ax		; ax =	bytes of local variables
	cmp sp,dgroup:splimit	; has kernel stack grown too large
	jbe csv1		; if sp	is too low, panic
	jmp [bx]		; normal return: copy bx to program counter

csv1:
	mov dgroup:splimit,0		; prevent call to panic	from aborting in csv
	mov bx,dgroup:proc_ptr		; update rp->p_splimit
	mov WORD PTR 50[bx],0		; rp->sp_limit = 0
	push dgroup:cur_proc		; task number
	mov ax,offset dgroup:stkoverrun	; stack	overran	the kernel stack area
	push ax				; push first parameter
	call panic		; call is: panic(stkoverrun, cur_proc)
	jmp csv1		; this should not be necessary


cret:
	lea	sp,-4[bp]	; set sp to point to saved si
	pop	si		; restore saved	si
	pop	di		; restore saved	di
	pop	bp		; restore bp
	ret			; end of procedure

;===========================================================================
;				get_chrome
;===========================================================================
; This routine calls the BIOS to find out if the display is monochrome or
; color.  The drivers are different, as	are the	video ram addresses, so	we
; need to know.
get_chro:
	int 11h			; call the BIOS	to get equipment type
	and al,30h		; isolate color/mono field
	cmp al,30h		; 0x30 is monochrome
	je getchr1		; if monochrome	then go	to getchr1
	mov ax,1		; color	= 1
	ret			; color	return
getchr1:xor ax,ax		; mono = 0
	ret			; monochrome return


;===========================================================================
;				vid_copy
;===========================================================================
; This routine takes a string of [character, attribute]	pairs and writes them
; onto the screen.  For	a color	display, the writing only takes	places during
; the vertical retrace interval, to avoid displaying garbage on	the screen.
; The call is:
;     vid_copy(buffer, videobase, offset, words)
; where
;     'buffer'	  is a pointer to the (character, attribute) pairs
;     'videobase' is 0xB800 for	color and 0xB000 for monochrome	displays
;     'offset'	  tells	where within video ram to copy the data
;     'words'	  tells	how many words to copy
; if buffer is zero, the fill character	(BLANK)	is used

BLANK =	0700h

vid_copy:
	push bp			; we need bp to	access the parameters
	mov bp,sp		; set bp to sp for indexing
	push si			; save the registers
	push di			; save di
	push bx			; save bx
	push cx			; save cx
	push dx			; save dx
	push es			; save es
vid0:	mov si,4[bp]		; si = pointer to data to be copied
	mov di,8[bp]		; di = offset within video ram
	and di,dgroup:vid_mask	; only 4K or 16K counts
	mov cx,10[bp]		; cx = word count for copy loop
	mov dx,03DAh		; prepare to see if color display is retracing

	mov bx,di		; see if copy will run off end of video ram
	add bx,cx		; compute where copy ends
	add bx,cx		; bx = last character copied + 1
	sub bx,dgroup:vid_mask	; bx = # characters beyond end of video ram
	sub bx,1		; note: dec bx doesn't set flags properly
	jle vid.1		; jump if no overrun
	sar bx,1		; bx = # words that don't fit in video ram
	sub cx,bx		; reduce count by overrun
	mov tmp,cx		; save actual count used for later

vid1:	test dgroup:color,1	; skip vertical	retrace	test if	display	is mono
	jz vid4			; if monochrome	then go	to vid.2

;  vid2:in			; with a color display,	you can	only copy to
;	test al,010q		; the video ram	during vertical	retrace, so
;	jnz vid2		; wait for start of retrace period.  Bit 3 of
vid3:   in al,dx		; 0x3DA	is set during retrace.	First wait
	test al,010q		; until	it is off (no retrace),	then wait
	jz vid3			; until	it comes on (start of retrace)

vid4:   pushf			; copying may now start; save the flags
	cli			; interrupts just get in the way: disable them
	mov es,6[bp]		; load es now: int routines may	ruin it

	cmp si,0		; si = 0 means blank the screen
	je vid7			; jmp for blanking
	lock nop		; this is a trick for the IBM PC-simulator only
				; 'lock' indicates a video ram access
	rep movsw		; this is the copy loop

vid5:   popf			; restore flags
	cmp bx,0		; if bx < 0, then no overrun and we are done
	jle vid6		; jump if everything fit
	mov 10[bp],bx		; set up residual count
	mov 8[bp],0		; start copying at base of video ram
	cmp 4[bp],0		; NIL_PTR means store blanks
	je vid0			; go do it
	mov si,tmp		; si = count of words copied
	add si,si		; si = count of bytes copied
	add 4[bp],si		; increment buffer pointer
	jmp vid0		; go copy some more

	pop es			; restore registers
	pop dx			; restore dx
	pop cx			; restore cx
	pop bx			; restore bx
	pop di			; restore di
	pop si			; restore si
	pop bp			; restore bp
	ret			; return to caller

vid7:   mov ax,BLANK		; ax = blanking	character
	rep stosw		; blank	the screen
	jmp vid5		; done

;===========================================================================
;				get_byte
;===========================================================================
; This routine is used to fetch	a byte from anywhere in	memory.
; The call is:
;     c	= get_byte(seg,	off)
; where
;     'seg' is the value to put	in es
;     'off' is the offset from the es value
get_byte:
	push bp			; save bp
	mov bp,sp		; we need to access parameters
	push es			; save es
	mov es,4[bp]		; load es with segment value
	mov bx,6[bp]		; load bx with offset from segment
	mov al,es:[bx]		; al = byte
	xor ah,ah		; ax = byte
	pop es			; restore es
	pop bp			; restore bp
	ret			; return to caller


;===========================================================================
;				reboot & dump
;===========================================================================
; This code reboots the	PC


reboot:
	cli			; disable interrupts
	mov al,20h
	out 20h,al		; re-enable interrupt controller
	call resvec		; restore the vectors in low core
	int 19h			; reboot the PC

wreboot:
	cli			; disable interrupts
	mov al,20h		; re-enable interrupt controller
	out 20h,al
	call resvec		; restore the vectors in low core
	xor ax,ax		; wait for character before continuing
	int 16h			; get char
	int 19h			; reboot the PC

; Restore the interrupt	vectors	in low core.
resvec:
	cld
	mov cx,2*65
	mov si,offset dgroup:vec_tabl
	xor di,di
	mov es,di
    rep	movsw
	ret

@CODE	ENDS



@DATAI	SEGMENT
lockvar	   DW	 0		; place	to store flags for lock()/restore()
tmp        DW    0		; count of bytes already copied
vec_tabl   DW	 130 dup(0)	; storage for interrupt	vectors
stkoverrun DB	 "Kernel stack overrun,	task = ",0
@DATAI	ENDS

	END	; end of assembly



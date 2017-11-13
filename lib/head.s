.globl _main, _stackpt, begtext, begdata, begbss, _data_org, _exit
.text
begtext:
	jmp L0
	.zerow 7		| kernel uses this area as stack for inital IRET
L0:	mov sp,_stackpt
	call _main
L1:	jmp L1			| this will never be executed
_exit:	jmp _exit		| this will never be executed either
.data
begdata:
_data_org:			| fs needs to know where build stuffed table
.word 0xDADA,0,0,0,0,0,0,0	| first 8 words of MM, FS, INIT are for stack
				| 0xDADA is magic number for build
.bss
begbss:

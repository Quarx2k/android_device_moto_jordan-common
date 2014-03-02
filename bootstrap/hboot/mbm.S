/*
 *This file contains ASM routines taken from MBM
 */

.align 4

/*
 * Delay methods
 */

# Delay CORE
delay_core:

	MOV				R2, #0x0
	B					dc_end
#----------------------------
dc_begin:
	MOV				R1, #0x0
	B					dc_check
#----------------------------
dc_loop:
	NOP
	ADD				R1, R1, #0x1

dc_check:
	CMP				R1, #0x80000
	BLT				dc_loop
	ADD				R2, R2, #0x1

dc_end:
	CMP				R2, R0
	BCC				dc_begin
	
# Return
	BX				LR
#----------------------------

# Delay
delay:

	PUSH			{LR}
	LSRS			R0, R0, #0xA
	ADDS			R0, #0x1
	BL				delay_core
	POP				{PC}
#----------------------------

.global delay


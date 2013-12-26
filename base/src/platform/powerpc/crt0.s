/**
 * \brief   Startup code for Genode applications on PowerPC
 * \author  Josef Soentgen
 * \date    2013-12-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
.section ".text"

	.global _start
_start:

	lis    r11, __initial_sp@h
	ori    r11, r11, __initial_sp@l
	stmw   r1, 0(r11)

	lis    r9, _stack_high@h
	addi   r9,r9, _stack_high@l
	clrrwi r1,r9,4                 # Make sure it is aligned on 16 bytes.
	#li     r0,0
	#stwu   r0,-16(r1)
	#mtlr   r9

	b _main

#.initial_sp: .word __initial_sp
#.stack_high: .word _stack_high

	.globl  __dso_handle
__dso_handle: .long 0

	/*--- .bss (non-initialized data) ------------------*/
.section ".bss"
	.align 2
	.p2align 4,,15
	.global _stack_low
_stack_low:
	.space  128*1024
	.global _stack_high
_stack_high:

	/* initial value of the SP register */
	.globl  __initial_sp
__initial_sp: .space 4


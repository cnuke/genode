/**
 * \brief   Startup code for Genode applications on PowerPC
 * \author  Josef Soentgen
 * \date    2014-01-03
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
.section ".text.crt0"

	.globl _start_ldso
_start_ldso:

	lis    r11, __initial_sp@h
	ori    r11, r11, __initial_sp@l
	stmw   r1, 0(r11)

	lis    r9, _stack_high@h
        addi   r9, r9, _stack_high@l
        clrrwi r1, r9, 4

	bl     init_rtld
	b      _main

	.initial_sp:   .word __initial_sp
	.stack_high:   .word _stack_high


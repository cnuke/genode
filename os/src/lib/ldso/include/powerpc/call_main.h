/*
 * \brief  Call main function  (PowerPC specific)
 * \author Josef Soentgen
 * \date   2014-01-03
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _POWERPC__CALL_MAIN_H_
#define _POWERPC__CALL_MAIN_H_

/**
 * Restore SP from initial sp and jump to entry function
 */
void call_main(void (*func)(void))
{
	extern long __initial_sp;

	/* b0rked */
	/* asm volatile ( */
		/* "lis  r11, %0@h      \n" */
		/* "ori  r11, r11, %0@l \n" */
		/* "stmw r1, 0(%0)     \n" */
		/* "b    %1             \n" */
		/* :: "r" (__initial_sp), "r" (func) */
		/* : "memory"); */
}

#endif /* _POWERPC__CALL_MAIN_H_ */

/*
 * \brief  Atomic operations for PowerPC
 * \author Josef Soentgen
 * \date   2013-12-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__POWERPC__CPU__ATOMIC_H_
#define _INCLUDE__POWERPC__CPU__ATOMIC_H_

namespace Genode {

	/**
	 * Atomic compare and exchange
	 *
	 * This function compares the value at dest with cmp_val.
	 * If both values are equal, dest is set to new_val. If
	 * both values are different, the value at dest remains
	 * unchanged.
	 *
	 * \return  1 if the value was successfully changed to new_val,
	 *          0 if cmp_val and the value at dest differ.
	 */
	inline int cmpxchg(volatile int *dest, int cmp_val, int new_val)
	{
		unsigned long equal, val;

		__asm volatile(
			"1: lwarx   %0, 0, %2  \n"
			"   li      %1, 0      \n"
			"   cmpw    0, %0, %3  \n"
			"   bne-    0,2f       \n"
			"   li      %1, 1      \n"
			"   stwcx.  %4, 0, %2  \n"
			"   bne-    1b         \n"
			"2:                    \n"
			: "=&r" (val), "=&r" (equal)
			: "r" (dest), "r" (cmp_val), "r" (new_val)
			: "cc", "memory");

		return equal;
	}
}

#endif /* _INCLUDE__POWERPC__CPU__ATOMIC_H_ */

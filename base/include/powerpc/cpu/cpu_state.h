/*
 * \brief  CPU state
 * \author Josef Soentgen
 * \date   2013-12-26
 *
 * This file contains the PowerPC-specific part of the CPU state.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__POWERPC__CPU__CPU_STATE_H_
#define _INCLUDE__POWERPC__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
	{
		addr_t r0; /* volatile */
		addr_t r1; /* stack frame pointer */
		addr_t r2; /* system reserved */
		addr_t r3, r4; /* parameter passing and return value */
		addr_t r5, r6, r7, r8, r9, r10; /* volatile parameter passing */

		addr_t r11, r12; /* volatile */
		addr_t r13; /* small data area pointer register */

		/* register used for local variables */
		addr_t r14, r15, r16, r17, r18, r19, r20, r21,
		       r22, r23, r24, r25, r26, r27, r28, r29, r30;

		/* register used for local variable or environment pointer */
		addr_t r31;

		/* register used for conditiom fields (4Bit each) */
		addr_t cr0, cr1, cr2, cr3, cr4, cr5, cr6, cr7;
		addr_t lr;
		addr_t ctr;

		addr_t xer;
		addr_t fpscr;

		Cpu_state() : r0(0), r1(0), r2(0), r3(0), r4(0), r5(0),
		              r6(0), r7(0), r8(0), r9(0), r10(0), r11(0),
		              r12(0), r13(0), r14(0), r15(0), r16(0), r17(0),
		              r18(0), r19(0), r20(0), r21(0), r22(0), r23(0),
		              r24(0), r25(0), r26(0), r27(0), r28(0), r29(0),
		              r30(0), r31(0), cr0(0), cr1(0), cr2(0), cr3(0),
		              cr4(0), cr5(0), cr6(0), cr7(0), lr(0), ctr(0),
		              xer(0), fpscr(0) { }
	};
}

#endif /* _INCLUDE__POWERPC__CPU__CPU_STATE_H_ */

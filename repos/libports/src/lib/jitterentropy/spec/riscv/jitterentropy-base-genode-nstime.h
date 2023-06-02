/*
 * \brief  Time backend for jitterentropy library
 * \author Josef Soentgen
 * \date   2023-06-01
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JITTERENTROPY_RISCV_BASE_GENODE_NSTIME_H_
#define _JITTERENTROPY_RISCV_BASE_GENODE_NSTIME_H_

static inline void jent_get_nstime(__u64 *out)
{
	uint64_t t;
	asm volatile ("rdtime %0" : "=r" (t));
	*out = t;
}

#endif /* _JITTERENTROPY_RISCV_BASE_GENODE_NSTIME_H */

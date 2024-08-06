/*
 * \brief  Replaces mm/percpu.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/percpu.h>
#include <linux/slab.h>
#include <lx_emul/alloc.h>

void __percpu * __alloc_percpu(size_t size, size_t align)
{
	size_t const old_align = align;

	align = max(align, (size_t)KMALLOC_MIN_SIZE);
	printk("%s:%d size: %zu align: %zu (%zu)\n", __func__, __LINE__, size, align, old_align);
	return lx_emul_mem_alloc_aligned(size, align);
}


void __percpu * __alloc_percpu_gfp(size_t size,size_t align,gfp_t gfp)
{
	return __alloc_percpu(size, align);
}


void free_percpu(void __percpu * ptr)
{
	printk("%s:%d ptr: %px\n", __func__, __LINE__, ptr);
	lx_emul_mem_free(ptr);
}

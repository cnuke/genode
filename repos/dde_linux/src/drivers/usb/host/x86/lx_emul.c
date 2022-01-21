/*
 * \brief  Linux emulation environment specific to this driver
 * \author Stefan Kalkowski
 * \date   2021-08-31
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <linux/slab.h>


#include <asm-generic/delay.h>

void __const_udelay(unsigned long xloops)
{
	lx_emul_time_udelay(xloops / 0x10C7UL);
}


#include <linux/cpumask.h>

atomic_t __num_online_cpus = ATOMIC_INIT(1);


#include <linux/dma-mapping.h>

dma_addr_t dma_map_page_attrs(struct device * dev,
                              struct page * page,
                              size_t offset,
                              size_t size,
                              enum dma_data_direction dir,
                              unsigned long attrs)
{
	dma_addr_t    const dma_addr  = page_to_phys(page);
	unsigned long const virt_addr = (unsigned long)page_to_virt(page);

	lx_emul_mem_cache_clean_invalidate((void *)(virt_addr + offset), size);
	return dma_addr + offset;
}


#include <linux/dmapool.h>

struct dma_pool { size_t size; };

void * dma_pool_alloc(struct dma_pool * pool, gfp_t mem_flags, dma_addr_t * handle)
{
	void * ret =
		lx_emul_mem_alloc_aligned_uncached(pool->size, PAGE_SIZE);
	*handle = lx_emul_mem_dma_addr(ret);
	return ret;
}


struct dma_pool * dma_pool_create(const char * name,
                                  struct device * dev,
                                  size_t size,
                                  size_t align,
                                  size_t boundary)
{
	struct dma_pool * pool = kmalloc(sizeof(struct dma_pool), GFP_KERNEL);
	pool->size = size;
	return pool;
}


void dma_pool_free(struct dma_pool * pool,void * vaddr,dma_addr_t dma)
{
	lx_emul_mem_free(vaddr);
}


#include <linux/dma-mapping.h>

int dma_supported(struct device * dev,u64 mask)
{
	lx_emul_trace(__func__);
	return 1;
}


#include <linux/dma-mapping.h>

void dma_unmap_page_attrs(struct device * dev,
                          dma_addr_t addr,
                          size_t size,
                          enum dma_data_direction dir,
                          unsigned long attrs)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	if (dir == DMA_FROM_DEVICE)
		lx_emul_mem_cache_invalidate((void *)virt_addr, size);
}


#include <linux/slab.h>

void * kmalloc_order(size_t size, gfp_t flags, unsigned int order)
{
	return kmalloc(size, flags);
}


#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/slab.h>

int simple_pin_fs(struct file_system_type * type, struct vfsmount ** mount, int * count)
{
	*mount = kmalloc(sizeof(struct vfsmount), GFP_KERNEL);
	return 0;
}


#include <linux/fs.h>

void simple_release_fs(struct vfsmount ** mount,int * count)
{
	kfree(*mount);
}


#include <linux/fs.h>

struct inode * alloc_anon_inode(struct super_block * s)
{
	return kmalloc(sizeof(struct inode), GFP_KERNEL);
}


#include <linux/interrupt.h>

// softirq.c
void tasklet_setup(struct tasklet_struct * t,
                   void (* callback)(struct tasklet_struct *))
{
	// XXX lx_backtrace();

	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->callback = callback;
	t->use_callback = true;
	t->data = 0;
}


void __tasklet_schedule(struct tasklet_struct * t)
{
	// XXX lx_backtrace();

	if (test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
		t->callback(t);
}


#include <linux/rcupdate.h>

void call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	lx_emul_trace(__func__);
	func(head);
}

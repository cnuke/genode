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


#include <asm/x86_init.h>

static int x86_init_pci_init(void)
{
	printk("%s:%d TODO\n", __func__, __LINE__);
	return 1;
}


static void x86_init_pci_init_irq(void)
{
	printk("%s:%d TODO\n", __func__, __LINE__);
}


struct x86_init_ops x86_init = {
	.pci = {
		.init     = x86_init_pci_init,
		.init_irq = x86_init_pci_init_irq,
	},
};


#include <lx_emul/pci_config_space.h>

#include <linux/pci.h>
#include <asm/pci.h>
#include <asm/pci_x86.h>

static int pci_raw_ops_read(unsigned int domain, unsigned int bus, unsigned int devfn,
                            int reg, int len, u32 *val)
{
	// printk("%s:%d %x:%x %d %d\n", __func__, __LINE__, bus, devfn, reg, len);
	return lx_emul_pci_read_config(bus, devfn, (unsigned)reg, (unsigned)len, val);
}


static int pci_raw_ops_write(unsigned int domain, unsigned int bus, unsigned int devfn,
                             int reg, int len, u32 val)
{
	// printk("%s:%d %x:%x %d %d %u\n", __func__, __LINE__, bus, devfn, reg, len, val);
	return lx_emul_pci_write_config(bus, devfn, (unsigned)reg, (unsigned)len, val);
}


const struct pci_raw_ops genode_raw_pci_ops = {
	.read  = pci_raw_ops_read,
	.write = pci_raw_ops_write,
};

const struct pci_raw_ops *raw_pci_ops = &genode_raw_pci_ops;


static int pci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
    return pci_raw_ops_read(0, bus->number, devfn, where, size, value);
}


static int pci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 value)
{
    return pci_raw_ops_write(0, bus->number, devfn, where, size, value);
}


struct pci_ops pci_root_ops = {
    .read  = pci_read,
    .write = pci_write,
};


void pcibios_scan_root(int busnum)
{
	struct pci_bus *bus;
	struct pci_sysdata *sd;
	LIST_HEAD(resources);

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd) {
		printk(KERN_ERR "PCI: OOM, skipping PCI bus %02x\n", busnum);
		return;
	}
	sd->node = NUMA_NO_NODE;
	pci_add_resource(&resources, &ioport_resource);
	pci_add_resource(&resources, &iomem_resource);

	printk(KERN_DEBUG "PCI: Probing PCI hardware (bus %02x)\n", busnum);
	bus = pci_scan_root_bus(NULL, busnum, &pci_root_ops, sd, &resources);

	printk("%s:%d bus: %px\n", __func__, __LINE__, bus);
	if (!bus) {
		pci_free_resource_list(&resources);
		kfree(sd);
		return;
	}
	pci_bus_add_devices(bus);
	printk("%s:%d bus: %px\n", __func__, __LINE__, bus);
}


#include <linux/pci.h>

void pci_assign_irq(struct pci_dev * dev)
{
	printk("%s: dev: %p TODO\n", __func__, dev);
	lx_emul_trace(__func__);
}

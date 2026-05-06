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
#include <linux/pci.h>
#include <linux/list.h>

void __iomem * pci_ioremap_bar(struct pci_dev * pdev, int bar)
{
	struct resource *res = &pdev->resource[bar];
	return ioremap(res->start, resource_size(res));
}


enum {
	UHCI_USBLEGSUP          = 0xc0,
	UHCI_USBRES_INTEL       = 0xc4,
	EHCI_SERIAL_BUS_RELEASE = 0x60,
	EHCI_PORT_WAKE          = 0x62,
};


int pci_read_config_byte(const struct pci_dev * dev,int where,u8 * val)
{
	switch (where) {
	case EHCI_SERIAL_BUS_RELEASE:
		*val = 0x20;
		return 0;
	};
	lx_emul_trace_and_stop(__func__);
}


int pci_read_config_word(const struct pci_dev * dev,int where,u16 * val)
{
	switch (where) {
	case PCI_COMMAND:
		*val = 0x7;
		return 0;
	case EHCI_PORT_WAKE:
		*val = 0;
		return 0;
	case UHCI_USBLEGSUP:
		/* force the driver to do a full_reset */
		*val = 0xffff;
		return 0;
	};
	lx_emul_trace_and_stop(__func__);
}


int pci_read_config_dword(const struct pci_dev * dev,int where,u32 * val)
{
	*val = 0;
	return 0;
}


int pci_write_config_byte(const struct pci_dev * dev,int where,u8 val)
{
	switch (where) {
	case UHCI_USBRES_INTEL:
		/* do nothing */
		return 0;
	}
	lx_emul_trace_and_stop(__func__);
}


int pci_write_config_word(const struct pci_dev * dev,int where,u16 val)
{
	switch (where) {
	case UHCI_USBLEGSUP:
		/* do nothing */
		return 0;
	}
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

unsigned long _copy_from_user(void * to,const void __user * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#include <linux/uaccess.h>

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
                                   unsigned int max_vecs, unsigned int flags,
                                   struct irq_affinity *aff_desc)
{
	if ((flags & PCI_IRQ_INTX) && min_vecs == 1 && dev->irq)
		return 1;
	return -ENOSPC;
}


int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (WARN_ON_ONCE(nr > 0))
		return -EINVAL;
	return dev->irq;
}


void pci_free_irq_vectors(struct pci_dev *dev) { }


struct pci_dev_msix_table_entry
{
	struct list_head node;

	struct pci_dev    *dev;
	struct msix_entry *table;
};


static LIST_HEAD(pci_dev_msix_table_entry_list);


int pci_alloc_irq_vectors(struct pci_dev * dev, unsigned int min_vecs,
                          unsigned int max_vecs,unsigned int flags)
{
	int nvecs = -ENOSPC;

	if (flags & PCI_IRQ_MSIX) {

		struct pci_dev_msix_table_entry *entry = NULL;
		/* lookup or create table for pci_dev */
		{
			struct pci_dev_msix_table_entry *pos;

			list_for_each_entry(pos, &pci_dev_msix_table_entry_list, node) {
				if (pos->dev == dev)
					entry = pos;
			}

			if (!entry) {
				entry = kzalloc(sizeof(*entry), GFP_KERNEL);
				if (!entry)
					return -ENOMEM;

				entry->dev = dev;
				list_add(&entry->node, &pci_dev_msix_table_entry_list);
			}
		}

		/* for the moment just wipe any existing entries */
		kfree(entry->table);
		entry->table = kmalloc_array(max_vecs, sizeof(struct msix_entry),
		                             GFP_KERNEL);
		if (!entry->table)
			return -ENOMEM;

		nvecs = pci_enable_msix_range(dev, entry->table, min_vecs, max_vecs);
	}
	if (nvecs > 0)
		return nvecs;

	if (flags & PCI_IRQ_MSI)
		nvecs = pci_enable_msi(dev);
	/* 0 is good and 1 vec supported */
	if (nvecs == 0)
		return 1;

	if ((flags & PCI_IRQ_INTX) && min_vecs == 1 && dev->irq)
		return 1;

	return -ENOSPC;
}

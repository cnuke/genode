/*
 * \brief  Replaces drivers/pci/pci.c
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/pci.h>
#include <lx_emul/pci.h>

int pci_enable_device(struct pci_dev * dev)
{
	lx_emul_pci_enable(dev_name(&dev->dev));
	return 0;
}


int pcim_enable_device(struct pci_dev *pdev)
{
	/* for now ignore devres */
	return pci_enable_device(pdev);
}


void pci_set_master(struct pci_dev * dev) { }


int pci_set_mwi(struct pci_dev * dev)
{
	return 1;
}


int pci_try_set_mwi(struct pci_dev *dev)
{
	return pci_set_mwi(dev);
}


bool pci_dev_run_wake(struct pci_dev * dev)
{
	return false;
}


u8 pci_find_capability(struct pci_dev * dev,int cap)
{
	return 0;
}


void pci_release_regions(struct pci_dev *pdev) { }


int pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	return 0;
}


#include <linux/irq.h>

extern struct irq_chip dde_msi_irqchip_data_chip;


int pci_enable_msi(struct pci_dev *dev)
{
	unsigned const num_vec = lx_emul_pci_msi_num_vec(pci_name(dev), false);
	if (num_vec != 1)
		return -ENOSYS;

	unsigned const handle = lx_emul_pci_msi_alloc(pci_name(dev), false);
	if (!handle)
		return -ENOSYS;

	int const irq = devm_irq_alloc_descs(&dev->dev, handle, 0, 1, 0);
	printk("%s:%d: irq: %d\n", __func__, __LINE__, irq);
	struct irq_data *irq_data = irq_get_irq_data(irq);
	if (!irq_data)
		return -ENOSYS;

	irq_data->hwirq = handle;
	irq_set_chip_and_handler(handle, &dde_msi_irqchip_data_chip,
	                         handle_edge_irq);

	/* override the GSI with the MSI */
	dev->irq = handle;

	return 0;
}


int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
                          int minvec, int maxvec)
{
	/*
	 * Happens with devices <= IWL_DEVICE_FAMILY_9000 that
	 * apparently only have 1 RX queue (see iwlwifi/pcie/gen1_2/trans.c).
	 */
	if (maxvec < minvec)
		return -EINVAL;

	unsigned const num_vec = lx_emul_pci_msi_num_vec(pci_name(dev), true);
	if (num_vec < minvec)
		return -ENOSPC;

	if (maxvec < 1)
		return -ENOSYS;

	unsigned const vec = min(num_vec, (unsigned)maxvec);

	for (unsigned i = 0; i < vec; i++) {

		unsigned const handle = lx_emul_pci_msi_alloc(pci_name(dev), true);
		if (!handle)
			return -ENOSYS;

		int const irq = devm_irq_alloc_descs(&dev->dev, handle, 0, 1, 0);
		struct irq_data *irq_data = irq_get_irq_data(irq);
		irq_data->hwirq = handle;

		irq_set_chip_and_handler(handle, &dde_msi_irqchip_data_chip,
		                         handle_edge_irq);

		entries[i].vector = handle;
	}

	return vec;
}

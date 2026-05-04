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

extern struct irq_chip dde_irqchip_data_chip;


int pci_enable_msi(struct pci_dev *dev)
{
	unsigned const num_vec = lx_emul_pci_msi_num_vec(dev_name(&dev->dev), false);
	if (num_vec != 1)
		return -ENOSYS;

	unsigned const base_number = lx_emul_pci_msi_base_number(dev_name(&dev->dev));
	if (!base_number)
		return -ENOSYS;

	struct irq_data *irq_data = irq_get_irq_data(base_number);
	irq_data->hwirq = base_number;
	irq_set_chip_and_handler(base_number, &dde_irqchip_data_chip,
	                         handle_edge_irq);

	/* override the GSI with the MSI */
	dev->irq = base_number;

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

	unsigned const num_vec = lx_emul_pci_msi_num_vec(dev_name(&dev->dev), true);
	if (num_vec < minvec)
		return -ENOSPC;

	if (maxvec < 1)
		return -ENOSYS;

	unsigned const base_number = lx_emul_pci_msi_base_number(dev_name(&dev->dev));
	if (!base_number)
		return -ENOSYS;

	unsigned const vec = min(num_vec, (unsigned)maxvec);

	for (unsigned i = 0; i < vec; i++) {
		entries[i].vector = base_number + i;

		struct irq_data *irq_data = irq_get_irq_data(entries[i].vector);
		irq_data->hwirq = entries[i].vector;
		irq_set_chip_and_handler(entries[i].vector, &dde_irqchip_data_chip,
		                         handle_edge_irq);
	}

	return vec;
}

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
	unsigned const num_vec = lx_emul_pci_msi_num_vec(pci_name(dev), false);
	if (num_vec != 1)
		return -ENOSYS;

	unsigned const msi = lx_emul_pci_msi_alloc(pci_name(dev), false);
	if (!msi)
		return -ENOSYS;

	int const irq = devm_irq_alloc_descs(&dev->dev, msi, 0, 1, 0);
	if (irq != msi) {
		devm_free_irq(&dev->dev, msi, NULL);
		lx_emul_pci_msi_free(pci_name(dev), msi);
		return -ENOSYS;
	}

	struct irq_data *irq_data = irq_get_irq_data(msi);
	irq_data->hwirq = msi;
	irq_set_chip_and_handler(msi, &dde_irqchip_data_chip,
	                         handle_edge_irq);

	/*
	 * Override the GSI with the MSI and for the time being we do
	 * not set it back as switching to GSI again is not anticipated.
	 */
	dev->irq = msi;

	return 0;
}


#include <linux/interrupt.h>

void pci_disable_msi(struct pci_dev *dev)
{
	int const msi = dev->irq;
	irq_free_desc(msi);
	lx_emul_pci_msi_free(pci_name(dev), msi);
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

		/*
		 * Returning here will leak resources but that in this case the
		 * driver might be already non-functional and a restart is
		 * required.
		 */
		unsigned const msix = lx_emul_pci_msi_alloc(pci_name(dev), true);
		if (!msix)
			return -ENOSYS;

		int const irq = devm_irq_alloc_descs(&dev->dev, msix, 0, 1, 0);
		if (irq != msix)
			return -ENOSYS;

		entries[i].vector = msix;

		struct irq_data *irq_data = irq_get_irq_data(entries[i].vector);
		irq_data->hwirq = entries[i].vector;
		irq_set_chip_and_handler(entries[i].vector, &dde_irqchip_data_chip,
		                         handle_edge_irq);
	}

	return vec;
}

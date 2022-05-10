/*
 * \brief  PCI backend
 * \author Josef Soentgen
 * \date   2022-05-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>
#include <lx_emul/pci_config_space.h>

#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/interrupt.h>


extern void lx_backtrace(void);


int arch_probe_nr_irqs(void)
{
	/* needed for 'irq_get_irq_data()' in 'pci_assign_irq()' below */
    return 16;
}


/* needed by intel_fb */
unsigned long pci_mem_start = 0xaeedbabe;


static struct attribute *pci_bus_attrs[] = {
    NULL,
};


static const struct attribute_group pci_bus_group = {
    .attrs = pci_bus_attrs,
};


const struct attribute_group *pci_bus_groups[] = {
    &pci_bus_group,
    NULL,
};


static struct attribute *pci_dev_attrs[] = {
    NULL,
};


static const struct attribute_group pci_dev_group = {
    .attrs = pci_dev_attrs,
};


const struct attribute_group *pci_dev_groups[] = {
    &pci_dev_group,
    NULL,
};


static const struct attribute_group *pci_dev_attr_groups[] = {
    NULL,
};


const struct device_type pci_dev_type = {
    .groups = pci_dev_attr_groups,
};


extern const struct device_type pci_dev_type;


static struct pci_bus *_pci_bus;


extern struct device * pci_get_host_bridge_device(struct pci_dev * dev);
struct device * pci_get_host_bridge_device(struct pci_dev * dev)
{
	static struct device inst;
	memset(&inst, 0, sizeof (struct device));
	return &inst;
}


static struct pci_dev *_pci_alloc_dev(struct pci_bus *bus)
{
    struct pci_dev *dev;

    dev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
    if (!dev)
        return NULL;

    INIT_LIST_HEAD(&dev->bus_list);
    dev->dev.type = &pci_dev_type;
    dev->bus = bus;

    return dev;
}


static int _pci_read_config(int devfn, int where, unsigned size, unsigned *value)
{
	/*
	 * See https://edc.intel.com/content/www/us/en/design/products-and-solutions/processors-and-chipsets/comet-lake-u/intel-400-series-chipset-on-package-platform-controller-hub-register-database/xhci-configuration-registers/
	 * for an overview of the xHCI specific registers in the config-space.
	 */
	switch (where) {
	case 0x00: /* vendor/device id */
	case 0x04: /* cmd */
	case 0x08: /* class */
	case 0x2c: /* wifi subsystem vendor/device id */
	case 0x3c: /* irq line */
	case 0x44: /* intel_fb mchbar i915 */
	case 0x48: /* intel_fb mchbar i965 */
	case 0x48 + 4: /* intel_fb mchbar i965 */
	case 0x50: /* intel_fb mirror gmch */
	case 0xc0: /* uhci PCI legacy support register 'USBLEGSUP' */
	case 0xfc: /* intel_fb ASL storage */
		return lx_emul_pci_read_config(0, devfn, where, size, value);
	case 0x60: /* serial bus release number, xhci 31h, ehci 20h */
	case 0x62: /* ehci portwake */
	case 0x63: /* ehci portwake */
		*value = 0;
		return 0;
	default:
		printk("%s:%d: where: 0x%x (%u) not allowed\n", __func__, __LINE__, where, size);
		lx_backtrace();
		return -1;
	}

	return -1;
}


static int _pci_write_config(int devfn, int where, unsigned size, unsigned value)
{
	switch (where) {
	case 0x04:
	case 0xc0: /* uhci PCI legacy support register 'USBLEGSUP' */
	case 0xc4: /* uhci PCI Intel-specific resume-enable register */
		return lx_emul_pci_write_config(0, devfn, where, size, value);
		return 0;
	case 0x41:
		/*
		 * wifi: "We disable the RETRY_TIMEOUT register (0x41) to keep
		 *       PCI Tx retries from interfering with C3 CPU state"
		 */
		return 0;
	default:
		printk("%s:%d: where: 0x%x (%u) not allowed\n", __func__, __LINE__, where, size);
		lx_backtrace();
		return -1;
	}

	return -1;
}


int pci_read_config_byte(const struct pci_dev * dev, int where, u8 *val)
{
	unsigned value = 0;
	int const err = _pci_read_config(dev->devfn, where, 1, &value);
	if (err)
		return err;

	*val = (u8)value;
	return 0;
}


int pci_read_config_word(const struct pci_dev * dev, int where, u16 *val)
{
	unsigned value = 0;
	int const err = _pci_read_config(dev->devfn, where, 2, &value);
	if (err)
		return err;

	*val = (u16)value;
	return 0;
}


int pci_read_config_dword(const struct pci_dev * dev, int where, u32 *val)
{
	unsigned value = 0;
	int const err = _pci_read_config(dev->devfn, where, 4, &value);
	if (err)
		return err;

	*val = (u32)value;
	return 0;
}


int pci_write_config_byte(const struct pci_dev * dev, int where, u8 val)
{
	int const err = _pci_write_config(dev->devfn, where, 1, val);
	if (err)
		return err;

	return 0;
}


int pci_write_config_word(const struct pci_dev * dev, int where, u16 val)
{
	int const err = _pci_write_config(dev->devfn, where, 2, val);
	if (err)
		return err;

	return 0;
}


int pci_write_config_dword(const struct pci_dev * dev, int where, u32 val)
{
	int const err = _pci_write_config(dev->devfn, where, 4, val);
	if (err)
		return err;

	return 0;
}


int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
                             int where, u8 *val)
{
	lx_emul_trace_and_stop(__func__);
	return -1;
}


int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
                             int where, u16 *val)
{
	lx_emul_trace_and_stop(__func__);
	return -1;
}


int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
                              int where, u8 val)
{
	lx_emul_trace_and_stop(__func__);
	return -1;
}


int pci_enable_device(struct pci_dev * dev)
{
	/*
	 * 'device_acquire()' should perform the needed steps with
	 * the new platform driver.
	 */
	unsigned value;
	int err;
	
	err = _pci_read_config(dev->devfn, 0x04, 2, &value);
	if (err)
		return -1;

	value |= 1u<<2;
	err = _pci_write_config(dev->devfn, 0x04, 2, value);
	if (err)
		return -1;

	return 0;
}


int pcim_enable_device(struct pci_dev *pdev)
{
	/* for now ignore devres */
	return pci_enable_device(pdev);
}


struct pci_dev * pci_get_class(unsigned int class, struct pci_dev *from)
{
	struct pci_bus *bus;
	struct pci_dev *dev;

	/*
	 * Break endless loop (see 'intel_dsm_detect()') by only querying
	 * the bus on the first executuin.
	 */
	if (from)
		return NULL;

	bus = _pci_bus;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->class == class) {
			return dev;
		}
	}

	return NULL;
}


extern struct irq_chip dde_irqchip_data_chip;


void pci_assign_irq(struct pci_dev * dev)
{
	struct irq_data *irq_data;

	/*
	 * Be lazy and treat irq as hwirq as this is used by the
	 * dde_irqchip_data_chip for (un-)masking.
	 */
	irq_data = irq_get_irq_data(dev->irq);

	irq_data->hwirq = dev->irq;

	irq_set_chip_and_handler(dev->irq, &dde_irqchip_data_chip,
	                         handle_level_irq);
}


resource_size_t __weak pcibios_align_resource(void *data,
                          const struct resource *res,
                          resource_size_t size,
                          resource_size_t align)
{
       return res->start;
}


struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
                                            unsigned int devfn)
{
	struct pci_bus *pbus;
	struct pci_dev *dev;

	pbus = _pci_bus;

	list_for_each_entry(dev, &pbus->devices, bus_list) {
		if (dev->devfn == devfn && dev->class == 0x60000)
			return dev;
	}

	return NULL;
}


void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	struct resource *r;
	unsigned long phys_addr;
	unsigned long size;

	if (!dev || bar > 5) {
		printk("%s:%d: invalid request for dev: %p bar: %d\n",
		       __func__, __LINE__, dev, bar);
		return NULL;
	}

	r = &dev->resource[bar];

	phys_addr = r->start;
	size      = r->end - r->start;

	if (!phys_addr || !size)
		return NULL;

	return lx_emul_io_mem_map(phys_addr, size);
}


static int __init pci_subsys_init(void)
{
	struct pci_bus *b;
	struct pci_sysdata *sd;
	unsigned devfn;

	/* pci_alloc_bus(NULL) */
	b = kzalloc(sizeof (struct pci_bus), GFP_KERNEL);
	if (!b)
		return -ENOMEM;

	sd = kzalloc(sizeof (struct pci_sysdata), GFP_KERNEL);
	if (!sd) {
		kfree(b);
		return -ENOMEM;
	}

	/* needed by intel_fb */
	sd->domain = 0;

	b->sysdata = sd;

	INIT_LIST_HEAD(&b->node);
	INIT_LIST_HEAD(&b->children);
	INIT_LIST_HEAD(&b->devices);
	INIT_LIST_HEAD(&b->slots);
	INIT_LIST_HEAD(&b->resources);
	b->max_bus_speed = PCI_SPEED_UNKNOWN;
	b->cur_bus_speed = PCI_SPEED_UNKNOWN;

	_pci_bus = b;

	/* attach PCI devices */
	for (devfn = 0; devfn < 6 * 8; devfn += 8) {
		unsigned value;

		unsigned subsys;
		unsigned class;
		unsigned bar;
		struct pci_dev *dev;

		int ret = _pci_read_config(devfn, 0x00, 4, &value);
		if (ret)
			continue;

		ret = _pci_read_config(devfn, 0x08, 4, &class);
		if (ret)
			continue;

		ret = _pci_read_config(devfn, 0x2C, 4, &subsys);
		if (ret)
			continue;

		dev = _pci_alloc_dev(b);
		if (!dev)
			break;

		dev->devfn  = devfn;
		dev->vendor = value & 0xffff;
		dev->device = (value >> 16) & 0xffff;

		dev->subsystem_vendor = subsys & 0xffff;
		dev->subsystem_device = (subsys >> 16) & 0xffff;

		ret = _pci_read_config(devfn, 0x3c, 1, &value);
		if (ret) {
			kfree(dev);
			break;
		}

		dev->irq = value;

		dev->dma_mask = 0xffffffff;
		dev->dev.bus  = &pci_bus_type;

		dev->revision = class & 0xff;
		dev->class    = class >> 8;

		dev->current_state = PCI_UNKNOWN;

		for (bar = 0; bar < 6; bar++) {

			unsigned const bar_reg = bar * 4 + 0x10;

			/* size query */
			unsigned value = ~0u;
			unsigned addr;
			unsigned size;

			/*
			 * Use the 'lx_emul_pci_*' API directly to not pollute the
			 * allow-list in '_pci_*_config'.
			 */
			int err = lx_emul_pci_write_config(0, devfn, bar_reg, 4, value);
			if (err)
				continue;
			err = lx_emul_pci_read_config(0, devfn, bar_reg, 4, &value);
			if (err)
				continue;
			size = value;

			err = lx_emul_pci_read_config(0, devfn, bar_reg, 4, &value);
			if (err)
				continue;
			addr = value;

			if (addr & 0x1) {
				dev->resource[bar].start = addr & 0xfffffffcu;
				dev->resource[bar].end   = dev->resource[bar].start + size - 1;
				dev->resource[bar].flags |= IORESOURCE_IO;
			} else {
				dev->resource[bar].start = addr & 0xfffffff8u;
				dev->resource[bar].end   = dev->resource[bar].start + size - 1;
			}
		}

		list_add_tail(&dev->bus_list, &b->devices);

		device_initialize(&dev->dev);

		dev_set_name(&dev->dev, "pci-%u:%u\n", 0, (devfn >> 3) & 0x1fu);
		dev->dev.dma_mask = &dev->dma_mask;

		dev->match_driver = false;
		ret = device_add(&dev->dev);
		if (ret) {
			list_del(&dev->bus_list);
			list_del(&dev->bus_list);
			kfree(dev);
			break;
			kfree(dev);
			break;
		}

		dev->match_driver = true;
		ret = device_attach(&dev->dev);
		if (ret) {
			list_del(&dev->bus_list);
			kfree(dev);
			break;
		}
	}
	return 0;
}


subsys_initcall(pci_subsys_init);


/*
 * Below are dummy implementations that would normally life
 * in 'dummies.c' but need to be implemented anyway by each
 * driver so put them here.
 */

extern void pci_put_host_bridge_device(struct device * dev);
void pci_put_host_bridge_device(struct device * dev)
{
	lx_emul_trace(__func__);
}


struct pci_dev * pci_get_device(unsigned int vendor,unsigned int device,struct pci_dev * from)
{
	lx_emul_trace(__func__);
	return NULL;
}


void pci_set_master(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


int pci_set_mwi(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 1;
}


bool pci_dev_run_wake(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return false;
}


u8 pci_find_capability(struct pci_dev * dev,int cap)
{
	lx_emul_trace(__func__);
	return 0;
}


void __iomem * pci_map_rom(struct pci_dev * pdev,size_t * size)
{
	/*
	 * Needed for VBT access which we do not allow
	 */
	lx_emul_trace(__func__);
	return NULL;
}


int pci_bus_alloc_resource(struct pci_bus *bus, struct resource *res,
        resource_size_t size, resource_size_t align,
        resource_size_t min, unsigned long type_mask,
        resource_size_t (*alignf)(void *,
                      const struct resource *,
                      resource_size_t,
                      resource_size_t),
        void *alignf_data)
{
	lx_emul_trace(__func__);
	return -1;
}

/*
 * \brief  Replaces drivers/pci/pci-driver.c
 * \author Josef Soentgen
 * \date   2022-01-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


#include <linux/pci.h>

void pci_clear_mwi(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


bool pci_dev_run_wake(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


void pci_disable_device(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_enable_device(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


u8 pci_find_capability(struct pci_dev * dev,int cap)
{
	lx_emul_trace_and_stop(__func__);
}


struct pci_dev * pci_get_device(unsigned int vendor,unsigned int device,struct pci_dev * from)
{
	lx_emul_trace_and_stop(__func__);
}


struct pci_dev * pci_get_slot(struct pci_bus * bus,unsigned int devfn)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_read_config_byte(const struct pci_dev * dev,int where,u8 * val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_read_config_dword(const struct pci_dev * dev,int where,u32 * val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_read_config_word(const struct pci_dev * dev,int where,u16 * val)
{
	lx_emul_trace_and_stop(__func__);
}


void pci_set_master(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_set_mwi(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_set_power_state(struct pci_dev * dev,pci_power_t state)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_write_config_byte(const struct pci_dev * dev,int where,u8 val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_write_config_dword(const struct pci_dev * dev,int where,u32 val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_write_config_word(const struct pci_dev * dev,int where,u16 val)
{
	lx_emul_trace_and_stop(__func__);
}

/*
 * \brief  Linux DDE MSI interrupt controller
 * \author Josef Soentgen
 * \date   2026-05-13
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <lx_emul/debug.h>
#include <lx_emul/pci.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irqchip.h>
#include <../kernel/irq/internals.h>
#include <linux/version.h>


static void dde_irq_mask(struct irq_data *d)
{
	printk("%s:%d\n", __func__, __LINE__);
	lx_emul_pci_msi_mask(d->hwirq);
}


static void dde_irq_unmask(struct irq_data *d)
{
	printk("%s:%d\n", __func__, __LINE__);
	lx_emul_pci_msi_unmask(d->hwirq);
}


struct irq_chip dde_msi_irqchip_data_chip = {
	.name           = "dde-msi",
	.irq_mask       = dde_irq_mask,
	.irq_unmask     = dde_irq_unmask,
};


int lx_emul_msi_task_function(void * data)
{
	unsigned msi;
	unsigned long flags;

	for (;;) {
		lx_emul_task_schedule(true);

		for (;;) {
			msi = lx_emul_pci_msi_pending();
			if (!msi)
				break;

			local_irq_save(flags);
			irq_enter();

			generic_handle_irq(msi);

			irq_exit();
			local_irq_restore(flags);
		}
	}

	return 0;
}


static struct task_struct msi_task = {
	.__state         = 0,
	.usage           = REFCOUNT_INIT(2),
	.flags           = PF_KTHREAD,
	.prio            = MAX_PRIO - 20,
	.static_prio     = MAX_PRIO - 20,
	.normal_prio     = MAX_PRIO - 20,
	.policy          = SCHED_NORMAL,
	.cpus_ptr        = &msi_task.cpus_mask,
	.cpus_mask       = CPU_MASK_ALL,
	.nr_cpus_allowed = 1,
	.mm              = NULL,
	.active_mm       = NULL,
	.tasks           = LIST_HEAD_INIT(msi_task.tasks),
	.real_parent     = &msi_task,
	.parent          = &msi_task,
	.children        = LIST_HEAD_INIT(msi_task.children),
	.sibling         = LIST_HEAD_INIT(msi_task.sibling),
	.group_leader    = &msi_task,
	.comm            = "kmsi",
	.thread          = INIT_THREAD,
	.pending         = {
		.list   = LIST_HEAD_INIT(msi_task.pending.list),
		.signal = {{0}}
	},
	.blocked         = {{0}},
};


void * lx_emul_msi_task_struct = &msi_task;

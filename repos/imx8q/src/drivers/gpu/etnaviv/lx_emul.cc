/*
 * \brief  Linux emulation backend functions
 * \author Josef Soentgen
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <util/list.h>

#include <lx_emul_cc.h>

#include <lx_kit/env.h>
#include <lx_kit/malloc.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/work.h>
#include <lx_kit/timer.h>


using size_t = Genode::size_t;


/*****************
 ** DRM session **
 *****************/

#include <drm_component.h>

static Genode::Constructible<Drm::Root> _drm_root { };

void lx_emul_announce_drm_session(void)
{
	if (!_drm_root.constructed()) {
		_drm_root.construct(Lx_kit::env().env, Lx_kit::env().heap);

		Genode::Entrypoint &ep = Lx_kit::env().env.ep();
		Lx_kit::env().env.parent().announce(ep.manage(*_drm_root));
	}
}


Genode::Ram_dataspace_capability lx_drm_object_dataspace(unsigned handle)
{
	(void)handle;
	return Genode::Ram_dataspace_capability();
}


Genode::Dataspace_capability lx_drm_object_gtt_dataspace(unsigned handle)
{
	(void)handle;
	return Genode::Dataspace_capability();
}


/************
 ** memory **
 ************/

void *lx_emul_vzalloc(unsigned long size)
{
	size_t const  real_size = size + sizeof (size_t);
	size_t       *addr = nullptr;

	if (!Lx_kit::env().heap.alloc(real_size, (void **)&addr)) {
		return nullptr;
	}

	if (addr) {
		Genode::memset(addr, 0, real_size);
	}

	*addr = real_size;
	return addr + 1;
}


void lx_emul_vfree(void const *addr)
{
	if (!addr) {
		return;
	}

	size_t const real_size = *(reinterpret_cast<size_t const*>(addr) - 1);
	Lx_kit::env().heap.free(const_cast<void *>(addr), real_size);
}


void *lx_emul_kmalloc(unsigned long size, unsigned int flags)
{
	void *addr = nullptr;

	addr = Lx::Malloc::mem().alloc(size);
	if (!addr)
		return 0;

	if ((Genode::addr_t)addr & 0x3)
		Genode::error("unaligned kmalloc ", (Genode::addr_t)addr);

	// if (flags & __GFP_ZERO)
	// 	Genode::memset(addr, 0, size);

	return addr;
}


void lx_emul_kfree(void const *p)
{
	if (!p) return;

	if (Lx::Malloc::mem().inside((Genode::addr_t)p))
		Lx::Malloc::mem().free(p);
	else
		Genode::error(__func__, ": unknown address: ", p,
		              " from: ", __builtin_return_address(0));
}


struct Kmem_cache : Lx::Slab_alloc,
                    Genode::Registry<Kmem_cache>::Element
{
	void   const *lx;
	size_t const  object_size;

	Kmem_cache(Genode::Registry<Kmem_cache> &registry,
	           void const *lx, size_t object_size, bool dma)
	:
		Lx::Slab_alloc(object_size, dma ? Lx::Slab_backend_alloc::dma()
		                                : Lx::Slab_backend_alloc::mem()),
		Genode::Registry<Kmem_cache>::Element(registry, *this),
		lx(lx),
		object_size(object_size)
	{ }
};


struct Kmem_cache_registry : Genode::Registry<Kmem_cache> { };


Kmem_cache_registry &kmem_cache_registry()
{
	static Kmem_cache_registry reg { };
	return reg;
}


static Kmem_cache *_kmem_cache_lookup(void const *lx)
{
	Kmem_cache *c = nullptr;
	kmem_cache_registry().for_each([&c, lx] (Kmem_cache &kc) {
		if (kc.lx == lx) {
			c = &kc;
		}
	});
	return c;
}


int lx_emul_kmem_cache_create(void const *c, unsigned int size, unsigned int align)
{
	new (Lx::Malloc::mem()) Kmem_cache(kmem_cache_registry(), c, size, false);
	return 0;
}


void lx_emul_kmem_cache_free(void const *c)
{
	Kmem_cache *mc = _kmem_cache_lookup(c);
	if (!c) {
		// warning invalid kmem_cache
	}

	Genode::destroy(Lx::Malloc::mem(), mc);
}


void *lx_emul_kmem_cache_alloc(void const *c)
{
	Kmem_cache *mc = _kmem_cache_lookup(c);
	if (!mc) {
		return nullptr;
	}

	return (void*)mc->alloc();
}


struct Dma_wc_dataspace : Genode::Attached_ram_dataspace,
                          Genode::List<Dma_wc_dataspace>::Element
{
	Dma_wc_dataspace(Genode::size_t size)
	: Genode::Attached_ram_dataspace(Lx_kit::env().env.ram(),
	                                 Lx_kit::env().env.rm(),
	                                 size,
	                                 Genode::Cache_attribute::WRITE_COMBINED) { }
};


static Genode::List<Dma_wc_dataspace> &_dma_wc_ds_list()
{
	static Genode::List<Dma_wc_dataspace> inst;
	return inst;
}


Lx_dma lx_emul_dma_alloc_attrs(void const *dev, unsigned long size, int wc)
{
	if (!wc) {
		Genode::warning("non write-combined DMA memory requested, from: ",
		                __builtin_return_address(0));
	}

	try {
		Dma_wc_dataspace *dma_wc_ds = new (Lx::Malloc::mem()) Dma_wc_dataspace(size);

		_dma_wc_ds_list().insert(dma_wc_ds);

		return Lx_dma {
			.vaddr = (unsigned long)dma_wc_ds->local_addr<void>(),
			.paddr = (unsigned long)Genode::Dataspace_client(dma_wc_ds->cap()).phys_addr()
		};
	} catch (...) { }
	return Lx_dma { .vaddr = 0, .paddr = 0 };
}


void lx_emul_dma_free_attrs(void const *dev, unsigned long size,
                            unsigned long cpu_addr, unsigned long dma_addr)
{
	for (Dma_wc_dataspace *ds = _dma_wc_ds_list().first(); ds; ds = ds->next()) {
		if (ds->local_addr<void>() == (void*)cpu_addr) {
			_dma_wc_ds_list().remove(ds);
			destroy(Lx::Malloc::mem(), ds);
			return;
		}
	}

	Genode::error("dma_free_wc(): unknown address");
}


/********************
 ** platform stuff **
 ********************/

#include <lx_kit/irq.h>

#include <base/attached_dataspace.h>
#include <platform_session/connection.h>

enum Device_id { DEV_UNKNOWN, DEV_GPU3D, };

namespace Lx_kit {
	Platform::Connection    & platform_connection();
	Platform::Device_client & platform_device(Device_id);
}


Platform::Connection & Lx_kit::platform_connection()
{
	static Platform::Connection plat { Lx_kit::env().env };
	return plat;
}


Platform::Device_client & Lx_kit::platform_device(Device_id id)
{
	if (id == DEV_GPU3D) {
		static Platform::Device_client gpu3d {
			platform_connection().device_by_property("compatible",
			                                         "vivante,gc") };
			platform_connection().update();
		return gpu3d;
	}

	struct Platform_device_not_found : Genode::Exception { };
	throw Platform_device_not_found();
}


int lx_emul_devm_request_threaded_irq(int id, int irq, int (*handler)(int, void*), void *dev_id,
                                      int (*thread_fn)(int, void*))
{
	Device_id platform_id = DEV_UNKNOWN;
	switch (id) {
	case GPU3D: platform_id = DEV_GPU3D; break;
	default:
		Genode::error(__func__, "IRQ: ", irq, " not found");
		return -1; // XXX
		break;
	}

	Genode::Irq_session_capability const irq_cap =
		Lx_kit::platform_device(platform_id).irq(0);

	Lx::Irq::irq().request_irq(irq_cap, irq, (Lx::Irq::handler_t)handler,
	                           dev_id, (Lx::Irq::handler_t)thread_fn);
	return 0;
}


void *lx_emul_devm_platform_ioremap_resource(void const *pdev, unsigned int index)
{
	// XXX use pdev to select gpu platform_device
	(void)pdev;

	using namespace Genode;

	Region_map & rm = Lx_kit::env().env.rm();

	try {
		static Attached_dataspace ds {
			rm, Lx_kit::platform_device(DEV_GPU3D).io_mem_dataspace(0) };

		return ds.local_addr<void>();
	} catch (...) { }

	Genode::error("Failed to remap I/O memory pdev: ", pdev, "index: ", index);
	return nullptr;
}


struct clk
{
	void const * dev;
	unsigned long rate;
	char const *id;
};


struct clk *lx_emul_devm_clk_get(void const *dev, char const *id)
{
	// XXX use dev to select gpu platform_device
	using namespace Genode;

	char const *clock_name = id;

	unsigned long rate  = 0;
	Lx_kit::platform_connection().with_xml([&] (Xml_node node) {
		node.for_each_sub_node("device", [&] (Xml_node node) {
			node.for_each_sub_node("clock", [&] (Xml_node node) {
				Genode::log(node);
				if (node.attribute_value("name", String<64>()) != clock_name) {
					return; }
				rate = node.attribute_value<unsigned long>("rate", 0);
			});
		});
	});

	if (!rate) {
		Genode::error(__func__, " clock '", id, " 'not found");
		return nullptr;
	}

	clk *clock = (clk*)lx_emul_kmalloc(sizeof (struct clk), 0);
	if (!clock) {
		Genode::error(__func__, " could not create clk object - ENOMEM");
		return nullptr;
	}
	Genode::memset(clock, sizeof (struct clk), 0);

	clock->dev = dev;
	clock->id = id;
	clock->rate = rate;

	return clock;
}


unsigned long  lx_emul_clk_get_rate(struct clk *clock)
{
	if (!clock) {
		Genode::warning("clock pointer invalid");
		return 0;
	}

	return clock->rate;
}


/*******************
 ** tasking stuff **
 *******************/

struct Registered_task : Genode::Registry<Registered_task>::Element
{
	Lx::Task *task;
	void     *lx_task;

	Registered_task(Genode::Registry<Registered_task> &registry,
	                Lx::Task *task, void *lx_task)
	:
		Genode::Registry<Registered_task>::Element { registry, *this },
		task { task }, lx_task { lx_task }
	{ }
};


struct Task_registry : Genode::Registry<Registered_task>
{
	Lx::Task *lookup_task(void *lx_task)
	{
		Lx::Task *t = nullptr;
		for_each([&t, lx_task] (Registered_task &rt) {
			if (rt.lx_task == lx_task) {
				t = rt.task;
			}
		});

		return t;
	}
};


Task_registry &task_registry()
{
	static Task_registry reg { };
	return reg;
}


int lx_emul_create_task(void *lx_task, int (*threadfn)(void *args), void *args)
{
	try {
		Lx::Task *task =
			new (Lx::Malloc::mem()) Lx::Task((void (*)(void *))threadfn, args, "XXX-task",
			                                 Lx::Task::PRIORITY_2, Lx::scheduler());
		new (Lx::Malloc::mem()) Registered_task(task_registry(), task, lx_task);
		return 0;
	} catch (...) { }

	return -1;
}


unsigned long lx_emul_current_task(void)
{
	if (!Lx::scheduler().active()) {
		Genode::error(__func__, ": scheduler not active");
		Genode::sleep_forever();
	}

	return (unsigned long)Lx::scheduler().current();
}


void lx_emul_block_current_task(void)
{
	if (!Lx::scheduler().active()) {
		Genode::error(__func__, ": scheduler not active");
		Genode::sleep_forever();
	}

	Lx::scheduler().current()->block_and_schedule();
}


void lx_emul_unblock_task(unsigned long lx_task)
{
	if (!Lx::scheduler().active()) {
		Genode::error(__func__, ": scheduler not active");
		Genode::sleep_forever();
	}

	Lx::Task *task = reinterpret_cast<Lx::Task*>(lx_task);
	task->unblock();
}


struct workqueue_struct
{
	Lx::Work *task;
};


struct workqueue_struct *lx_emul_alloc_workqueue(char const *fmt, unsigned int flags)
{
	workqueue_struct *wq =
		(workqueue_struct *)lx_emul_kmalloc(sizeof (workqueue_struct), 0);
	if (!wq) {
		return nullptr;
	}

	Genode::memset(wq, sizeof (workqueue_struct), 0);

	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), fmt);
	wq->task       = work;

	Genode::error("wq: ", wq, " work: ", work);
	return wq;
}


/******************
 ** timing stuff **
 ******************/

unsigned long long lx_emul_ktime_get_mono_fast_ns(void)
{
	return Lx::timer_get_nanoseconds();
}


void lx_emul_usleep(unsigned long us)
{
	Lx::timer().usleep((Genode::uint64_t)us);
}

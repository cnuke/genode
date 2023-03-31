/*
 * \brief  Lx_emul backend for memory allocation
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <os/reporter.h>
#include <cpu/cache.h>

#include <lx_emul/alloc.h>
#include <lx_emul/debug.h>
#include <lx_emul/page_virt.h>
#include <lx_kit/env.h>


struct Mem_track
{
	struct Allocation
	{
		Genode::uint64_t const size  { 0 };
		Genode::uint64_t       count { 0 };
	};

	Allocation _buckets[16] {
		{      16, 0 },
		{      32, 0 },
		{      64, 0 },
		{     128, 0 },
		{     256, 0 },
		{     512, 0 },
		{    1024, 0 },
		{    2048, 0 },
		{    4096, 0 },
		{    8192, 0 },
		{   16384, 0 },
		{   32768, 0 },
		{   65536, 0 },
		{  131072, 0 },
		{  262144, 0 },
		{ 8388608, 0 },
	};

	struct Total
	{
		Genode::uint64_t amount { 0 };
		Genode::uint64_t count  { 0 };
	};

	Total _alloc { };
	Total _free  { };

	Genode::Constructible<Genode::Expanding_reporter> _reporter { };

	Genode::uint64_t _report_count { 0 };

	void _report()
	{
		if (!_reporter.constructed())
			return;

		Genode::Ram_quota const ram_total = Lx_kit::env().env.pd().ram_quota();
		Genode::Ram_quota const ram_used  = Lx_kit::env().env.pd().used_ram();
		Genode::Cap_quota const cap_total = Lx_kit::env().env.pd().cap_quota();
		Genode::Cap_quota const cap_used  = Lx_kit::env().env.pd().used_caps();

		_reporter->generate([&] (Genode::Xml_generator &xml) {
			xml.node("PD", [&] {
				xml.node("ram", [&] {
					xml.attribute("used", ram_used.value);
					xml.attribute("total", ram_total.value);
				});
				xml.node("caps", [&] {
					xml.attribute("used", cap_used.value);
					xml.attribute("total", cap_total.value);
				});
			});
			xml.node("total", [&] {
				xml.node("amount", [&] {
					xml.attribute("alloc", _alloc.amount);
					xml.attribute("free", _free.amount);
					xml.attribute("diff", _alloc.amount - _free.amount);
				});
				xml.node("count", [&] {
					xml.attribute("alloc", _alloc.count);
					xml.attribute("free", _free.count);
					xml.attribute("diff", _alloc.count - _free.count);
				});
			});

			xml.node("buckets", [&] {
				for (auto v : _buckets) {
					xml.node("bucket", [&] {
						xml.attribute("size", v.size);
						xml.attribute("count", v.count);
					});
				}
			});
		});
	}

	Mem_track(Genode::Env &env, bool reporting = true)
	{
		if (reporting)
			_reporter.construct(env, "mem_track", "mem_track");
	}

	void alloc(void const *, unsigned long size)
	{
		for (auto &v : _buckets) {
			if (size <= v.size) {
				v.count++;
				break;
			}
		}
		_alloc.amount += size;
		if (size > 131072) {
			Genode::log(__func__, ": size: ", size, " > ", 131072);
			lx_emul_backtrace();
		}
		// if (size == 4096) {
		// 	Genode::log(__func__, ": size: ", size);
		// 	lx_emul_backtrace();
		// }

		_alloc.count++;

		if (++_report_count >= 100) {
			_report_count = 0;
			_report();
		}
	}

	void free(void const * ptr, unsigned long size)
	{
		if (size > 131072) {
			Genode::log(__func__, ": size: ", size, " > ", 131072);
			lx_emul_backtrace();
		}

		for (auto &v : _buckets) {
			if (size <= v.size) {
				if (v.count)
					v.count--;
				else
					Genode::warning(__func__, ": ptr: ", ptr,
					                " size: ", size, " was not counted");
				break;
			}
		}
		_free.count++;
		_free.amount += size;
	}
};

Genode::Constructible<Mem_track> mem_track { };


extern "C" void * lx_emul_mem_alloc_aligned(unsigned long size, unsigned long align)
{
	void * const ptr = Lx_kit::env().memory.alloc(size, align, &lx_emul_add_page_range);
	if (!mem_track.constructed())
		mem_track.construct(Lx_kit::env().env);
	mem_track->alloc(ptr, size);
	return ptr;
};


extern "C" void * lx_emul_mem_alloc_aligned_uncached(unsigned long size,
                                                     unsigned long align)
{
	void * const ptr = Lx_kit::env().uncached_memory.alloc(size, align, &lx_emul_add_page_range);
	if (!mem_track.constructed())
		mem_track.construct(Lx_kit::env().env);
	mem_track->alloc(ptr, size);
	return ptr;
};


extern "C" unsigned long lx_emul_mem_dma_addr(void * addr)
{
	unsigned long ret = Lx_kit::env().memory.dma_addr(addr);
	if (ret)
		return ret;
	if (!(ret = Lx_kit::env().uncached_memory.dma_addr(addr)))
		Genode::error(__func__, " called with invalid addr ", addr);
	return ret;
}


extern "C" unsigned long lx_emul_mem_virt_addr(void * dma_addr)
{
	unsigned long ret = Lx_kit::env().memory.virt_addr(dma_addr);
	if (ret)
		return ret;
	if (!(ret = Lx_kit::env().uncached_memory.virt_addr(dma_addr)))
		Genode::error(__func__, " called with invalid dma_addr ", dma_addr);
	return ret;
}


extern "C" void lx_emul_mem_free(const void * ptr)
{
	if (!ptr)
		return;

	unsigned long const size = lx_emul_mem_size(ptr);
	if (!size) {
		Genode::warning(__func__, ": ptr: ", ptr, " size 0");
		lx_emul_backtrace();
	}

	if (!mem_track.constructed())
		mem_track.construct(Lx_kit::env().env);
	mem_track->free(ptr, size);

	if (Lx_kit::env().memory.free(ptr))
		return;
	if (Lx_kit::env().uncached_memory.free(ptr))
		return;
	Genode::error(__func__, " called with invalid ptr ", ptr);
};


extern "C" unsigned long lx_emul_mem_size(const void * ptr)
{
	unsigned long ret = 0;
	if (!ptr)
		return ret;
	if ((ret = Lx_kit::env().memory.size(ptr)))
		return ret;
	if (!(ret = Lx_kit::env().uncached_memory.size(ptr)))
		Genode::error(__func__, " called with invalid ptr ", ptr);
	return ret;
};


extern "C" void lx_emul_mem_cache_clean_invalidate(const void * addr,
                                                   unsigned long size)
{
	Genode::cache_clean_invalidate_data((Genode::addr_t)addr, size);
}


extern "C" void lx_emul_mem_cache_invalidate(const void * addr,
                                             unsigned long size)
{
	Genode::cache_invalidate_data((Genode::addr_t)addr, size);
}


/*
 * Heap for lx_emul metadata - unprepared for Linux code
 */

void * lx_emul_heap_alloc(unsigned long size)
{
	void *ptr = Lx_kit::env().heap.alloc(size);
	if (ptr)
		Genode::memset(ptr, 0, size);
	return ptr;
}


void lx_emul_heap_free(void * ptr)
{
	Lx_kit::env().heap.free(ptr, 0);
}

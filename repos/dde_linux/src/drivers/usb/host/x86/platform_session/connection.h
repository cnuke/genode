/*
 * \brief  Dummy - platform session device interface
 * \author Stefan Kalkowski
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_SESSION__CONNECTION_H_
#define _PLATFORM_SESSION__CONNECTION_H_

#include <util/mmio.h>
#include <util/string.h>
#include <base/exception.h>
#include <io_mem_session/client.h>
#include <irq_session/client.h>

namespace Platform {
	struct Connection;

	using namespace Genode;
}


struct Platform::Connection
{
	Connection(Genode::Env &)
	{
		Genode::error(__func__, ": this: ", this, " not implemented");
	}

	void update()
	{
		Genode::error(__func__, ": not implemented");
	}

	template <typename FN>
	void with_xml(FN const & fn)
	{
		(void)fn;

		Genode::error(__func__, ": not implemented");
	}

	Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache)
	{
		Genode::error(__func__, ": size: ", size, " cache: ", (unsigned)cache, " not implemented");
		return Ram_dataspace_capability();
	}

	void free_dma_buffer(Ram_dataspace_capability)
	{
		Genode::error(__func__, ": not implemented");
	}

	addr_t dma_addr(Ram_dataspace_capability)
	{
		Genode::error(__func__, ": not implemented");
		return 0;
	}
};

#endif /* _PLATFORM_SESSION__CONNECTION_H_ */

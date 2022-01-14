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

#define Platform Legacy_platform
#include_next <platform_session/connection.h>
#undef Platform


struct Platform::Connection
{
	Genode::Env &_env;

	Genode::Constructible<Legacy_platform::Connection> _legacy_platform { };

	Connection(Genode::Env &);

	void update();

	template <typename FN> void with_xml(FN const & fn)
	{
		Genode::Xml_node xml { "<emtpy/>", 8 };
		fn(xml);
	}

	Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache);

	void free_dma_buffer(Ram_dataspace_capability);

	addr_t dma_addr(Ram_dataspace_capability);
};

#endif /* _PLATFORM_SESSION__CONNECTION_H_ */

/*
 * \brief  'Mem_alloc' content copied from parent via 'fork'
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2022-03-21
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__CLONED_MEM_ALLOC_RANGE_H_
#define _LIBC__INTERNAL__CLONED_MEM_ALLOC_RANGE_H_

/* Genode includes */
#include <region_map/region_map.h>

/* libc-internal includes */
#include <internal/clone_session.h>

namespace Libc { struct Cloned_mem_alloc_range; }


struct Libc::Cloned_mem_alloc_range
{
	Ram_dataspace_capability ds;

	size_t const size;
	void * const local_addr;

	Cloned_mem_alloc_range(Ram_allocator &ram, Region_map &rm,
	                       void *start, size_t size)
	try :
		ds(ram.alloc(size)), size(size),
		local_addr(rm.attach_at(ds, (addr_t)start))
	{ }
	catch (Region_map::Region_conflict) {
		error("could not clone Mem_alloc region ", Hex_range((addr_t)start, size));
		throw;
	}

	void import_content(Clone_connection &clone_connection)
	{
		clone_connection.memory_content(local_addr, size);
	}

	virtual ~Cloned_mem_alloc_range() { }
};

#endif /* _LIBC__INTERNAL__CLONED_MEM_ALLOC_RANGE_H_ */

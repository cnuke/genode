/*
 * \brief  Utilitize used by the NVMe driver
 * \author Josef Soentgen
 * \date   2018-03-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NVME_UTIL_H_
#define _NVME_UTIL_H_

/* Genode includes */
#include <base/fixed_stdint.h>

namespace Util {

	using namespace Genode;

	/*
	 * DMA allocator helper
	 */
	struct Dma_allocator : Genode::Interface
	{
		virtual Genode::Ram_dataspace_capability alloc(size_t) = 0;
		virtual void free(Genode::Ram_dataspace_capability) = 0;
	};
}

#endif /* _NVME_UTIL_H_ */

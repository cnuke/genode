/*
 * \brief  Genode HW backend glue implementation
 * \author Josef Soentgen
 * \date   2017-10-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBHWBASE__GLUE_H_
#define _LIBHWBASE__GLUE_H_

/* Genode includes */
#include <base/env.h>

namespace Libhwbase {
	void init(Genode::Env &env);
	bool handle_io_port(unsigned short, unsigned int);
	Platform::Device_capability pci_dev_cap();
}

#endif /* _LIBHWBASE__GLUE_H_ */

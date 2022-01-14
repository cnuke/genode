/*
 * \brief  Lx_emul backend for accessing PCI(e) config space
 * \author Josef Soentgen
 * \date   2022-01-18
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/pci_config_space.h>

int lx_emul_pci_read_config(unsigned bus, unsigned devfn,
                            unsigned reg, unsigned len, unsigned *val)
{
	using namespace Lx_kit;
	using namespace Genode;

	bool result = false;
	bool matched = false;
	env().devices.for_each([&] (Device & d) {
		matched = d.matches(bus, devfn);
		if (matched && val)
			result = d.read_config(reg, len, val);
	});

	if (result && matched && val)
		log("read ", reg, " (", len, "): ", Genode::Hex(*val));

	if (!result && matched)
		error("could not read config space register ", Hex(reg));

	return result ? 0 : -1;
}


int lx_emul_pci_write_config(unsigned bus, unsigned devfn,
                             unsigned reg, unsigned len, unsigned val)
{
	using namespace Lx_kit;
	using namespace Genode;

	bool result = false;
	bool matched = false;
	env().devices.for_each ([&] (Device & d) {
		matched = d.matches(bus, devfn);
		if (matched)
			result = d.write_config(reg, len, val);
	});

	if (result && matched)
		log("wrote ", reg, " (", len, "): ", Genode::Hex(val));

	if (!result && matched)
		error("could not write config space register ", Hex(reg),
		      " with ", Hex(val));

	return result ? 0 : -1;
}

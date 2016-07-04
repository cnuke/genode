/*
 * \brief  Argon2 test
 * \author Josef Soentgen
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>

/* argon2 includes */
#include <argon2.h>


Genode::size_t Component::stack_size() { return 1024 * sizeof(long); }


void Component::construct(Genode::Env &env)
{
	Genode::String<8> pw("foobar");
	Genode::String<8> salt("saltsalt");

	char hash[32];

	uint32_t const t_cost = 8;
	/* if too high, e.g. > 1MiB, will generated SIGSEGV */
	uint32_t const m_cost = 64 * (1<<10);
	uint32_t const p = 1;

	int const err = argon2_hash(t_cost, m_cost, p, pw.string(), pw.length(),
	                            salt.string(), salt.length(), hash,
	                            sizeof(hash), nullptr, 0, Argon2_i,
	                            ARGON2_VERSION_13);
	if (err != ARGON2_OK) {
		Genode::error("argon2_hash returned: ", err);
	}

	env.parent().exit(0);
}

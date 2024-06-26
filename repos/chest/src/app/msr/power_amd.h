/*
 * \author Alexander Boettcher
 * \date   2022-10-15
 */

/*
 * Copyright (C) 2022-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include "cpuid.h"

namespace Msr {
	using Genode::uint64_t;
	using Genode::uint8_t;
	struct Power_amd;
}

struct Msr::Power_amd
{
	Cpuid cpuid { };

	uint64_t pstate_limit  { };
	uint64_t pstate_ctrl   { };
	uint64_t pstate_status { };

	uint64_t swpwracc    { };
	uint64_t swpwraccmax { };

	bool valid_pstate_limit  { };
	bool valid_pstate_ctrl   { };
	bool valid_pstate_status { };

	bool valid_swpwracc      { };
	bool valid_swpwraccmax   { };

	struct Pstate_limit : Genode::Register<64> {
		struct Cur_limit : Bitfield< 0, 4> { };
		struct Max_value : Bitfield< 4, 4> { };
	};

	struct Pstate_ctrl : Genode::Register<64> {
		struct Command : Bitfield< 0, 4> { };
	};

	struct Pstate_status : Genode::Register<64> {
		struct Status : Bitfield< 0, 4> { };
	};

	enum {
		AMD_PSTATE_LIMIT  = 0xc0010061,
		AMD_PSTATE_CTRL   = 0xc0010062,
		AMD_PSTATE_STATUS = 0xc0010063,

		AMD_CPUSWPWRACC    = 0xc001007a,
		AMD_MAXCPUSWPWRACC = 0xc001007b,
	};

	void read_pstate(System_control &system)
	{
		System_control::State state { };

		system.add_rdmsr(state, AMD_PSTATE_LIMIT);
		system.add_rdmsr(state, AMD_PSTATE_CTRL);
		system.add_rdmsr(state, AMD_PSTATE_STATUS);

		state = system.system_control(state);

		addr_t success = 0;
		bool    result = system.get_state(state, success, &pstate_limit,
		                                  &pstate_ctrl, &pstate_status);

		valid_pstate_limit  = result && (success & 1);
		valid_pstate_ctrl   = result && (success & 2);
		valid_pstate_status = result && (success & 4);
	}

	bool write_pstate(System_control &system, uint64_t const &value) const
	{
		System_control::State state { };

		system.add_wrmsr(state, AMD_PSTATE_CTRL, value);

		state = system.system_control(state);

		addr_t success = 0;
		bool   result  = system.get_state(state, success);

		return result && (success & 1);
	}

	void read_power(System_control &system)
	{
		System_control::State state { };

		system.add_rdmsr(state, AMD_CPUSWPWRACC);
		system.add_rdmsr(state, AMD_MAXCPUSWPWRACC);

		state = system.system_control(state);

		addr_t success = 0;
		bool    result = system.get_state(state, success, &swpwracc,
		                                  &swpwraccmax);

		valid_swpwracc    = result && (success & 1);
		valid_swpwraccmax = result && (success & 2);
	}

	void update(System_control &);
	void update(System_control &, Genode::Xml_node const &);
	void report(Genode::Xml_generator &) const;
};

void Msr::Power_amd::update(System_control &system)
{
	if (cpuid.pstate_support())
		read_pstate(system);

	if (cpuid.amd_pwr_report())
		read_power(system);
}

void Msr::Power_amd::report(Genode::Xml_generator &xml) const
{
	if (cpuid.pstate_support()) {
		xml.node("pstate", [&] () {
			if (valid_pstate_limit) {
				xml.attribute("ro_limit_cur", Pstate_limit::Cur_limit::get(pstate_limit));
				xml.attribute("ro_max_value", Pstate_limit::Max_value::get(pstate_limit));
			}
			if (valid_pstate_ctrl) {
				xml.attribute("rw_command", Pstate_ctrl::Command::get(pstate_ctrl));
			}
			if (valid_pstate_status) {
				xml.attribute("ro_status", Pstate_status::Status::get(pstate_status));
			}
		});
	}

	if (cpuid.amd_pwr_report() || cpuid.amd_cppc()) {
		xml.node("power", [&] () {
			/* unimplemented by kernel by now - just report the feature atm */
			xml.attribute("amd_pwr_report", cpuid.amd_pwr_report());
			xml.attribute("amd_cpc", cpuid.amd_cppc());
			if (valid_swpwracc)
				xml.attribute("swpwracc", swpwracc);
			if (valid_swpwraccmax)
				xml.attribute("swpwraccmax", swpwraccmax);
		});
	}
}

void Msr::Power_amd::update(System_control &system, Genode::Xml_node const &config)
{
	using Genode::warning;

	bool const verbose = config.attribute_value("verbose", false);

	config.with_optional_sub_node("pstate", [&] (Genode::Xml_node const &node) {
		if (!cpuid.pstate_support())
			return;

		if (!node.has_attribute("rw_command"))
			return;

		unsigned value = node.attribute_value("rw_command", 0u /* max */);

		if (valid_pstate_limit && value > Pstate_limit::Max_value::get(pstate_limit)) {
			if (verbose)
				warning("pstate - out of range - ", value, " [0-",
				        Pstate_limit::Max_value::get(pstate_limit), "]");
			return;
		}

		if (!write_pstate(system, value)) {
			if (verbose)
				warning("pstate - setting ", value, " failed");
			Genode::error("write failed");
		}
	});
}

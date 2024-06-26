/*
 * \author Alexander Boettcher
 * \date   2021-10-24
 */

/*
 * Copyright (C) 2021-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

namespace Msr {
	struct Msr;
	struct Monitoring;
	using Genode::uint8_t;
	using Genode::uint64_t;
};

struct Msr::Monitoring
{
	virtual ~Monitoring() { }

	uint64_t mperf { };
	uint64_t aperf { };
	uint64_t freq_khz { };
	bool     freq_khz_valid { };

	uint8_t temp_tcc           { };
	bool    temp_tcc_valid     { };
	uint8_t temp_package       { };
	bool    temp_package_valid { };
	uint8_t temp_cpu           { };
	bool    temp_cpu_valid     { };

	void update_package_temperature(System_control &system)
	{
		enum Registers { IA32_PKG_THERM_STATUS = 0x1b1 };

		System_control::State state { };

		system.add_rdmsr(state, IA32_PKG_THERM_STATUS);

		state = system.system_control(state);

		addr_t   success = 0;
		uint64_t status  = 0;

		bool result = system.get_state(state, success, &status);

		temp_package_valid = result && (success & 1);
		if (!temp_package_valid)
			return;

		struct Status : Genode::Register<64> {
			struct Temperature : Bitfield<16, 7> { }; };

		temp_package = uint8_t(Status::Temperature::get(status));
	}

	void update_cpu_temperature(System_control &system)
	{
		enum Registers { IA32_THERM_STATUS = 0x19c };

		System_control::State state { };

		system.add_rdmsr(state, IA32_THERM_STATUS);

		state = system.system_control(state);

		addr_t   success = 0;
		uint64_t status  = 0;

		bool result = system.get_state(state, success, &status);

		struct Status : Genode::Register<64> {
			struct Temperature : Bitfield<16, 7> { };
			struct Valid       : Bitfield<31, 1> { };
		};

		temp_cpu_valid = result && (success & 1) &&
		                 Status::Valid::get(status);

		if (!temp_cpu_valid)
			return;

		temp_cpu = uint8_t(Status::Temperature::get(status));
	}

	void target_temperature(System_control &system)
	{
		enum Registers { MSR_TEMPERATURE_TARGET = 0x1a2 };

		System_control::State state { };

		system.add_rdmsr(state, MSR_TEMPERATURE_TARGET);

		state = system.system_control(state);

		addr_t   success = 0;
		uint64_t target  = 0;

		bool result = system.get_state(state, success, &target);

		temp_tcc_valid = result && (success & 1);

		if (!temp_tcc_valid)
			return;

		struct Target : Genode::Register<64> {
			struct Temperature : Bitfield<16, 8> { }; };

		temp_tcc = uint8_t(Target::Temperature::get(target));
	}

	static bool mperf_aperf(System_control &system, uint64_t &mperf, uint64_t &aperf)
	{
		enum Registers {
			IA32_MPERF = 0xe7,
			IA32_APERF = 0xe8,
		};

		System_control::State state { };

		system.add_rdmsr(state, IA32_MPERF);
		system.add_rdmsr(state, IA32_APERF);

		state = system.system_control(state);

		addr_t success = 0;

		return system.get_state(state, success, &mperf, &aperf) &&
		       (success == 3);
	}

	void cpu_frequency(System_control &system, uint64_t const tsc_freq_khz)
	{
		uint64_t mcurr = 0ULL, acurr = 0ULL;

		freq_khz_valid = mperf_aperf(system, mcurr, acurr);
		if (!freq_khz_valid)
			return;

		uint64_t mdiff = mcurr > mperf ? mcurr - mperf : 0ull;
		uint64_t adiff = acurr > aperf ? acurr - aperf : 0ull;

		if ((~0ULL / tsc_freq_khz) > adiff)
			freq_khz = mdiff ? (adiff * tsc_freq_khz) / mdiff : 0ull;
		else
			freq_khz = mdiff ? (adiff / mdiff) * tsc_freq_khz : 0ull;

		mperf = mcurr;
		aperf = acurr;
	}

	static bool supported(System_control &system, bool const amd, bool const intel)
	{
		if (!amd && !intel)
			return false;

		uint64_t mperf = 0ULL, aperf = 0ULL;
		return mperf_aperf(system, mperf, aperf);
	}

	void report(Genode::Xml_generator &xml, unsigned const tcc) const
	{
		if (freq_khz_valid)
			xml.attribute("freq_khz", freq_khz);
		if (tcc && temp_cpu_valid)
			xml.attribute("temp_c", tcc - temp_cpu);
	}
};

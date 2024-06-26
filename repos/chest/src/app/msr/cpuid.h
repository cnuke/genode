/*
 * \brief  CPUID support
 * \author Martin Stein
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

namespace Msr
{
	struct Cpuid;

	void
	cpuid(unsigned *ax, unsigned *bx, unsigned *cx, unsigned *dx)
	{
		asm volatile ("cpuid" : "+a" (*ax), "+d" (*dx), "+b" (*bx), "+c"(*cx)
		                      :: "memory");
	}

	void cpuid(unsigned const idx, unsigned &a, unsigned &b,
	           unsigned &c, unsigned &d)
	{
		a = idx;
		b = c = d = 0;
		cpuid(&a, &b, &c, &d);
	}
}

struct Msr::Cpuid
{
	enum { MAX_LEAF_IDX = 9 };

	unsigned eax[MAX_LEAF_IDX] { };
	unsigned ebx[MAX_LEAF_IDX] { };
	unsigned ecx[MAX_LEAF_IDX] { };
	unsigned edx[MAX_LEAF_IDX] { };

	unsigned eax_8000[MAX_LEAF_IDX] { };
	unsigned ebx_8000[MAX_LEAF_IDX] { };
	unsigned ecx_8000[MAX_LEAF_IDX] { };
	unsigned edx_8000[MAX_LEAF_IDX] { };

	uint8_t core_type { };

	enum Core_type
	{
		INTEL_ATOM = 0x20,
		INTEL_CORE = 0x40,
	};

	Cpuid()
	{
		cpuid (0, eax[0], ebx[0], ecx[0], edx[0]);
		for (auto idx = 1u; idx <= eax[0] && idx < MAX_LEAF_IDX; idx++) {
			cpuid (idx, eax[idx], ebx[idx], ecx[idx], edx[idx]);
		}

		if (eax[0] >= 0x1a) {
			unsigned eax, ebx, ecx, edx { };
			cpuid (0x1a, eax, ebx, ecx, edx);
			core_type = uint8_t((eax >> 24) & 0xffu);
		}

		unsigned const ids_8000 = 0x80000000u;

		cpuid (ids_8000, eax_8000[0], ebx_8000[0], ecx_8000[0], edx_8000[0]);

		for (auto idx = 1u; idx <= max_id_8000() && idx < MAX_LEAF_IDX; idx++) {
			cpuid(ids_8000 + idx, eax_8000[idx], ebx_8000[idx],
			      ecx_8000[idx], edx_8000[idx]);
		}
	}

	uint8_t max_id_8000() const { return uint8_t(eax_8000[0]); }

    using Family_id = unsigned;
    enum { FAMILY_ID_UNKNOWN = ~static_cast<unsigned>(0) };

    Family_id family_id() const
    {
        if (eax[0] < 1) {
            return FAMILY_ID_UNKNOWN;
        }
        enum { FAMILY_ID_SHIFT = 8 };
        enum { FAMILY_ID_MASK = 0xf };
        enum { EXT_FAMILY_ID_SHIFT = 20 };
        enum { EXT_FAMILY_ID_MASK = 0xff };
        Family_id family_id {
            (eax[1] >> FAMILY_ID_SHIFT) & FAMILY_ID_MASK };

        if (family_id == 15) {
            family_id += (eax[1] >> EXT_FAMILY_ID_SHIFT) & EXT_FAMILY_ID_MASK;
        }
        return family_id;
    }

    using Model_id = unsigned;
    enum { MODEL_ID_UNKNOWN = ~static_cast<unsigned>(0) };

    Model_id model_id() const
    {
        if (eax[0] < 1) {
            return MODEL_ID_UNKNOWN;
        }
        enum { MODEL_ID_SHIFT = 4 };
        enum { MODEL_ID_MASK = 0xf };
        enum { EXT_MODEL_ID_SHIFT = 16 };
        enum { EXT_MODEL_ID_MASK = 0xf };
        unsigned const fam_id { family_id() };
        unsigned model_id { (eax[1] >> MODEL_ID_SHIFT) & MODEL_ID_MASK };
        if (fam_id == 6 ||
            fam_id == 15)
        {
            model_id +=
                ((eax[1] >> EXT_MODEL_ID_SHIFT) & EXT_MODEL_ID_MASK) << 4;
        }

        return model_id;
    }

	bool hwp() const
	{
		if (eax[0] < 6) {
			return false;
		}
		return ((eax[6] >> 7) & 1) == 1;
	}

	bool hwp_energy_perf_pref() const
	{
		if (eax[0] < 6) {
			return false;
		}
		return ((eax[6] >> 10) & 1) == 1;
	}

	bool hardware_coordination_feedback_cap() const
	{
		if (eax[0] < 6) {
			return false;
		}
		return ((ecx[6] >> 0) & 1) == 1;
	}

	bool hwp_energy_perf_bias() const
	{
		if (eax[0] < 6) {
			return false;
		}
		return ((ecx[6] >> 3) & 1) == 1;
	}

	bool pstate_support() const
	{
		if (max_id_8000() < 7)
			return false;

		return !!(edx_8000[7] & (1 << 7));
	}

	bool enhanced_speedstep() const {
		return (ecx[0] >= 1) && (ecx[1] >> 7) & 1; }

	bool amd_cppc() const
	{
		if (max_id_8000() < 8)
			return false;

		return !!(ebx_8000[8] & (1 << 27));
	}

	bool amd_pwr_report() const
	{
		if (max_id_8000() < 7)
			return false;

		return !!(edx_8000[7] & (1 << 12));
	}
};

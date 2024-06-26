/*
 * \author Alexander Boettcher
 * \date   2023-10-02
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

namespace Msr {
	struct Msr;
	using namespace Genode;
	struct System_control;
};

struct Msr::System_control: Genode::Rpc_client<Pd_session::System_control>
{
	typedef Pd_session::Managing_system_state State;

	explicit System_control(Capability<Pd_session::System_control> cap)
	: Rpc_client<Pd_session::System_control>(cap) { }

	State system_control(State const &state) override {
		return call<Rpc_system_control>(state); }

	void add_rdmsr(State &state, uint64_t const msr) const
	{
		state.trapno = Genode::Cpu_state::MSR_ACCESS;

		switch (state.ip) {
		case 0: state.r8  = msr; state.ip ++; break;
		case 1: state.r9  = msr; state.ip ++; break;
		case 2: state.r10 = msr; state.ip ++; break;
		case 3: state.r11 = msr; state.ip ++; break;
		case 4: state.r12 = msr; state.ip ++; break;
		case 5: state.r13 = msr; state.ip ++; break;
		case 6: state.r14 = msr; state.ip ++; break;
		case 7: state.r15 = msr; state.ip ++; break;
		default:
			error("too many rdmsr");
		}
	}

	void add_wrmsr(State &state, uint64_t msr, uint64_t const value) const
	{
		state.trapno = Genode::Cpu_state::MSR_ACCESS;

		msr |= 1u << 29; /* wrmsr tag */

		switch (state.ip) {
		case 0: state.r8  = msr; state.r9  = value; state.ip += 2; break;
		case 1: state.r9  = msr; state.r10 = value, state.ip += 2; break;
		case 2: state.r10 = msr; state.r11 = value, state.ip += 2; break;
		case 3: state.r11 = msr; state.r12 = value, state.ip += 2; break;
		case 4: state.r12 = msr; state.r13 = value, state.ip += 2; break;
		case 5: state.r13 = msr; state.r14 = value, state.ip += 2; break;
		case 6: state.r14 = msr; state.r15 = value, state.ip += 2; break;
		default:
			error("too many wrmsr");
		}
	}

	bool get_state(State const &state, addr_t &success,
	               uint64_t * msr_1 = nullptr, uint64_t * msr_2 = nullptr,
	               uint64_t * msr_3 = nullptr, uint64_t * msr_4 = nullptr,
	               uint64_t * msr_5 = nullptr, uint64_t * msr_6 = nullptr,
	               uint64_t * msr_7 = nullptr, uint64_t * msr_8 = nullptr)
	{
		if (msr_1) *msr_1 = state.r8;
		if (msr_2) *msr_2 = state.r9;
		if (msr_3) *msr_3 = state.r10;
		if (msr_4) *msr_4 = state.r11;
		if (msr_5) *msr_5 = state.r12;
		if (msr_6) *msr_6 = state.r13;
		if (msr_7) *msr_7 = state.r14;
		if (msr_8) *msr_8 = state.r15;

		success = state.ip;

		return !!state.trapno;
	}
};

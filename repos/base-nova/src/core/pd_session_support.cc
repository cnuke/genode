/*
 * \brief  Extension of core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pd_session_component.h>
#include <assertion.h>

using namespace Core;

template <typename FUNC>
inline Nova::uint8_t retry_syscall(addr_t pd_sel, FUNC func)
{
	Nova::uint8_t res;
	do {
		res = func();
	} while (res == Nova::NOVA_PD_OOM &&
	         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
	                                                   pd_sel,
	                                                   "core", "ep",
	                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

	return res;
}

bool Pd_session_component::assign_pci(addr_t pci_config_memory, uint16_t bdf)
{
	return retry_syscall(_pd->pd_sel(), [&]() {
		return Nova::assign_pci(_pd->pd_sel(), pci_config_memory, bdf);
	}) == Nova::NOVA_OK;
}


void Pd_session_component::map(addr_t virt, addr_t size)
{
	Platform_pd &target_pd = *_pd;
	Nova::Utcb  &utcb      = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
	addr_t const pd_core   = platform_specific().core_pd_sel();
	addr_t const pd_dst    = target_pd.pd_sel();

	auto map_memory = [&] (Mapping const &mapping)
	{
		/* asynchronously map memory */
		uint8_t err = retry_syscall(_pd->pd_sel(), [&]() {
			utcb.set_msg_word(0);

			bool res = utcb.append_item(nova_src_crd(mapping), 0, true, false,
			                            false,
			                            mapping.dma_buffer,
			                            mapping.write_combined);

			/* one item ever fits on the UTCB */
			(void)res;

			return Nova::delegate(pd_core, pd_dst, nova_dst_crd(mapping));
		});

		if (err != Nova::NOVA_OK) {
			error("could not eagerly map memory ",
			      Hex_range<addr_t>(mapping.dst_addr, 1UL << mapping.size_log2) , " "
			      "error=", err);
		}
	};

	try {
		while (size) {

			Fault const artificial_fault {
				.hotspot = { virt },
				.access  = Access::READ,
				.rwx     = Rwx::rwx(),
				.bounds  = { .start = 0, .end  = ~0UL },
			};

			_address_space.with_mapping_for_fault(artificial_fault,
				[&] (Mapping const &mapping)
				{
					map_memory(mapping);

					size_t const mapped_bytes = 1 << mapping.size_log2;

					virt += mapped_bytes;
					size  = size < mapped_bytes ? 0 : size - mapped_bytes;
				},

				[&] (Region_map_component &, Fault const &) { /* don't reflect */ }
			);
		}
	} catch (...) {
		error(__func__, " failed ", Hex(virt), "+", Hex(size));
	}
}


using State = Genode::Pd_session::Managing_system_state;

static State acpi_suspend(State const &request)
{
	State respond { .trapno = 0 };

	/*
	 * The trapno/ip/sp registers used below are just convention to transfer
	 * the intended sleep state S0 ... S5. The values are read out by an
	 * ACPI AML component and are of type TYP_SLPx as described in the
	 * ACPI specification, e.g. TYP_SLPa and TYP_SLPb. The values differ
	 * between different PC systems/boards.
	 *
	 * \note trapno/ip/sp registers are chosen because they exist in
	 *       Managing_system_state for x86_32 and x86_64.
	 */
	uint8_t const sleep_type_a = uint8_t(request.ip);
	uint8_t const sleep_type_b = uint8_t(request.sp);

	auto const cap_suspend = platform_specific().core_pd_sel() + 3;
	auto const result      = Nova::acpi_suspend(cap_suspend, sleep_type_a,
	                                            sleep_type_b);

	if (result == Nova::NOVA_OK)
		respond.trapno = 1 /* success, which means we resumed already */;

	return respond;
}


static State msr_access_cap(State const &, Platform_pd &target_pd)
{
	enum { SM_MSR = 0x20 };  /* convention */

	Genode::addr_t const  pd_core   = platform_specific().core_pd_sel();
	Genode::addr_t const  pd_dst    = target_pd.pd_sel();
	Nova::Utcb           &utcb      = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());

	unsigned const msr_cap = platform_specific().core_pd_sel() + 4;

	Nova::Obj_crd src_crd(msr_cap, 0 /* order */);
	Nova::Obj_crd dst_crd(SM_MSR , 0 /* order */);

	retry_syscall(target_pd.pd_sel(), [&]() {
		utcb.set_msg_word(0);
		bool res = utcb.append_item(src_crd, 0 /* hotspot */, true /* kernel pd */);
		/* one item ever fits on the UTCB */
		(void)res;
		return Nova::delegate(pd_core, pd_dst, dst_crd);
	});

	return State();
}


State Pd_session_component::managing_system(State const &request)
{
	if (_managing_system != Managing_system::PERMITTED) {
		return State();
	}

	if (request.trapno == State::ACPI_SUSPEND_REQUEST)
		return acpi_suspend(request); 

	if (request.trapno == State::ACPI_SUSPEND_REQUEST + 1 /* XXX */)
		return msr_access_cap(request, *_pd);

	return State();
}

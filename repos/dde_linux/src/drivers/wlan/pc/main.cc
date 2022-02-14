/*
 * \brief  Wireless network driver Linux port
 * \author Josef Soentgen
 * \date   2022-02-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <genode_c_api/uplink.h>

/* DDE Linux includes */
#include <lx_emul/init.h>
#include <lx_emul/page_virt.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>
#include <lx_user/io.h>

/* local includes */
#include "frontend.h"


extern "C" void lx_user_init(void) { }
extern "C" void lx_user_handle_io(void) { }


using namespace Genode;


struct Main : private Entrypoint::Io_progress_handler
{
	Env                    &_env;
	Io_signal_handler<Main> _signal_handler { _env.ep(), *this,
	                                          &Main::_handle_signal };

	/**
	 * Entrypoint::Io_progress_handler
	 */
	void handle_io_progress() override
	{
		genode_uplink_notify_peers();
	}

	void _handle_signal()
	{
		lx_user_handle_io();
		Lx_kit::env().scheduler.schedule();
	}

	Main(Env &env) : _env { env }
	{
		Lx_kit::initialize(env);

		genode_uplink_init(genode_env_ptr(_env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(_signal_handler));

		lx_emul_start_kernel(nullptr);

		env.ep().register_io_progress_handler(*this);
	}
};


void Component::construct(Env &env)
{
	static Main main(env);
}

/*
 * \brief  i.MX8 USB-card driver Linux port
 * \author Stefan Kalkowski
 * \date   2021-06-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>

#include <lx_emul/init.h>
#include <lx_emul/page_virt.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>
#include <lx_user/io.h>


extern "C" void lx_user_init(void) { }
extern "C" void lx_user_handle_io(void) { }


using namespace Genode;


struct Main : private Entrypoint::Io_progress_handler
{
	Env                  & env;
	Signal_handler<Main>   signal_handler { env.ep(), *this,
	                                        &Main::handle_signal };
	Sliced_heap            sliced_heap    { env.ram(), env.rm()  };

	Attached_rom_dataspace config_rom { env, "config" };

	/**
	 * Entrypoint::Io_progress_handler
	 */
	void handle_io_progress() override
	{
	}

	void handle_signal()
	{
		lx_user_handle_io();
		Lx_kit::env().scheduler.schedule();
	}

	Main(Env & env) : env(env)
	{
		Lx_kit::initialize(env);

		lx_emul_start_kernel(nullptr);

		env.ep().register_io_progress_handler(*this);
	}
};


void Component::construct(Env & env)
{
	static Main main(env);
}

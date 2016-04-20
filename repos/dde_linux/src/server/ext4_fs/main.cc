/*
 * \brief  vfs_ext4 Link test
 * \author Josef Soentgen
 * \date   2016-04-26
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <os/server.h>


extern bool block_init(Server::Entrypoint &, Genode::Allocator &);
extern bool fs_init(Server::Entrypoint &, Genode::Signal_transmitter &);


struct Main
{
	Server::Entrypoint &ep { ep };

	void handle_mounted(unsigned)
	{
		PINF("--- File system mounted successfully ---");
	}

	Genode::Signal_rpc_member<Main> mounted_dispatcher {
		ep, *this, &Main::handle_mounted };

	Genode::Signal_transmitter signal_sender;

	Main(Server::Entrypoint &ep)
	{
		if (!block_init(ep, *Genode::env()->heap())) {
			PERR("Could not open block session");
			return;
		}

		signal_sender.context(mounted_dispatcher);

		fs_init(ep, signal_sender);
	}
};


namespace Server {
	char const *name()             { return "ext4_fs_ep";        }
	size_t stack_size()            { return 8*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}

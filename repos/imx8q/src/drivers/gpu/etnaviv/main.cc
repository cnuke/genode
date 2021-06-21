/*
 * \brief  i.MX8 framebuffer driver Linux port
 * \author Stefan Kalkowski
 * \date   2021-03-08
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/component.h>
#include <lx_kit/init.h>

namespace Framebuffer {
	using namespace Genode;
	struct Main;
}

extern "C" void dump_quota(int, int);

struct Framebuffer::Main
{
	Env & env;

	Heap  heap { env.ram(), env.rm() };

	Main(Env & env) : env(env)
	{
		Lx_kit::initialize(env, heap);
		dump_quota(0, 0);
	}
};


void Component::construct(Genode::Env &env)
{
	static Framebuffer::Main main(env);
}

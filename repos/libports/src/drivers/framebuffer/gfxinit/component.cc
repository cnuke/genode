/*
 * \brief  Genode gfxinit driver
 * \author Josef Soentgen
 * \date   2017-10-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <timer_session/connection.h>

/**
 * Make the linker happy
 */
extern "C" void __gnat_eh_personality()
{
	Genode::warning(__func__, " not implemented");
}

namespace Libhwbase {
	void init(Genode::Env&);
}


extern "C" void hw__gfx__gma__gfx__main();


void Component::construct(Genode::Env &env)
{
	/* FIXME */
	env.exec_static_constructors();

	Libhwbase::init(env);

	hw__gfx__gma__gfx__main();

	env.parent().exit(0);
}

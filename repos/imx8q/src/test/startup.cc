/**
 * \brief  DRM tests startup code
 * \author Josef Soentgen
 * \date   2021-04-13
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <libc/args.h>
#include <libc/component.h>

#include <stdlib.h> /* 'exit'   */

/*
 * Override the libc implementation to allow for opening
 * "/dev/dri/<device>".
 */
extern "C" int open(char const *pathname, int flags, ...)
{
	(void)flags;

	if (Genode::strcmp("/dev/dri/render0", pathname) != 0) {
		return -1;
	}

	Genode::log("Override open()");
	return 0x42;
}

extern void drm_init(Genode::Env &);

extern char **environ;

/* provided by the application */
extern "C" int main(int argc, char **argv, char **envp);

static void construct_component(Libc::Env &env)
{
	int argc    = 0;
	char **argv = nullptr;
	char **envp = nullptr;

	populate_args_and_env(env, argc, argv, envp);

	environ = envp;

	exit(main(argc, argv, envp));
}


void Libc::Component::construct(Libc::Env &env)
{
	drm_init(env);

	Libc::with_libc([&] () { construct_component(env); });
}

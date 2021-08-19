/**
 * \brief  Initialize DRM libraries session interface
 * \author Josef Soentgen
 * \date   2021-04-30
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <../include/util/list.h>
#include <base/env.h>

extern "C" {
#include <platform.h>
}

extern Genode::Entrypoint &genode_entrypoint();
extern void drm_init(Genode::Env &env, Genode::Entrypoint &ep, bool use_gpu_session);

void genode_drm_init(int use_gpu_session)
{
	drm_init(*genode_env, genode_entrypoint(), use_gpu_session);
}

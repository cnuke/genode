/*
 * \brief  Testing 'Genode::log()' and LOG session
 * \author Christian Prochaska
 * \date   2012-04-20
 *
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <timer_session/connection.h>


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	Timer::Connection timer(env);
	timer.msleep(4000);
	log("Test done.");
	env.parent().exit(0);
}

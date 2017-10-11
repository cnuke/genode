/*
 * \brief  Genode HW backend glue implementation
 * \author Josef Soentgen
 * \date   2017-10-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <timer_session/connection.h>

#include <glue.h>


/*
 * Glue code for using Genode services from Ada
 */
struct Glue
{
	Genode::Env &_env;

	Timer::Connection _timer { _env };

	Glue(Genode::Env &env) : _env(env) { }

	unsigned long timer_now() const
	{
		return (unsigned long)_timer.elapsed_ms();
	}
};


static Genode::Constructible<Glue> _glue;

/**
 * HW.Time.Timer backend implementation
 */
extern "C" unsigned long genode_timer_now()
{
	if (!_glue.constructed()) { return 0; }

	return _glue->timer_now();
}


/**
 * HW.Debug_Sink backend implementation
 */
extern "C" void genode_put(char const *string)
{
	Genode::log(string);
}


void Libhwbase::init(Genode::Env &env)
{
	_glue.construct(env);
}

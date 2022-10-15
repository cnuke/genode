/*
 * \brief  Timer accuracy test
 * \author Norman Feske
 * \author Martin Stein
 * \date   2010-03-09
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/component.h>

using namespace Genode;

#if 0
struct Main
{
	Timer::Connection    timer;
	Signal_handler<Main> timer_handler;
	uint64_t             duration_us { 0 };

	void handle_timer()
	{
		duration_us += 1000 * 1000;
		timer.trigger_once(duration_us);
		log("");
	}

	Main(Env &env) : timer(env), timer_handler(env.ep(), *this, &Main::handle_timer)
	{
		timer.sigh(timer_handler);
		handle_timer();
	}
};
#endif

void Component::construct(Env &env)
{
//	static Main main(env);
	for (;;) {

#if 0
		static Genode::Trace::Timestamp last_ts = Genode::Trace::timestamp();
		Genode::Trace::Timestamp ts = Genode::Trace::timestamp();
		if (ts >= last_ts + 816000000UL) {
			Genode::log(ts);
			last_ts = ts;
		}
#endif
		static Timer::Connection timer(env);
		timer.msleep(1000);
		Genode::Trace::Timestamp ts = Genode::Trace::timestamp();
		Genode::log(ts);
	}
}

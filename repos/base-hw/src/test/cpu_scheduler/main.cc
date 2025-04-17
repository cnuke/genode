/*
 * \brief  Unit-test scheduler implementation of the kernel
 * \author Stefan Kalkowski
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>

/* core includes */
#include <kernel/scheduler.h>

using namespace Genode;

namespace Scheduler_test { struct Main; }


struct Scheduler_test::Main
{
	using time_t = Kernel::time_t;
	using Scheduler = Kernel::Scheduler;

	struct Context : Scheduler::Context
	{
		using Label = String<32>;

		Label const _label;

		Context(Kernel::Scheduler::Group_id const id,
		        Label const &label)
		:
			Kernel::Scheduler::Context { id },
			_label { label }
		{ }

		Label const &label() const { return _label; }
	};

	Env &env;

	enum Id {
		IDLE,
		DRV1, DRV2, DRV3,
		MUL1, MUL2, MUL3,
		APP1, APP2, APP3,
		BCK1, BCK2, BCK3,
		MAX = BCK3
	};

	using Gids = Scheduler::Group_id::Ids;

	Context contexts[MAX+1] {
		[IDLE] = { Gids::INVALID,    "idle"        },
		[DRV1] = { Gids::DRIVER,     "driver1"     },
		[DRV2] = { Gids::DRIVER,     "driver2"     },
		[DRV3] = { Gids::DRIVER,     "driver3"     },
		[MUL1] = { Gids::MULTIMEDIA, "multimedia1" },
		[MUL2] = { Gids::MULTIMEDIA, "multimedia2" },
		[MUL3] = { Gids::MULTIMEDIA, "multimedia3" },
		[APP1] = { Gids::APP,        "app1"        },
		[APP2] = { Gids::APP,        "app2"        },
		[APP3] = { Gids::APP,        "app3"        },
		[BCK1] = { Gids::BACKGROUND, "background1" },
		[BCK2] = { Gids::BACKGROUND, "background2" },
		[BCK3] = { Gids::BACKGROUND, "background3" }
	};

	Kernel::Timer timer {};
	Scheduler scheduler { timer, contexts[IDLE] };

	Context& current() {
		return static_cast<Context&>(scheduler.current()); }

	Context& next()
	{
		Context * ret = &current();
		scheduler._with_next([&] (Kernel::Scheduler::Context &next, time_t) {
			ret = &static_cast<Context&>(next); });
		return *ret;
	}

	Context::Label const &label(Kernel::Scheduler::Context &c) const {
		return static_cast<Context&>(c).label(); }

	void dump()
	{
		log("");
		log("Scheduler state:");
		unsigned i = 0;
		scheduler._for_each_group([&] (Scheduler::Group &group) {
			log("Group ", i++, " (weight=", group._weight,
			    ", warp=", group._warp,
			    ") has vtime: ", group._vtime,
			    " and min_vtime: ", group._min_vtime);

			using List_element = Genode::List_element<Scheduler::Context>;

			if (group._contexts.first()) log("  Contexts:");
			for (List_element * le = group._contexts.first(); le;
			     le = le->next()) {
			Scheduler::Context &c = *le->object();
				log("    ", label(c), " has vtime: ", c.vtime(),
				    " and real execution time: ", c.execution_time());
			}
		});

		log("Current context: ", current().label(),
		    " (group=", current()._id.value,
		    ") has vtime: ", current().vtime(),
		    " and real execution time: ", current().execution_time());
		log(" Next context: ", next().label());
	}

	void update_and_check(time_t   const consumed_abs_time,
	                      Id       const expected_current,
	                      Id       const expected_next,
	                      time_t   const expected_abs_timeout,
	                      unsigned const line_nr)
	{
		timer.set_time(consumed_abs_time);
		scheduler.update();

		if (&current() != &contexts[expected_current]) {
			error("wrong current context ", current().label(),
			      " in line ", line_nr);
			dump();
			env.parent().exit(-1);
		}

		if (&next() != &contexts[expected_next]) {
			error("wrong next context ", next().label(),
			      " in line ", line_nr);
			dump();
			env.parent().exit(-1);
		}

		if (timer._next_timeout != expected_abs_timeout) {
			error("expected timeout ", expected_abs_timeout,
			      " in line ", line_nr);
			error("But actual timeout is: ", timer._next_timeout);
			dump();
			env.parent().exit(-1);
		}
	}

	void test_background_idle();
	void test_one_per_group();
	void test_io_signal();
	void test_all_and_yield();

	Main(Env &env) : env(env) { }
};


void Scheduler_test::Main::test_background_idle()
{
	time_t MAX_TIME = scheduler._max_timeout;

	/* params:       time, curr, next,  timeout, line */
	update_and_check(   0, IDLE, IDLE,        0, __LINE__);
	scheduler.ready(contexts[BCK1]);
	update_and_check(   0, BCK1, IDLE, MAX_TIME, __LINE__);
	update_and_check(  10, BCK1, IDLE, MAX_TIME, __LINE__);
	update_and_check(   0, BCK1, IDLE, MAX_TIME, __LINE__);
	scheduler.ready(contexts[BCK2]);
	update_and_check(  10, BCK2, BCK1,      510, __LINE__);
	update_and_check( 510, BCK1, BCK2,     1011, __LINE__);
	update_and_check(1530, BCK2, BCK1,     2051, __LINE__);
	scheduler.ready(contexts[BCK3]);
	update_and_check(2000, BCK3, BCK2,     2500, __LINE__);
	update_and_check(2500, BCK2, BCK1,     3000, __LINE__);
	update_and_check(3000, BCK1, BCK2,     3500, __LINE__);
	scheduler.unready(contexts[BCK1]);
	update_and_check(3020, BCK2, BCK3,     3520, __LINE__);
	scheduler.unready(contexts[BCK2]);
	update_and_check(3040, BCK3, IDLE, MAX_TIME
	                                     + 3040, __LINE__);
	update_and_check(4000, BCK3, IDLE, MAX_TIME
	                                     + 4000, __LINE__);
}


void Scheduler_test::Main::test_one_per_group()
{
	scheduler.ready(contexts[BCK1]);
	scheduler.ready(contexts[APP1]);
	scheduler.ready(contexts[DRV1]);
	scheduler.ready(contexts[MUL1]);

	/* params:       time, curr, next,  timeout, line */
	update_and_check(   0, DRV1, MUL1,      500, __LINE__);
	update_and_check( 500, MUL1, DRV1,     1000, __LINE__);
	update_and_check(1000, DRV1, APP1,     1500, __LINE__);
	update_and_check(1500, APP1, MUL1,     2000, __LINE__);
	update_and_check(2000, MUL1, BCK1,     2500, __LINE__);
	update_and_check(2500, BCK1, DRV1,     3000, __LINE__);
	update_and_check(3000, DRV1, MUL1,     3500, __LINE__);
	update_and_check(3500, MUL1, APP1,     4000, __LINE__);
	update_and_check(4000, APP1, MUL1,     4500, __LINE__);
	update_and_check(4500, MUL1, DRV1,     5000, __LINE__);
	update_and_check(5000, DRV1, APP1,     5500, __LINE__);
	update_and_check(5500, APP1, MUL1,     6000, __LINE__);
	update_and_check(6000, MUL1, BCK1,     6500, __LINE__);
	update_and_check(6500, BCK1, DRV1,     7000, __LINE__);
	update_and_check(7000, DRV1, MUL1,     7500, __LINE__);
	update_and_check(7500, MUL1, APP1,     8000, __LINE__);
	update_and_check(8000, APP1, MUL1,     8500, __LINE__);
	update_and_check(8500, MUL1, DRV1,     9000, __LINE__);
	update_and_check(9000, DRV1, APP1,     9500, __LINE__);
	update_and_check(9500, APP1, MUL1,    10000, __LINE__);
}


void Scheduler_test::Main::test_io_signal()
{
	scheduler.ready(contexts[BCK1]);
	scheduler.ready(contexts[BCK2]);
	scheduler.ready(contexts[BCK3]);
	scheduler.ready(contexts[APP1]);

	/* params:       time, curr, next,  timeout, line */
	update_and_check(   0, APP1, BCK1,      500, __LINE__);
	update_and_check( 500, BCK1, APP1,     1000, __LINE__);
	update_and_check(1000, APP1, BCK2,     1702, __LINE__);
	update_and_check(1800, BCK2, APP1,     2300, __LINE__);
	scheduler.ready(contexts[DRV1]); /* irq occurred */
	update_and_check(1900, DRV1, APP1,     2602, __LINE__);
	scheduler.ready(contexts[MUL1]); /* signal occurred */
	scheduler.unready(contexts[DRV1]);
	update_and_check(2200, MUL1, APP1,     2700, __LINE__);
	scheduler.ready(contexts[APP2]); /* signal occurred */
	scheduler.unready(contexts[MUL1]);
	update_and_check(2500, APP2, BCK3,     3000, __LINE__);
	scheduler.unready(contexts[APP2]);
	update_and_check(2900, APP1, BCK3,     3400, __LINE__);
	update_and_check(3500, BCK3, APP1,     4000, __LINE__);
}


void Scheduler_test::Main::test_all_and_yield()
{
	scheduler.ready(contexts[BCK1]);
	scheduler.ready(contexts[BCK2]);
	scheduler.ready(contexts[BCK3]);
	scheduler.ready(contexts[APP1]);
	scheduler.ready(contexts[APP2]);
	scheduler.ready(contexts[APP3]);
	scheduler.ready(contexts[MUL1]);
	scheduler.ready(contexts[MUL2]);
	scheduler.ready(contexts[MUL3]);
	scheduler.ready(contexts[DRV1]);
	scheduler.ready(contexts[DRV2]);
	scheduler.ready(contexts[DRV3]);

	/* params:       time, curr, next,  timeout, line */
	update_and_check(   0, DRV1, MUL1,      500, __LINE__);
	update_and_check( 500, MUL1, DRV2,     1000, __LINE__);
	update_and_check(1000, DRV2, APP1,     1500, __LINE__);
	update_and_check(1500, APP1, MUL2,     2000, __LINE__);
	update_and_check(2000, MUL2, BCK1,     2500, __LINE__);
	update_and_check(2500, BCK1, DRV3,     3000, __LINE__);
	update_and_check(3000, DRV3, MUL3,     3500, __LINE__);
	update_and_check(3500, MUL3, APP2,     4000, __LINE__);
	update_and_check(4000, APP2, MUL3,     4500, __LINE__);
	update_and_check(4500, MUL3, DRV3,     5000, __LINE__);
	update_and_check(5000, DRV3, APP3,     5500, __LINE__);
	update_and_check(5500, APP3, MUL2,     6000, __LINE__);
	update_and_check(6000, MUL2, BCK2,     6500, __LINE__);
	update_and_check(6500, BCK2, DRV2,     7000, __LINE__);
	timer.set_time(6600);
	scheduler.yield();
	update_and_check(6600, BCK3, DRV2,     7100, __LINE__);
	timer.set_time(6700);
	scheduler.yield();
	update_and_check(6700, DRV2, MUL1,     7200, __LINE__);
	update_and_check(7500, MUL1, APP3,     8000, __LINE__);
	update_and_check(8000, APP3, BCK1,     8500, __LINE__);
	scheduler.yield();
	update_and_check(8000, APP2, BCK1,     8500, __LINE__);
	update_and_check(8500, BCK1, MUL1,     9000, __LINE__);
	update_and_check(9000, MUL1, APP1,     9500, __LINE__);
	update_and_check(9500, APP1, MUL2,    10000, __LINE__);
}

void Component::construct(Env &env)
{
	{
		Scheduler_test::Main main { env };
		main.test_background_idle();
	}

	{
		Scheduler_test::Main main { env };
		main.test_one_per_group();
	}

	{
		Scheduler_test::Main main { env };
		main.test_io_signal();
	}

	{
		Scheduler_test::Main main { env };
		main.test_all_and_yield();
	}


	env.parent().exit(0);
}

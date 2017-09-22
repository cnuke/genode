/*
 * \brief  RTC server
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <root/component.h>

#include "rtc.h"


namespace Rtc {
	using namespace Genode;

	struct Session_component;
	struct Root;
	struct Main;
}


struct Rtc::Session_component : public Genode::Rpc_object<Session>
{
	Env &_env;

	Timestamp current_time() override
	{
		Timestamp ret = Rtc::get_time(_env);

		return ret;
	}

	Session_component(Env &env) : _env(env) { }
};


class Rtc::Root : public Genode::Root_component<Session_component>
{
	private:

		Env &_env;

	protected:

		Session_component *_create_session(const char *args)
		{
			return new (md_alloc()) Session_component(_env);
		}

	public:

		Root(Env &env, Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			/* trigger initial RTC read */
			Rtc::get_time(_env);
		}
};


struct Rtc::Main
{
	Env &env;

	Attached_rom_dataspace config_rom { env, "config" };

	void handle_config_update()
	{
		config_rom.update();

		if (!config_rom.valid()) { return; }

		try {
			Xml_node const node = config_rom.xml().sub_node("time");

			Timestamp const curr = Rtc::get_time(env);
			Timestamp ts;

			ts.second = node.attribute_value("second", curr.second);
			if (ts.second > 59) {
				Genode::error("second invalid");
				return;
			}

			ts.minute = node.attribute_value("minute", curr.minute);
			if (ts.minute > 59) {
				Genode::error("minute invalid");
				return;
			}

			ts.hour = node.attribute_value("hour", curr.hour);
			if (ts.hour > 23) {
				Genode::error("hour invalid");
				return;
			}

			ts.day = node.attribute_value("day", curr.day);
			if (ts.day > 31 || ts.day == 0) {
				Genode::error("day invalid");
				return;
			}

			ts.month = node.attribute_value("month", curr.month);
			if (ts.month > 12 || ts.month == 0) {
				Genode::error("month invalid");
				return;
			}

			ts.year = node.attribute_value("year", curr.year);

			Rtc::set_time(env, ts);
		} catch (Xml_node::Nonexistent_sub_node) { }
	}

	Signal_handler<Main> config_sigh {
		env.ep(), *this, &Main::handle_config_update };

	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root root { env, sliced_heap };

	Main(Env &env) : env(env)
	{
		config_rom.sigh(config_sigh);

		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Rtc::Main main(env); }

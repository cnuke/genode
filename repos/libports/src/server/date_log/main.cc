/*
 * \brief  LOG server that forwards messages to another LOG server
 * \author Josef Soentgen
 * \date   2019-03-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <log_session/log_session.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>

/* local includes */
#include <tm.h>


namespace Log_log {

	class Session_component;
	class Root_component;
	class Main;
} /* namespace Log_log */


class Log_log::Session_component : public Genode::Rpc_object<Genode::Log_session>
{
	private:

		Genode::Env       &_env;
		Timer::Connection &_timer;
		long long          _time;

		Genode::Session_label const _label;

		char _string_buffer[Genode::Log_session::MAX_STRING_LEN];


	public:

		Session_component(Genode::Env &env, Timer::Connection &timer,
		                  long long time, Genode::Session_label const &label)
		: _env(env), _timer(timer), _time(time), _label(label) { }

		/***************************
		 ** Log_session interface **
		 ***************************/

		Genode::size_t write(String const &string) override
		{
			if (!string.valid_string()) {
				Genode::error("invalid string");
				return 0;
			}

			Genode::size_t string_len = Genode::strlen(string.string());
			/* assert string_len <= sizeof (_string_buffer) */
			Genode::memcpy(_string_buffer, string.string(), string_len);

			_string_buffer[string_len] = 0;

			/* prevent double new line */
			if (_string_buffer[string_len-1] == '\n') {
				_string_buffer[string_len-1] = 0;
			}

			long long const curr_time = _time + ((long long)_timer.elapsed_ms() / 1000);

			struct tm tm;
			Genode::memset(&tm, 0, sizeof(tm));

			int const err = secs_to_tm((long long)curr_time, &tm);
			if (err) { Genode::warning("could not convert timestamp"); }

			using Time = Genode::String<128>;
			Time time { tm.tm_year + 1900LL, "-", tm.tm_mon + 1, "-", tm.tm_mday, " ",
			            tm.tm_hour, ":", tm.tm_min, ":", tm.tm_sec };

			Genode::log(time, " [", _label, "] ", (char const*)_string_buffer);

			return string_len;
		}
};


class Log_log::Root_component : public Genode::Root_component<Log_log::Session_component>
{
	private:

		Genode::Env &_env;

		Timer::Connection _timer { _env };
		Rtc::Connection   _rtc   { _env };

		long long _convert_rtc()
		{
			Rtc::Timestamp const ts = _rtc.current_time();

			struct tm tm;
			Genode::memset(&tm, 0, sizeof(struct tm));
			tm.tm_sec  = ts.second;
			tm.tm_min  = ts.minute;
			tm.tm_hour = ts.hour;
			tm.tm_mday = ts.day;
			tm.tm_mon  = ts.month - 1;
			tm.tm_year = ts.year - 1900;

			return tm_to_secs(&tm);
		}

		long long _rtc_time { _convert_rtc() };


	protected:

		Session_component *_create_session(char const *args) override
		{
			using namespace Genode;

			Session_label const label { label_from_args(args) };

			return new (md_alloc()) Session_component(_env, _timer, _rtc_time, label);
		}

	public:

		Root_component(Genode::Env       &env,
		               Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env) { }
};


class Log_log::Main
{
	private:

		Genode::Env &_env;

		Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

		Log_log::Root_component _root { _env, _sliced_heap };

	public:

	Main(Genode::Env &env) : _env(env)
	{
		using namespace Genode;
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Log_log::Main main { env };
}

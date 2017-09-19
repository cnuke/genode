/*
 * \brief  Clock
 * \author Josef Soentgen
 * \date   2017-09-19
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* local include */
#include <tm.h>

namespace Clock {

	using uint32_t = Genode::uint32_t;
	using uint64_t = Genode::uint64_t;

	struct Main;
}


struct Clock::Main
{
	Genode::Env &_env;

	/*******************
	 ** Time handling **
	 *******************/

	Genode::Reporter _reporter { _env, "dialog" };

	enum { DEFAULT_INTERVAL = 5, };
	int      _utc_offset  { 0 };
	uint64_t _time_offset { 0 };
	bool     _show_date   { false };

	Timer::Connection _timer { _env };

	void _handle_timeout()
	{
		uint64_t curr_time  = _time_offset + _utc_offset;
		         curr_time += (_timer.elapsed_ms() / 1000);

		try {
			char time[128];

			if (!_show_date) {
				uint32_t const seconds = curr_time % 60; curr_time /= 60;
				uint32_t const minutes = curr_time % 60; curr_time /= 60;
				uint32_t const hours   = curr_time % 24; curr_time /= 24;
				(void)seconds;
				Genode::snprintf(time, sizeof(time), "%02u:%02u", hours, minutes);
			} else {
				tm tm;
				Genode::memset(&tm, 0, sizeof(tm));

				int const err = secs_to_tm((long long)curr_time, &tm);
				if (err) { Genode::warning("could not convert timestamp"); }

				Genode::snprintf(time, sizeof(time), "%02u:%02u  %lld-%02u-%02u",
				                 tm.tm_hour, tm.tm_min, tm.tm_year + 1900LL,
				                 tm.tm_mon + 1, tm.tm_mday);
			}

			Genode::Reporter::Xml_generator xml(_reporter, [&] () {
				xml.node("label", [&] () {
					xml.attribute("text", time);
					xml.attribute("color", "#ffffff");
				});
			});
		} catch (...) { Genode::warning("could not report time"); }
	}

	Genode::Signal_handler<Main> _timeout_sigh {
		_env.ep(), *this, &Main::_handle_timeout };

	/*********************
	 ** Config handling **
	 *********************/

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	void _handle_config_update()
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		Genode::Xml_node const config = _config_rom.xml();

		_show_date = config.attribute_value<bool>("date", _show_date);

		_time_offset = config.attribute_value<uint64_t>("offset", _time_offset);

		_utc_offset = config.attribute_value<long>("utc_offset", _utc_offset);
		if (_utc_offset < -12 || _utc_offset > 14) {
			Genode::warning("UTC offset ", _utc_offset, " out of range, reset to 0");
			_utc_offset = 0;
		}
		_utc_offset *= 60*60;

		uint64_t timeout = config.attribute_value<uint64_t>("interval", DEFAULT_INTERVAL);
		if (timeout < 1 || timeout > 60) {
			Genode::warning("interval ", timeout, " out of range, reset to ",
			                (int)DEFAULT_INTERVAL);
			timeout = DEFAULT_INTERVAL;
		}
		timeout *= 1000000;

		_timer.trigger_periodic(timeout);
	}

	Genode::Signal_handler<Main> _config_sigh {
		_env.ep(), *this, &Main::_handle_config_update };

	/**********
	 ** Main **
	 **********/

	Main(Genode::Env &env) : _env(env)
	{
		_reporter.enabled(true);

		_config_rom.sigh(_config_sigh);
		_timer.sigh(_timeout_sigh);

		/* initial config update */
		_handle_config_update();
	}
};


void Component::construct(Genode::Env &env) { static Clock::Main main(env); }

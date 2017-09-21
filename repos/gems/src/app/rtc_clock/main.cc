/*
 * \brief  RTC Clock
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
#include <rtc_session/connection.h>

/* local include */
#include <tm.h>

namespace Rtc_clock {

	struct Main;
}


struct Rtc_clock::Main
{
	Genode::Env                    &_env;
	Rtc::Connection                 _rtc        { _env };
	Genode::Reporter                _reporter   { _env, "config" };
	Genode::Attached_rom_dataspace  _config_rom { _env, "config" };

	void _handle_config_update()
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		Rtc::Timestamp const ts = _rtc.current_time();

		struct tm tm;
		Genode::memset(&tm, 0, sizeof(struct tm));
		tm.tm_sec  = ts.second;
		tm.tm_min  = ts.minute;
		tm.tm_hour = ts.hour;
		tm.tm_mday = ts.day;
		tm.tm_mon  = ts.month - 1;
		tm.tm_year = ts.year - 1900;

		long long const offset = tm_to_secs(&tm);

		try {
			Genode::Reporter::Xml_generator xml(_reporter, [&] () {
				xml.attribute("offset", offset);
				xml.attribute("utc_offset", 2);
				xml.attribute("interval", 5);
				xml.attribute("date", true);
			});
		} catch (...) { Genode::warning("could not generate config"); }
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

		/* initial config update */
		_handle_config_update();
	}
};


void Component::construct(Genode::Env &env) { static Rtc_clock::Main m(env); }

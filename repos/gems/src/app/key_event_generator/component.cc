/*
 * \brief  Key event generator
 * \author Josef Soentgen
 * \date   2024-06-21
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <event_session/connection.h>
#include <input/event.h>

namespace Key_event_generator {
	using namespace Genode;
	struct Main;
}


struct Key_event_generator::Main
{
	Env &_env;

	Input::Keycode _keycode = Input::KEY_UNKNOWN;

	Event::Connection _event { _env };

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	void _generate()
	{
		Genode::log("Generate key event for code ", Input::key_name(_keycode));

		_event.with_batch([&] (Event::Connection::Batch &batch) {
			batch.submit(Input::Press   { _keycode });
			batch.submit(Input::Release { _keycode });
		});
	}

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		config.with_optional_sub_node("event",
			[&] (Xml_node const event) {
			Input::Key_name const key_name =
					event.attribute_value("key", Input::Key_name("KEY_UNKNOWN"));

				_keycode = Input::key_code(key_name);
			});

		if (_keycode == Input::KEY_UNKNOWN)
			return;

		_generate();
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env)
{
	static Key_event_generator::Main main(env);
}

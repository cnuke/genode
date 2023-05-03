/*
 * \brief  Startup Wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>

/* local includes */
#include <util.h>
#include <wpa.h>
#include <frontend.h>


using namespace Genode;

static Msg_buffer      _wifi_msg_buffer;
static Wifi::Frontend *_wifi_frontend = nullptr;


/**
 * Notify front end about command processing
 *
 * Called by the CTRL interface after wpa_supplicant has processed
 * the command.
 */
void wifi_block_for_processing(void)
{
	if (!_wifi_frontend) {
		warning("frontend not available, dropping notification");
		return;
	}

	/*
	 * Next time we block as long as the front end has not finished
	 * handling our previous request
	 */
	_wifi_frontend->block_for_processing();

	/* XXX hack to trick poll() into returning faster */
	wpa_ctrl_set_fd();
}


void wifi_notify_cmd_result(void)
{
	if (!_wifi_frontend) {
		warning("frontend not available, dropping notification");
		return;
	}

	Signal_transmitter(_wifi_frontend->result_sigh()).submit();
}


/**
 * Notify front end about triggered event
 *
 * Called by the CTRL interface whenever wpa_supplicant has triggered
 * a event.
 */
void wifi_notify_event(void)
{
	if (!_wifi_frontend) {
		Genode::warning("frontend not available, dropping notification");
		return;
	}

	Signal_transmitter(_wifi_frontend->event_sigh()).submit();
}


/* exported by wifi.lib.so */
extern void wifi_init(Genode::Env&, Genode::Blockade&);
extern void wifi_set_rfkill_sigh(Genode::Signal_context_capability);

#include <wifi/firmware_access.h>
#include "access_firmware.h"

extern void wifi_firmware_request_sigh(Genode::Signal_context_capability);
extern void wifi_firmware_response_notification();
extern Wifi::Firmware_request *wifi_firmware_get_request();

struct Main
{
	Env  &env;

	Constructible<Wpa_thread>     _wpa;
	Constructible<Wifi::Frontend> _frontend;

	Blockade _wpa_startup_blockade { };

	Signal_handler<Main> _firmware_request_sigh {
		env.ep(), *this, &Main::_handle_firmware_request };

	void _handle_firmware_request()
	{
		using Fw_path = Genode::String<128>;
		using namespace Wifi;

		Firmware_request *request_ptr = wifi_firmware_get_request();
		if (!request_ptr)
			return;

		Firmware_request &request = *request_ptr;

		request.success = false;

		switch (request.state) {
		case Firmware_request::State::PROBING:
		{
			Fw_path const path { "/firmware/", request.name };

			Stat_firmware_result const result = access_firmware(path.string());

			request.fw_len = result.success ? result.length : 0;
			request.success = result.success;
			request.state = Firmware_request::State::PROBING_COMPLETE;

			wifi_firmware_response_notification();
			break;
		}
		case Firmware_request::State::REQUESTING:
		{
			Fw_path const path { "/firmware/", request.name };

			Read_firmware_result const result =
				read_firmware(path.string(), request.dst, request.dst_len);

			request.success = result.success;
			request.state = Firmware_request::State::REQUESTING_COMPLETE;

			wifi_firmware_response_notification();
			break;
		}
		case Firmware_request::State::INVALID:
			break;
		case Firmware_request::State::PROBING_COMPLETE:
			break;
		case Firmware_request::State::REQUESTING_COMPLETE:
			break;
		}
	}

	Main(Genode::Env &env) : env(env)
	{
		_frontend.construct(env, _wifi_msg_buffer);
		_wifi_frontend = &*_frontend;
		wifi_set_rfkill_sigh(_wifi_frontend->rfkill_sigh());

		wifi_firmware_request_sigh(_firmware_request_sigh);

		_wpa.construct(env, _wpa_startup_blockade);

		wifi_init(env, _wpa_startup_blockade);
	}
};


/**
 * Return shared-memory message buffer
 *
 * It is used by the wpa_supplicant CTRL interface.
 */
void *wifi_get_buffer(void)
{
	return &_wifi_msg_buffer;
}


void Libc::Component::construct(Libc::Env &env)
{
	static Main server(env);
}

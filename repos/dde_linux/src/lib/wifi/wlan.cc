/*
 * \brief  Wireless network driver Linux port
 * \author Josef Soentgen
 * \date   2022-02-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <os/reporter.h>
#include <net/mac_address.h>
#include <genode_c_api/uplink.h>

/* DDE Linux includes */
#include <lx_emul/init.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/task.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>
#include <lx_user/io.h>

/* local includes */
#include "lx_user.h"
#include "dtb_helper.h"

using namespace Genode;


extern "C" int  lx_emul_rfkill_get_any(void);
extern "C" void lx_emul_rfkill_switch_all(int blocked);

static Signal_context_capability _rfkill_sigh_cap;


bool _wifi_get_rfkill(void)
{
	/*
	 * It is safe to call this from non EP threads as we
	 * only query a variable.
	 */
	return lx_emul_rfkill_get_any();
}


void _wifi_set_rfkill(bool blocked)
{
	if (!rfkill_task_struct_ptr)
		return;

	lx_emul_rfkill_switch_all(blocked);

	lx_emul_task_unblock(rfkill_task_struct_ptr);
	Lx_kit::env().scheduler.schedule();

	/*
	 * We have to open the device again after unblocking
	 * as otherwise we will get ENETDOWN. So unblock the uplink
	 * task _afterwards_ because there we call * 'dev_open()'
	 * unconditionally and that will bring the netdevice UP again.
	 */
	lx_emul_task_unblock(uplink_task_struct_ptr);
	Lx_kit::env().scheduler.schedule();

	Signal_transmitter(_rfkill_sigh_cap).submit();
}


bool wifi_get_rfkill(void)
{
	return _wifi_get_rfkill();
}

/* Firmware access, move to object later */

#include <wifi/firmware_access.h>

struct task_struct;

struct Firmware_access_helper
{
	Firmware_access_helper(Firmware_access_helper const&) = delete;
	Firmware_access_helper & operator = (Firmware_access_helper const&) = delete;

	Genode::Signal_handler<Firmware_access_helper> _response_handler;

	Genode::Signal_context_capability _request_sigh;

	Wifi::Firmware_request _request { };

	void *calling_task { nullptr };

	void _handle_response()
	{
		if (calling_task)
			lx_emul_task_unblock((struct task_struct*)calling_task);

		Lx_kit::env().scheduler.schedule();
	}

	Firmware_access_helper(Genode::Entrypoint &ep,
	                       Genode::Signal_context_capability request_sigh)
	:
		_response_handler { ep, *this, &Firmware_access_helper::_handle_response },
		_request_sigh { request_sigh }
	{ }

	void submit_response()
	{
		Genode::Signal_transmitter(_response_handler).submit();
	}

	void submit_request()
	{
		Genode::Signal_transmitter(_request_sigh).submit();
	}

	Wifi::Firmware_request *request()
	{
		return &_request;
	}
};

Constructible<Firmware_access_helper> firmware_access_helper { };


size_t _wifi_probe_firmware(char const *name)
{
	using namespace Wifi;

	Firmware_request &request = *firmware_access_helper->request();

	if (request.state != Firmware_request::State::INVALID) {
		error(__func__, ": cannot probe '", name, "' state: ",
		      (unsigned)request.state);
		return 0;
	}

	request.name    = name;
	request.state   = Firmware_request::State::PROBING;
	request.dst     = nullptr;
	request.dst_len = 0;

	firmware_access_helper->calling_task = lx_emul_task_get_current();
	firmware_access_helper->submit_request();

	do {
		lx_emul_task_schedule(true);
	} while (request.state != Firmware_request::State::PROBING_COMPLETE);

	request.state = Firmware_request::State::INVALID;
	firmware_access_helper->calling_task = nullptr;

	return request.fw_len;
}


int _wifi_request_firmware(char const *name, char *dst, size_t dst_len)
{
	using namespace Wifi;

	Firmware_request &request = *firmware_access_helper->request();

	if (request.state != Firmware_request::State::INVALID) {
		error(__func__, ": cannot request '", name, "' state: ",
		      (unsigned)request.state);
		return -1;
	}

	if (strcmp(request.name, name) != 0) {
		error(__func__, ": cannot request '", name, "' name does not match");
		return -1;
	}

	if (request.fw_len != dst_len) {
		error(__func__, ": cannot request '", name, "' length does not match");
		return -1;
	}

	request.state   = Firmware_request::State::REQUESTING;
	request.dst     = dst;
	request.dst_len = dst_len;

	firmware_access_helper->calling_task = lx_emul_task_get_current();
	firmware_access_helper->submit_request();

	do {
		lx_emul_task_schedule(true);
	} while (request.state != Firmware_request::State::REQUESTING_COMPLETE);

	request.state        = Firmware_request::State::INVALID;
	firmware_access_helper->calling_task = nullptr;

	return 0;
}


extern "C" unsigned int wifi_ifindex(void)
{
	/* TODO replace with actual qyery */
	return 2;
}


extern "C" char const *wifi_ifname(void)
{
	/* TODO replace with actual qyery */
	return "wlan0";
}


struct Mac_address_reporter
{
	bool _enabled = false;
 
	Net::Mac_address _mac_address { };

	Constructible<Reporter> _reporter { };

	Env &_env;

	Signal_context_capability _sigh;

	Mac_address_reporter(Env &env, Signal_context_capability sigh)
	: _env(env), _sigh(sigh)
	{
		Attached_rom_dataspace config { _env, "config" };

		config.xml().with_optional_sub_node("report", [&] (Xml_node const &xml) {
			_enabled = xml.attribute_value("mac_address", false); });
	}

	void mac_address(Net::Mac_address const &mac_address)
	{
		_mac_address = mac_address;

		Signal_transmitter(_sigh).submit();
	}

	void report()
	{
		if (!_enabled)
			return;

		_reporter.construct(_env, "devices");
		_reporter->enabled(true);

		Reporter::Xml_generator report(*_reporter, [&] () {
			report.node("nic", [&] () {
				report.attribute("mac_address", String<32>(_mac_address));
			});
		});

		/* report only once */
		_enabled = false;
	}
};

Constructible<Mac_address_reporter> mac_address_reporter;


/* used from socket_call.cc */
void _wifi_report_mac_address(Net::Mac_address const &mac_address)
{
	mac_address_reporter->mac_address(mac_address);
}


struct Wlan
{
	Env                    &_env;
	Io_signal_handler<Wlan> _signal_handler { _env.ep(), *this,
	                                          &Wlan::_handle_signal };

	Dtb_helper _dtb_helper { _env };

	void _handle_signal()
	{
		if (uplink_task_struct_ptr) {
			lx_emul_task_unblock(uplink_task_struct_ptr);
			Lx_kit::env().scheduler.schedule();
		}

		genode_uplink_notify_peers();

		mac_address_reporter->report();
	}

	Wlan(Env &env) : _env { env }
	{
		mac_address_reporter.construct(_env, _signal_handler);

		genode_uplink_init(genode_env_ptr(_env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(_signal_handler));

		lx_emul_start_kernel(_dtb_helper.dtb_ptr());
	}
};


static Blockade *wpa_blockade;


extern "C" void wakeup_wpa()
{
	static bool called_once = false;
	if (called_once)
		return;

	wpa_blockade->wakeup();
	called_once = true;
}


void wifi_init(Env &env, Blockade &blockade)
{
	wpa_blockade = &blockade;

	static Wlan wlan(env);
}


void wifi_set_rfkill_sigh(Signal_context_capability cap)
{
	_rfkill_sigh_cap = cap;
}


void wifi_firmware_request_sigh(Genode::Signal_context_capability sig_cap)
{
	firmware_access_helper.construct(Lx_kit::env().env.ep(), sig_cap);
}


void wifi_firmware_response_notification()
{
	if (firmware_access_helper.constructed())
		firmware_access_helper->submit_response();
}


Wifi::Firmware_request *wifi_firmware_get_request()
{
	if (firmware_access_helper.constructed())
		return firmware_access_helper->request();

	return nullptr;
}

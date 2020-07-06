/*
 * \brief  OpenBSD USB subsystem API emulation
 * \author Josef Soentgen
 * \date   2020-06-19
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator.h>
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <usb_session/connection.h>

/* local includes */
#include <bsd.h>
#include <bsd_emul.h>
#include <scheduler.h>
#include <dev/usb/usb.h>


extern "C" int probe_cfdata(struct usb_attach_arg *);


namespace {

using Label = Genode::String<64>;


struct Usb_report_handler
{
	Genode::Attached_rom_dataspace _report_rom;

	Usb_report_handler(Genode::Env &env,
	                   Genode::Signal_context_capability sigh)
	:
		_report_rom { env, "report" }
	{
		_report_rom.sigh(sigh);
	}

	Label process_report()
	{
		_report_rom.update();

		using namespace Genode;

		Label device_label { };

		Xml_node report_node = _report_rom.xml();
		report_node.for_each_sub_node([&] (Xml_node &dev_node) {

			unsigned const c = dev_node.attribute_value("class", 0u);
			if (c != UICLASS_AUDIO) {
				return;
			}

			device_label = dev_node.attribute_value("label", Label());
		});

		return device_label;
	}
};


struct Usb_driver
{
	struct Could_not_read_config_descriptor : Genode::Exception { };
	struct Device_already_claimed           : Genode::Exception { };

	Genode::Env       &_env;
	Genode::Allocator &_alloc;

	Genode::Signal_handler<Usb_driver> _usb_state_change_sigh;

	int  _found  { 0 };
	bool _probed { false };

	bool _plugged { false };

	void _handle_usb_state_change()
	{
		// XXX handling to BSD task
		if (_usb.plugged()) {
			Genode::log("device plugged in");
			// _found = probe();
		} else {
			Genode::log("device unplugged");
		}

		// Bsd::scheduler().schedule();
	}

	Genode::Allocator_avl _usb_alloc;
	Usb::Connection       _usb;

	struct Device
	{
		Usb::Device_descriptor dev_descr;
		Usb::Config_descriptor config_descr;
	};

	Device _usb_device { };

	struct usb_attach_arg           _ua              { NULL, NULL };
	struct usbd_device              _usbd_device     { NULL, 0 };
	struct usbd_interface           _usbd_iface      { NULL };
	struct usb_interface_descriptor _usb_iface_descr  { };
	struct usb_config_descriptor    _usb_config_descr { };

	char _config_descr_buffer[4096] { };

	u_int8_t _get_speed(Usb::Device_descriptor const &descr)
	{
		switch (descr.speed) {
		case 1: /* SPEED_LOW      */ return USB_SPEED_LOW;
		case 2: /* SPEED_FULL     */ return USB_SPEED_FULL;
		case 3: /* SPEED_HIGH     */ return USB_SPEED_HIGH;
		case 5: /* SPEED_SUPER    */ return USB_SPEED_SUPER;
		case 0: /* SPEED_UNKNOWN  */ [[fallthrough]];
		case 4: /* SPEED_WIRELESS */ [[fallthrough]];
		default:                     return 0;
		}
	}

	Usb_driver(Genode::Env       &env,
	           Genode::Allocator &alloc,
	           Label              label)
	:
		_env { env },
		_alloc { alloc },
		_usb_state_change_sigh { _env.ep(), *this,
		                         &Usb_driver::_handle_usb_state_change },
		_usb_alloc { &_alloc },
		_usb { _env, &_usb_alloc, label.string(), 256 * 1024,
		       _usb_state_change_sigh }
	{ }

	int probe()
	{
		try {
			_usb.config_descriptor(&_usb_device.dev_descr,
			                       &_usb_device.config_descr);
		} catch (Usb::Session::Device_not_found) {
			Genode::error("cound not read config descriptor");
			throw Could_not_read_config_descriptor();
		}

		// XXX only claim audio related interfaces
		bool already_claimed = false;
		for (unsigned i = 0; i < _usb_device.config_descr.num_interfaces; i++) {
			try {
				_usb.claim_interface(i);
			} catch (Usb::Session::Interface_already_claimed) {
				already_claimed = true;
				break;
			}
		}

		if (already_claimed) {
			Genode::error("device already claimed");
			throw Device_already_claimed();
		}

		_usbd_device.speed = _get_speed(_usb_device.dev_descr);
		_usbd_device.genode_usb_device = this;
		_usbd_iface.genode_usb_device  = this;

		_ua.device = &_usbd_device;
		_ua.iface  = &_usbd_iface;
		int found = 0;
		Genode::log("num_interfaces: ", _usb_device.config_descr.num_interfaces);
		for (uint8_t i = 0; i < _usb_device.config_descr.num_interfaces; i++) {

			Usb::Interface_descriptor iface_descr;
			_usb.interface_descriptor(i, 0, &iface_descr);

			uint16_t const total_length = _usb_device.config_descr.total_length;
			Genode::log("Probe interface ", i, ":",
			            " number: ",   iface_descr.number,
			            " class: ",    iface_descr.iclass,
			            " subclass: ", iface_descr.isubclass,
			            " config_descr length: ", total_length);

			_usb_iface_descr.bInterfaceClass    = iface_descr.iclass;
			_usb_iface_descr.bInterfaceSubClass = iface_descr.isubclass;

			_usb_config_descr.bLength         = _usb_device.config_descr.length;
			_usb_config_descr.bDescriptorType = _usb_device.config_descr.type;
			USETW(_usb_config_descr.wTotalLength, _usb_device.config_descr.total_length);
			_usb_config_descr.bNumInterface       = _usb_device.config_descr.num_interfaces;
			_usb_config_descr.bConfigurationValue = _usb_device.config_descr.config_value;
			_usb_config_descr.iConfiguration      = _usb_device.config_descr.config_index;
			_usb_config_descr.bmAttributes        = _usb_device.config_descr.attributes;
			_usb_config_descr.bMaxPower           = _usb_device.config_descr.max_power;

			// found = probe_cfdata(&_ua);
			// if (found) { break; }
		}

		_probed = true;
		return found;
	}

	bool probed() const { return _probed; }
	int  found()  const { return _found; }

	struct usb_interface_descriptor *usb_iface_descr()
	{
		return &_usb_iface_descr;
	}

	struct usb_config_descriptor *usb_config_descr()
	{
		return &_usb_config_descr;
	}
};

} /* anonymous namespace */


struct usbd_xfer *usbd_alloc_xfer(struct usbd_device *)
{
	Genode::error(__func__, ": not implemented");
	return NULL;
}


void usbd_free_xfer(struct usbd_xfer *xfer)
{
	Genode::error(__func__, ": not implemented");
}


usbd_status usbd_close_pipe(struct usbd_pipe *)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}

usbd_status usbd_do_request(struct usbd_device *, usb_device_request_t *,
                            void *)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}

void usbd_claim_iface(struct usbd_device *, int)
{
	Genode::error(__func__, ": not implemented");
}


void *usbd_alloc_buffer(struct usbd_xfer *, u_int32_t)
{
	Genode::error(__func__, ": not implemented");
	return NULL;
}


usbd_status usbd_device2interface_handle(struct usbd_device *,
                                         u_int8_t,
                                         struct usbd_interface **)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


usbd_status usbd_set_interface(struct usbd_interface *, int)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


usbd_status usbd_open_pipe(struct usbd_interface *iface, u_int8_t address,
                                 u_int8_t flags, struct usbd_pipe **pipe)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


void usbd_setup_isoc_xfer(struct usbd_xfer *, struct usbd_pipe *,
                          void *, u_int16_t *, u_int32_t, u_int16_t,
                          usbd_callback)
{
	Genode::error(__func__, ": not implemented");
}


usbd_status usbd_transfer(struct usbd_xfer *req)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


void usbd_get_xfer_status(struct usbd_xfer *, void **,
                          void **, u_int32_t *, usbd_status *)
{
	Genode::error(__func__, ": not implemented");
}


int usbd_is_dying(struct usbd_device *)
{
	Genode::error(__func__, ": not implemented");
	return 1;
}


usb_interface_descriptor_t *usbd_get_interface_descriptor(struct usbd_interface *iface)
{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(iface->genode_usb_device);
	Genode::log(__func__, ": called: ", iface);

	return usb.usb_iface_descr();
}


usb_config_descriptor_t *usbd_get_config_descriptor(struct usbd_device *dev)

{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	Genode::log(__func__, ": called: ", dev);

	return usb.usb_config_descr();
}


char const *usbd_errstr(usbd_status)
{
	Genode::error(__func__, ": not implemented");
	return "<unknown>";
}


void getmicrotime(struct timeval *tv)
{
	Genode::error(__func__, ": not implemented");
	if (tv) {
		tv->tv_sec = 0;
		tv->tv_usec = 0;
	}
}


static Usb_driver *_usb_driver;


int Bsd::probe_drivers(Genode::Env &env, Genode::Allocator &alloc,
                       Genode::Signal_context_capability announce_sigh,
                       Genode::Signal_context_capability report_sigh)
{
	static Usb_report_handler report_handler(env, report_sigh);
	(void)announce_sigh;

	try {
		Genode::log("--- probe USB audio driver ---");
		while (true) {
			Label device = report_handler.process_report();
			if (!_usb_driver && device.valid()) {
				_usb_driver =
					new (&alloc) Usb_driver { env, alloc, device };
			}

			if (_usb_driver && _usb_driver->probe()) {
				break;
			}

			Genode::log("wait for USB plug signal");
			Bsd::scheduler().current()->block_and_schedule();
		}
		return _usb_driver->found();
	}
	catch (Usb_driver::Could_not_read_config_descriptor) { }
	catch (Usb_driver::Device_already_claimed) { }
	catch (...) { /* XXX */ }

	return 0;
}

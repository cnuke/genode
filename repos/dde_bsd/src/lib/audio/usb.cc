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
#include <util/bit_array.h>
#include <timer_session/connection.h>

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
			_plugged = true;
			// _found = probe();
		} else {
			Genode::log("device unplugged");
			_plugged = false;
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
	// struct usbd_interface           _usbd_iface      { NULL };
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

	bool _config_descriptor_available { false };

	Genode::Signal_context_capability _task_sigh;

	void _handle_ctrl(Usb::Packet_descriptor &p)
	{
		Genode::log("Handle USB packet");

		uint8_t const * const data = (uint8_t *) _usb.source()->packet_content(p);
		int             const len  = p.control.actual_size;

		if (len != (int)p.size()) {
			Genode::warning("size differs: ", len, " != ", p.size());
		}

		if ((size_t)len > sizeof (_config_descr_buffer)) {
			Genode::error("static config descriptor buffer too small");
			return;
		}

		Genode::memcpy(_config_descr_buffer, data, len);
		_config_descriptor_available = true;

		Genode::Signal_transmitter(_task_sigh).submit();
	}

	void _handle_ctrl_sync(Usb::Packet_descriptor &p, void *buf)
	{
		Genode::log("Handle ctrl sync USB packet");

		int const len = p.control.actual_size;
		if (len != (int)p.size()) {
			Genode::warning("size differs: ", len, " != ", p.size());
		}

		if ((p.control.request_type & 0x80) && buf) {
			uint8_t const * const data = (uint8_t *) _usb.source()->packet_content(p);
			Genode::memcpy(buf, data, len);
		}

		Genode::Signal_transmitter(_task_sigh).submit();
	}

	void _handle_alt_setting(Usb::Packet_descriptor &p, void *buf)
	{
		Genode::log("Handle alt setting USB packet");

		Genode::Signal_transmitter(_task_sigh).submit();
	}

	struct Sync_completion : Usb::Completion
	{
		Usb_driver &_usb_driver;

		void *buf;
		bool  success;
		bool  completed;

		Sync_completion(Usb_driver &driver)
		: _usb_driver { driver }, buf { nullptr }, success { false }, completed { false }
		{ }

		void complete(Usb::Packet_descriptor &p) override
		{
			completed = true;
			success = p.succeded;
			if (!p.succeded) {
				Genode::error("sync completion failed");
				return;
			}

			Genode::log(__func__, ": success: ", success);

			using Upd = Usb::Packet_descriptor;

			switch (p.type) {
			case Upd::CTRL:        _usb_driver._handle_ctrl_sync(p, buf); break;
			case Upd::ALT_SETTING: _usb_driver._handle_alt_setting(p, buf); break;
			default: break;
			}
		}
	};

	Sync_completion _sync_completion { *this };


	struct Completion : Usb::Completion
	{
		Usb_driver &_usb_driver;

		Completion(Usb_driver &driver) : _usb_driver { driver }
		{ }

		void complete(Usb::Packet_descriptor &p) override
		{
			if (!p.succeded) {
				Genode::error("completion failed");
				return;
			}

			using Upd = Usb::Packet_descriptor;

			switch (p.type) {
			case Upd::CTRL: _usb_driver._handle_ctrl(p); break;
			default: break;
			}
		}
	};

	Completion _completion { *this };

	void _get_config_descriptor()
	{
		Genode::error(__func__, ":", __LINE__, ": this: ", this, " _usb: ", &_usb, " source: ", _usb.source());
		bool const ready = _usb.source()->ready_to_submit();
		if (!ready) {
			Genode::error(__func__, ":", __LINE__, ": could not submit");
			return;
		}

		size_t const total_length = _usb_device.config_descr.total_length;
		Usb::Packet_descriptor p = _usb.source()->alloc_packet(total_length);
		p.completion = &_completion;

		enum {
			REQUEST_TYPE              = 0x80,  /* bmRequestType */
			REQUEST_GET_DESCRIPTOR    = 0x06,  /* bRequest */
			REQUEST_CONFIG_DESCRIPTOR = 0x02   /* wValue */
		};

		p.type                 = Usb::Packet_descriptor::CTRL;
		p.control.request_type = REQUEST_TYPE;
		p.control.request      = REQUEST_GET_DESCRIPTOR;
		p.control.value        = (REQUEST_CONFIG_DESCRIPTOR << 8);
		p.control.index        = 0;
		p.control.timeout      = 1000; // XXX

		_usb.source()->submit_packet(p);
	}

	void _process_completions()
	{
		while (_usb.source()->ack_avail()) {
			Usb::Packet_descriptor p = _usb.source()->get_acked_packet();
			if (p.completion) {
				p.completion->complete(p);
			}
			_usb.source()->release_packet(p);

			// XXX for now just handle one packet
			break;
		}

		if (_usb.source()->ack_avail()) {
			Genode::Signal_transmitter(_task_sigh).submit();
		}
	}

	enum State {
		INVALID, INIT, GET_CONFIG_DESCR, GOT_CONFIG_DESCR,
		XXX
	};

	State _state { INVALID };

	Timer::Connection _timer { _env };

	Usb_driver(Genode::Env       &env,
	           Genode::Allocator &alloc,
	           Label              label,
	           Genode::Signal_context_capability task_sigh)
	:
		_env { env },
		_alloc { alloc },
		_usb_state_change_sigh { _env.ep(), *this,
		                         &Usb_driver::_handle_usb_state_change },
		_usb_alloc { &_alloc },
		_usb { _env, &_usb_alloc, label.string(), 256 * 1024,
		       _usb_state_change_sigh },
		_task_sigh { task_sigh }
	{
		_usb.tx_channel()->sigh_ack_avail(task_sigh);
		try {
			_usb.config_descriptor(&_usb_device.dev_descr,
			                       &_usb_device.config_descr);
		} catch (Usb::Session::Device_not_found) {
			Genode::error("cound not read config descriptor");
			throw Could_not_read_config_descriptor();
		}

		_usbd_device.speed = _get_speed(_usb_device.dev_descr);
		_usbd_device.genode_usb_device = this;
		// _usbd_iface.genode_usb_device  = this;

		_ua.device = &_usbd_device;
		// _ua.iface  = &_usbd_iface;

		_state = State::INIT;
	}

	bool plugged() const { return _plugged; }

	Timer::Connection const &timer() const { return _timer; }

	void execute()
	{
		switch (_state) {
		case State::INIT:
			_get_config_descriptor();
			_state = State::GET_CONFIG_DESCR;
			break;
		case State::GET_CONFIG_DESCR:
			_process_completions();
			if (_config_descriptor_available) {
				_state = State::GOT_CONFIG_DESCR;
			}
			break;
		// case State::GOT_CONFIG_DESCR: [[fallthrough]];
		case State::XXX:
		{
			_process_completions();
		}
		default: break;
		}
	}

	bool config_descriptor_available() const
	{
		return _state == State::GOT_CONFIG_DESCR;
	}

	void claim_interface(unsigned index)
	{
		try {
			_usb.claim_interface(index);
		} catch (Usb::Session::Interface_already_claimed) {
			Genode::warning("interface ", index, " already claimed");
			// throw Device_already_claimed();
		}
	}

	Usb::Interface_descriptor       _iface_descr[16] { };
	struct usbd_interface           _usbd_iface[16] { };
	struct usb_interface_descriptor _usb_iface_descr[16] { };

	int probe()
	{
		_state = State::XXX;

		_usb_config_descr.bLength             = _usb_device.config_descr.length;
		_usb_config_descr.bDescriptorType     = _usb_device.config_descr.type;
		USETW(_usb_config_descr.wTotalLength, _usb_device.config_descr.total_length);
		_usb_config_descr.bNumInterface       = _usb_device.config_descr.num_interfaces;
		_usb_config_descr.bConfigurationValue = _usb_device.config_descr.config_value;
		_usb_config_descr.iConfiguration      = _usb_device.config_descr.config_index;
		_usb_config_descr.bmAttributes        = _usb_device.config_descr.attributes;
		_usb_config_descr.bMaxPower           = _usb_device.config_descr.max_power;

		_ua.device = &_usbd_device;

		int found = 0;
		Genode::log("prepare num_interfaces: ", _usb_device.config_descr.num_interfaces);
		for (uint8_t i = 0; i < _usb_device.config_descr.num_interfaces; i++) {

			// XXX move to construction
			_usb.interface_descriptor(i, 0, &_iface_descr[i]);
			_usbd_iface[i] = { i, this };

			uint16_t const total_length = _usb_device.config_descr.total_length;
			_usb_iface_descr[i].bInterfaceClass    = _iface_descr[i].iclass;
			_usb_iface_descr[i].bInterfaceSubClass = _iface_descr[i].isubclass;

			Genode::log("prepare interface ", i, ":",
			            " number: ",   _iface_descr[i].number,
			            " class: ",    _iface_descr[i].iclass,
			            " subclass: ", _iface_descr[i].isubclass,
			            " config_descr length: ", total_length);
		}

		Genode::log("probe num_interfaces: ", _usb_device.config_descr.num_interfaces);
		for (uint8_t i = 0; i < _usb_device.config_descr.num_interfaces; i++) {

			Genode::log("Probe interface ", i, ":",
			            " number: ",   _iface_descr[i].number,
			            " class: ",    _iface_descr[i].iclass,
			            " subclass: ", _iface_descr[i].isubclass);

			_ua.iface  = &_usbd_iface[i];
			found = probe_cfdata(&_ua);
			if (found) {
				_found++;
				break;
			}
		}

		_probed = true;
		return found;
	}

	bool probed() const { return _probed; }
	int  found()  const { return _found; }

	struct usbd_interface *get_usbd_interface(uint8_t num)
	{
		if (num >= _usb_device.config_descr.num_interfaces) {
			return NULL;
		}

		Genode::error("num: ", num, " iface: ", &_usbd_iface[num], " genode: ",
		              _usbd_iface[num].genode_usb_device);

		return &_usbd_iface[num];
	}

	struct usb_interface_descriptor *usb_iface_descr(uint8_t num)
	{
		if (num >= _usb_device.config_descr.num_interfaces) {
			return NULL;
		}

		return &_usb_iface_descr[num];
	}

	struct usb_config_descriptor *usb_config_descr()
	{
		return (struct usb_config_descriptor*)_config_descr_buffer;
	}

	usbd_status set_alternate_interface(uint8_t ifnum, uint8_t altnum)
	{
		bool const ready = _usb.source()->ready_to_submit();
		if (!ready) {
			Genode::error(__func__, ":", __LINE__, ": could not submit");
			return USBD_IOERROR;
		}

		Usb::Packet_descriptor p = _usb.source()->alloc_packet(0);

		_sync_completion.completed = false;
		_sync_completion.success = false;

		p.completion = &_sync_completion;

		p.type                  = Usb::Packet_descriptor::ALT_SETTING;
		p.interface.number      = ifnum;
		p.interface.alt_setting = altnum;

		_usb.source()->submit_packet(p);

		while (!_sync_completion.completed) {
			Bsd::scheduler().current()->block_and_schedule();
			_process_completions();
		}

		return _sync_completion.success ? USBD_NORMAL_COMPLETION : USBD_IOERROR;
	}

	usbd_status sync_request(usb_device_request_t const &req, void *buf)
	{
		bool const ready = _usb.source()->ready_to_submit();
		if (!ready) {
			Genode::error(__func__, ":", __LINE__, ": could not submit");
			return USBD_IOERROR;
		}

		uint16_t const len = UGETW(req.wLength);
		Usb::Packet_descriptor p = _usb.source()->alloc_packet(len);
		bool const in = req.bmRequestType & 0x80;

		if (!in) {
			void *dst = (void*) _usb.source()->packet_content(p);
			Genode::memcpy(dst, buf, len);
		}

		_sync_completion.completed = false;
		_sync_completion.success = false;
		_sync_completion.buf = in ? buf : nullptr;

		p.completion = &_sync_completion;

		p.type                 = Usb::Packet_descriptor::CTRL;
		p.control.request_type = req.bmRequestType;
		p.control.request      = req.bRequest;
		p.control.value        = UGETW(req.wValue);
		p.control.index        = UGETW(req.wIndex);
		p.control.timeout      = 5000; // XXX

		_usb.source()->submit_packet(p);

		while (!_sync_completion.completed) {
			Bsd::scheduler().current()->block_and_schedule();
			_process_completions();
		}

		return _sync_completion.success ? USBD_NORMAL_COMPLETION : USBD_IOERROR;
	}

	template <unsigned ENTRIES>
	struct Id_alloc
	{
		using Bitmap = Genode::Bit_array<ENTRIES>;
		Bitmap _bitmap { };

		enum { INVALID_ID = (uint16_t)~0, };

		uint16_t _bitmap_find_free() const
		{
			for (size_t i = 0; i < ENTRIES; i++) {
				if (_bitmap.get(i, 1)) { continue; }
				return i;
			}
			return INVALID_ID;
		}

		bool used(uint16_t const cid) const
		{
			return _bitmap.get(cid, 1);
		}

		uint16_t alloc()
		{
			uint16_t const id = _bitmap_find_free();
			if (id != INVALID_ID) {
				_bitmap.set(id, 1);
			}
			return id;
		}

		void free(uint16_t id)
		{
			_bitmap.clear(id, 1);
		}
	};

	enum { MAX_PACKETS = 64, };

	Id_alloc<MAX_PACKETS> _id_alloc { };

	struct Iso_packet
	{
		bool valid;
		Usb::Packet_descriptor packet_descriptor;

		uint16_t id;
	};

	Iso_packet _iso_packet[MAX_PACKETS] { };

	void _handle_iso(Usb::Packet_descriptor &p,
	                 usbd_xfer *xfer, usbd_callback callback,
	                 Iso_packet &iso_packet)
	{
		iso_packet.valid = false;
		/* copy completed packet descriptor to get current actual_length */
		iso_packet.packet_descriptor = p;

		if (callback) {
			callback(xfer, xfer->priv,
			         p.succeded ? USBD_NORMAL_COMPLETION
			                    : USBD_IOERROR);
		}
	}

	struct Iso_completion : Usb::Completion
	{
		Usb_driver *driver;

		usbd_callback callback;
		usbd_xfer *xfer;
		uint16_t packet_id;

		Iso_completion()
		:
			driver   { nullptr },
			callback { nullptr }, xfer { nullptr }
		{ }

		void complete(Usb::Packet_descriptor &p) override
		{
			driver->_id_alloc.free(packet_id);

			if (!p.succeded) {
				Genode::error("iso completion failed");
				// return;
			}

			using Upd = Usb::Packet_descriptor;

			switch (p.type) {
			case Upd::ISOC: driver->_handle_iso(p, xfer, callback,
			                                    driver->_iso_packet[packet_id]); break;
			default: break;
			}
		}
	};

	Iso_completion _iso_completion[MAX_PACKETS] { };

	void *setup_iso_packet(struct usbd_xfer *xfer, struct usbd_pipe *pipe,
                           u_int16_t *frlengths, u_int32_t nframes,
                           usbd_callback callback)
	{
		bool const ready = _usb.source()->ready_to_submit();
		if (!ready) {
			Genode::error(__func__, ":", __LINE__, ": could not submit");
			return NULL;
		}

		uint16_t packet_id = _id_alloc.alloc();
		if (packet_id == Id_alloc<MAX_PACKETS>::INVALID_ID) {
			Genode::error(__func__, ":", __LINE__, ": no free ISO packet left");
			return NULL;
		}

		if (_iso_packet[packet_id].valid) {
			Genode::error(__func__, ":", __LINE__, ": ISO packet already used");
			return NULL;
		}

		size_t len = 0;
		for (u_int32_t i = 0; i < nframes; i++) {
			len += frlengths[i];
		}

		Usb::Packet_descriptor p = _usb.source()->alloc_packet(len);
		bool const in = pipe->addr & 0x80;

		if (!in) {
			void *dst = (void*) _usb.source()->packet_content(p);
			Genode::memcpy(dst, xfer->dma_buf, len);
		}

		_iso_completion[packet_id].driver    = this;
		_iso_completion[packet_id].xfer      = xfer;
		_iso_completion[packet_id].callback  = callback;
		_iso_completion[packet_id].packet_id = packet_id;

		p.completion = &_iso_completion[packet_id];

		p.type                       = Usb::Packet_descriptor::ISOC;
		p.transfer.ep                = pipe->addr;
		p.transfer.number_of_packets = nframes;
		for (u_int32_t i = 0; i < nframes; i++) {
			p.transfer.packet_size[i] = frlengths[i];
		}

		_iso_packet[packet_id].packet_descriptor = p;
		_iso_packet[packet_id].valid = true;
		return &_iso_packet[packet_id];
	}

	usbd_status submit_iso_packet(struct usbd_xfer *xfer)
	{
		Iso_packet &iso =
			*reinterpret_cast<Iso_packet*>(xfer->genode_packet_descriptor);

		_usb.source()->submit_packet(iso.packet_descriptor);
		return USBD_IN_PROGRESS;
	}
};

} /* anonymous namespace */


struct usbd_xfer *usbd_alloc_xfer(struct usbd_device *dev)
{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	// Genode::log(__func__, ": dev: ", dev);

	struct usbd_xfer *xfer =
		(struct usbd_xfer*)malloc(sizeof (struct usbd_xfer), 0, M_ZERO);
	if (xfer == NULL) {
		return NULL;
	}
	xfer->genode_usb_device = &usb;
	xfer->dma_buf  = NULL;
	xfer->dma_size = 0;
	return xfer;
}


void usbd_free_xfer(struct usbd_xfer *xfer)
{
	// Genode::log(__func__, ": xfer: ", xfer);

	if (xfer->dma_size) {
		free(xfer->dma_buf, 0, xfer->dma_size);

		xfer->dma_size = 0;
		xfer->dma_buf  = NULL;
	}

	free(xfer, 0, sizeof (struct usbd_xfer));
}


void *usbd_alloc_buffer(struct usbd_xfer *xfer, u_int32_t size)
{
	// Genode::log(__func__, ": xfer: ", xfer, " size: ", size);
	if (xfer->dma_size) {
		Genode::warning(__func__, ": xfer: ", xfer,
		                " dma_size: ", xfer->dma_size,
		                " dma_buf: ", xfer->dma_buf,
		                " used, leaking memory");
	}

	xfer->dma_size = size;
	xfer->dma_buf  = malloc(xfer->dma_size, 0, M_ZERO);
	if (xfer->dma_buf == NULL) {
		return NULL;
	}
	Genode::warning(__func__, ": xfer: ", xfer,
	                " dma_size: ", xfer->dma_size,
	                " dma_buf: ", xfer->dma_buf);
	return xfer->dma_buf;
}


usbd_status usbd_open_pipe(struct usbd_interface *iface, u_int8_t address,
                                 u_int8_t flags, struct usbd_pipe **pipe)
{
	// Genode::log(__func__, ": iface: ", iface,
	//             " addr: ", Genode::Hex(address), " flags: ", flags);
	struct usbd_pipe *p =
		(struct usbd_pipe*)malloc(sizeof (struct usbd_pipe), 0, M_ZERO);
	if (p == NULL) {
		return USBD_IOERROR;
	}

	p->iface = iface;
	p->addr  = address;

	*pipe = p;
	return USBD_NORMAL_COMPLETION;
}


usbd_status usbd_close_pipe(struct usbd_pipe *)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


usbd_status usbd_do_request(struct usbd_device *dev,
                            usb_device_request_t *req,
                            void *buf)
{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	// Genode::log(__func__, ": called: ", dev, " req: ", req, " buf: ", buf);
	return usb.sync_request(*req, buf);
}

void usbd_claim_iface(struct usbd_device *dev, int i)
{
	// XXX uaudio(4) will claim the same interface multiple times
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	// Genode::log(__func__, ": called: ", dev, " i: ", i);
	usb.claim_interface(i);
}


usbd_status usbd_device2interface_handle(struct usbd_device *dev,
                                         u_int8_t num,
                                         struct usbd_interface **iface)
{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	// Genode::log(__func__, ": dev: ", dev, " num: ", num);

	struct usbd_interface *p = usb.get_usbd_interface(num);
	if (p == NULL) {
		return USBD_IOERROR;
	}

	*iface = p;
	return USBD_NORMAL_COMPLETION;
}


usbd_status usbd_set_interface(struct usbd_interface *iface, int altnum)
{
	if (iface == NULL) {
		Genode::error(__func__, ": iface: ", iface);
		return USBD_IOERROR;
	}

	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(iface->genode_usb_device);
	// Genode::log(__func__, ": genode_usb_device: ", iface->genode_usb_device,
	//             " iface: ", iface, " num: ", iface->num, " altnum: ", altnum);

	return usb.set_alternate_interface(iface->num, altnum);
}


void usbd_setup_isoc_xfer(struct usbd_xfer *xfer, struct usbd_pipe *pipe,
                          void *priv, u_int16_t *frlengths, u_int32_t nframes,
                          u_int16_t flags, usbd_callback callback)
{
	// Genode::log(__func__, ": xfer: ", xfer, " pipe: ", pipe, " priv: ", priv,
	//             " frlengths: ", frlengths, " nframes: ", nframes, " flags: ",
	//             flags, " callback: ", callback);
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(xfer->genode_usb_device);
	xfer->priv = priv;

	xfer->genode_packet_descriptor =
		usb.setup_iso_packet(xfer, pipe, frlengths, nframes, callback);
	if (xfer->genode_packet_descriptor == NULL) {
		Genode::warning("could not setup ISO transfer");
	}
}


usbd_status usbd_transfer(struct usbd_xfer *xfer)
{
	// Genode::log(__func__, ": xfer: ", xfer,
	//             " pd: ", xfer->genode_packet_descriptor);
	if (xfer->genode_packet_descriptor == NULL) {
		return USBD_IOERROR;
	}

	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(xfer->genode_usb_device);
	return usb.submit_iso_packet(xfer);
}


void usbd_get_xfer_status(struct usbd_xfer *xfer, void **priv,
                          void **buffer, u_int32_t *count, usbd_status *status)
{
	// Genode::log(__func__, ": xfer: ", xfer,
	//             " pd: ", xfer->genode_packet_descriptor);
	Usb_driver::Iso_packet const &iso_packet =
		*reinterpret_cast<Usb_driver::Iso_packet*>(xfer->genode_packet_descriptor);

	if (count) {
		*count = iso_packet.packet_descriptor.transfer.actual_size;
	}
}


int usbd_is_dying(struct usbd_device *dev)
{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	return usb.plugged() ? 0 : 1;
}


usb_interface_descriptor_t *usbd_get_interface_descriptor(struct usbd_interface *iface)
{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(iface->genode_usb_device);
	// Genode::log(__func__, ": called: ", iface);

	return usb.usb_iface_descr(iface->num);
}


usb_config_descriptor_t *usbd_get_config_descriptor(struct usbd_device *dev)

{
	Usb_driver &usb = *reinterpret_cast<Usb_driver*>(dev->genode_usb_device);
	// Genode::log(__func__, ": called: ", dev);

	return usb.usb_config_descr();
}


char const *usbd_errstr(usbd_status)
{
	Genode::error(__func__, ": not implemented");
	return "<unknown>";
}


static Usb_driver *_usb_driver;


void getmicrotime(struct timeval *tv)
{
	/* AFAICT only used in DEBUG code */
	// Genode::error(__func__, ": not implemented");
	if (tv) {
		tv->tv_sec = 0;
		tv->tv_usec = 0;
	}

	if (_usb_driver) {
		uint64_t const ms = _usb_driver->timer().elapsed_ms();
		tv->tv_sec = ms / 1000;
		tv->tv_usec = (ms % 1000) * 1000;
	}
}


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
				Genode::log("Device valid: ", device);
				_usb_driver =
					new (&alloc) Usb_driver { env, alloc, device, report_sigh };
			}

			if (_usb_driver) {
				_usb_driver->execute();

				if (_usb_driver->config_descriptor_available()) {
					Genode::log("Config_descriptor available");
					if (_usb_driver->probe()) {
						break;
					}
				}
			}

			Genode::log("wait for USB signal");
			Bsd::scheduler().current()->block_and_schedule();
		}
		return _usb_driver->found();
	}
	catch (Usb_driver::Could_not_read_config_descriptor) { }
	catch (Usb_driver::Device_already_claimed) { }
	catch (...) { /* XXX */ }

	return 0;
}


void Bsd::execute_driver()
{
	if (_usb_driver) {
		_usb_driver->execute();
	}
}

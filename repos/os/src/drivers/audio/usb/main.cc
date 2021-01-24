/*
 * \brief  USB audio driver component
 * \author Josef Soentgen
 * \date   2021-01-13
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/reporter.h>
#include <os/static_root.h>
#include <usb/types.h>
#include <usb_session/connection.h>


static bool const verbose_intr = false;
static bool const verbose      = false;
static bool const debug        = false;
static bool const dump_dt      = false;


namespace Util {

	using namespace Genode;
	using namespace Usb;

	template<typename T, size_t S>
	struct Packet_queue
	{
		T _elems[S] { };

		unsigned _tail { 0 };
		unsigned _head { 0 };
		unsigned _count { 0 };

		void enqueue(T &e)
		{
			_elems[_tail] = e;
			_tail = (_tail + 1) % S;
			++_count;
		}

		T dequeue()
		{
			unsigned const cur = _head;
			_head = (_head + 1) % S;
			--_count;
			return _elems[cur];
		}

		T head() const { return _elems[_head]; }

		unsigned queued() const { return _count; }
	};

	namespace Dump {

		void device(Device_descriptor &);
		void iface(Interface_descriptor &);
		void ep(Endpoint_descriptor &);
	} /* namespace Dump */
} /* namespace Util */


void Util::Dump::device(Device_descriptor &descr)
{
	log("Device: "
	    "len: ",             Hex(descr.length),    " "
	    "type: " ,           Hex(descr.type),      " "
	    "class: ",           Hex(descr.dclass),    " "
	    "sub-class: ",       Hex(descr.dsubclass), " "
	    "proto: ",           Hex(descr.dprotocol), " "
	    "max_packet_size: ", Hex(descr.max_packet_size));
	log("        "
	    "vendor: ",      Hex(descr.vendor_id),  " "
	    "product: ",     Hex(descr.product_id), " "
	    "num_configs: ", Hex(descr.num_configs));
}


void Util::Dump::iface(Interface_descriptor &descr)
{
	log("Iface: ",
	    "len: ",           Hex(descr.length),        " "
	    "type: ",          Hex(descr.type),          " "
	    "number: ",        Hex(descr.number),        " "
	    "alt_settings: ",  Hex(descr.alt_settings),  " "
	    "num_endpoints: ", Hex(descr.num_endpoints), " "
	    "iclass: ",        Hex(descr.iclass),        " "
	    "isubclass: ",     Hex(descr.isubclass),     " "
	    "iprotocol: ",     Hex(descr.iprotocol),     " "
	    "str_index: ",     Hex(descr.interface_index));
}


void Util::Dump::ep(Endpoint_descriptor &descr)
{
	log("Endpoint: ",
	    "len: ",              Hex(descr.length),          " "
	    "type: ",             Hex(descr.type),            " "
	    "address: ",          Hex(descr.address),         " "
	    "attributes: ",       Hex(descr.attributes),      " "
	    "max_packet_size: ",  Hex(descr.max_packet_size), " "
	    "polling_interval: ", descr.polling_interval);
}


namespace Usb {
	using namespace Genode;

	struct Packet_stream : Genode::Interface
	{
		virtual Usb::Packet_descriptor alloc(size_t) = 0;
		virtual void free(Usb::Packet_descriptor &) = 0;
		virtual void submit(Usb::Packet_descriptor &) = 0;
		virtual char *content(Usb::Packet_descriptor &) = 0;
	};

	struct Audio;
	struct Main;
}


/******************************
 ** USB audio implementation **
 ******************************/

struct Usb::Audio : Packet_stream
{
	Env &_env;

	Attached_rom_dataspace _samples_rom { _env, "samples.raw" };

	Attached_ram_dataspace _in_samples_ram { _env.ram(), _env.rm(), 4u << 20 };

	struct Samples
	{
		char   * const base;
		size_t const  size;

		size_t current_offset;

		Samples(char * const base, size_t size)
		:
			base { base }, size { size }, current_offset { 0 }
		{
			Genode::log("base: ", (void *)base, " size: ", size);
		}
	};

	Samples _samples {
		_samples_rom.local_addr<char>(), _samples_rom.size() };

	Samples _in_samples {
		_in_samples_ram.local_addr<char>(), _in_samples_ram.size() };

	enum { MAX_OUT_TRANSFERS = 25, };
	using Packet_queue =
		Util::Packet_queue<Usb::Packet_descriptor, MAX_OUT_TRANSFERS>;

	Packet_queue _packet_queue { };

	enum {
		// FREQ   = 48000,
		FREQ   = 44100,
		PERIOD = 10,
		PPS    = 1000 / PERIOD,
		CHANS  = 2,
		BPS    = sizeof (short),
		BPT    = (FREQ / PPS) * CHANS * BPS,
	};

	void _handle_state_change()
	{
		if (_usb->plugged()) {
			if (verbose) {
				log("USB device plugged in");
			}
			_probe_device();
			return;
		}

		if (verbose) {
			log("USB device unplugged");
		}
	}

	Signal_handler<Audio> _state_sigh {
		_env.ep(), *this, &Audio::_handle_state_change };

	enum { MAX_TRANSFERS = Usb::Session::TX_QUEUE_SIZE * 2 + 1, };

	Allocator_avl                   _usb_alloc;
	Constructible<Usb::Connection>  _usb { };

	Attached_rom_dataspace _usb_devices_rom { _env, "usb-devices" };

	Signal_handler<Audio> _usb_devices_sigh {
		_env.ep(), *this, &Audio::_handle_usb_devices_report };

	void _handle_usb_devices_report()
	{
		if (verbose) {
			log("New USB devices report");
		}

		_usb_devices_rom.update();
		if (!_usb_devices_rom.valid()) {
			warning("ignore invalid report");
			return;
		}

		auto check_device = [&] (Xml_node device) {

			enum { USB_AUDIO_CLASS = 0x01u, };
			if (device.attribute_value("class", 0u) != USB_AUDIO_CLASS) {
				return;
			}

			if (_usb.constructed()) {
				warning("USB connection already constructed, ignore report");
				return;
			}

			try {
				// MAX_TRANSFERS * 1024 + 40000 (bulk offset)
				_usb.construct(_env, &_usb_alloc, "", 1u << 20, _state_sigh);
				_usb->tx_channel()->sigh_ack_avail(_ack_avail_sigh);
			} catch (...) {
				error("could not construct USB connection");
				return;
			}
		};
		try {
			_usb_devices_rom.xml().for_each_sub_node("device", check_device);
		} catch (Xml_node::Nonexistent_sub_node) {
			/* apparently an empty report */
		}
	}

	struct Iface
	{
		uint8_t number;
		uint8_t alt_setting;
		uint8_t ep;

		enum State : uint8_t {
			DISABLED,
			ENABLE_PENDING, ENABLED,
			CONFIGURE_PENDING, CONFIGURED,
			USEABLE, ACTIVE
		};
		State state;

		void enable(Packet_stream &ps)
		{
			Usb::Packet_descriptor p = ps.alloc(0);
			p.type                   = Usb::Packet_descriptor::ALT_SETTING;
			p.interface.number       = number;
			p.interface.alt_setting  = alt_setting;

			Completion *c = dynamic_cast<Completion*>(p.completion);
			c->iface = this;

			state = State::ENABLE_PENDING;
			ps.submit(p);
		}

		void configure(Packet_stream &ps, int freq)
		{
			enum {
				USB_REQUEST_TO_DEVICE  = 0x00,
				USB_REQUEST_TYPE_CLASS = 0x20,
				USB_REQUEST_RCPT_EP    = 0x02,

				USB_AUDIO_REQUEST_SET_CUR     = 0x01,
				USB_AUDIO_REQUEST_SELECT_RATE = 0x01,

				REQUEST = USB_REQUEST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_RCPT_EP,
			};

			uint8_t cmd[3] { };
			cmd[0] = freq;
			cmd[1] = freq >> 8;
			cmd[2] = freq >> 16;

			Usb::Packet_descriptor p = ps.alloc(sizeof(cmd));

			char *data = ps.content(p);
			Genode::memcpy(data, cmd, sizeof(cmd));

			p.type                 = Usb::Packet_descriptor::CTRL;
			p.control.request_type = REQUEST;
			p.control.request      = USB_AUDIO_REQUEST_SET_CUR;
			p.control.value        = (USB_AUDIO_REQUEST_SELECT_RATE) << 8;
			p.control.index        = ep;
			p.control.timeout      = 1000;

			Completion *c = dynamic_cast<Completion*>(p.completion);
			c->iface = this;

			state = State::CONFIGURE_PENDING;
			ps.submit(p);
		}

		void queue_in_transfer(Packet_stream &ps, Packet_queue &q)
		{
			Usb::Packet_descriptor p     = ps.alloc(BPT);
			p.type                       = Usb::Packet_descriptor::ISOC;
			p.transfer.ep                = ep;
			p.transfer.polling_interval  = Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;

			Completion *c = dynamic_cast<Completion*>(p.completion);
			c->iface = this;

			p.transfer.number_of_packets = 10;

			if (FREQ == 44100) {
				for (int i = 0; i < p.transfer.number_of_packets - 1; i++) {
					p.transfer.packet_size[i] = 176;
				}
				p.transfer.packet_size[9] = 180;
			} else if (FREQ == 48000) {
				for (int i = 0; i < p.transfer.number_of_packets; i++) {
					p.transfer.packet_size[i] = 192;
				}
			}
			ps.submit(p);
			q.enqueue(p);
		}

		void complete_in_transfer(Packet_stream &ps,
		                          Usb::Packet_descriptor &p,
		                          Packet_queue &q, Samples &samples)
		{
			(void)q;

			char *content = ps.content(p);

			size_t length = 0;
			for (int i = 0; i < p.transfer.number_of_packets; i++) {
				length += p.transfer.packet_size[i];
			}

			// XXX double mapping would be easier
			size_t leftover = 0;
			if (samples.current_offset + length > samples.size) {
				size_t t = samples.size - samples.current_offset;
				leftover = length - t;
				length = t;
			}
			Genode::memcpy(samples.base + samples.current_offset, content, length);
			samples.current_offset = (samples.current_offset + length) % samples.size;
			if (leftover) {
				Genode::memcpy(samples.base + samples.current_offset, content + length, leftover);
				samples.current_offset = (samples.current_offset + leftover) % samples.size;
			}
		}

		void transfer(Packet_stream &ps, Packet_queue &q,
		              Samples &samples, size_t length)
		{
			try {
				Usb::Packet_descriptor p     = ps.alloc(length);
				p.type                       = Usb::Packet_descriptor::ISOC;
				p.transfer.ep                = ep;
				p.transfer.polling_interval  = Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;

				Completion *c = dynamic_cast<Completion*>(p.completion);
				c->iface = this;

				p.transfer.number_of_packets = 10;

				if (FREQ == 44100) {
					for (int i = 0; i < p.transfer.number_of_packets - 1; i++) {
						p.transfer.packet_size[i] = 176;
					}
					p.transfer.packet_size[9] = 180;
				} else if (FREQ == 48000) {
					for (int i = 0; i < p.transfer.number_of_packets; i++) {
						p.transfer.packet_size[i] = 192;
					}
				}

				char *content = ps.content(p);

				// XXX double mapping would be easier
				size_t leftover = 0;
				if (samples.current_offset + length > samples.size) {
					size_t t = samples.size - samples.current_offset;
					leftover = length - t;
					length = t;
				}
				Genode::memcpy(content, samples.base + samples.current_offset, length);
				samples.current_offset = (samples.current_offset + length) % samples.size;
				if (leftover) {
					Genode::memcpy(content + length, samples.base + samples.current_offset, leftover);
					samples.current_offset = (samples.current_offset + leftover) % samples.size;
				}

				q.enqueue(p);

			} catch (...) {
				error("could not fill isoc packet");
			}

			bool const active = state == State::ACTIVE;

			if (active && q.queued() > 0) {
				Usb::Packet_descriptor p = q.dequeue();
				ps.submit(p);
				return;
			}

			enum { THRESHOLD = 2, };
			if (!active && q.queued() >= THRESHOLD) {
				for (int i = 0; i < THRESHOLD; i++) {
					Usb::Packet_descriptor p = q.dequeue();
					ps.submit(p);
				}
				state = State::ACTIVE;
				return;
			}

			state = State::USEABLE;
		}
	};

	Usb::Config_descriptor    config_descr { };
	Usb::Device_descriptor    device_descr { };
	Usb::Interface_descriptor iface_descr  { };
	Usb::Endpoint_descriptor  ep_descr     { };

	void handle_alt_setting(Iface &iface, Packet_descriptor &p)
	{
		try {
			_usb->interface_descriptor(p.interface.number,
			                           p.interface.alt_setting,
			                           &iface_descr);
			if (dump_dt) { Util::Dump::iface(iface_descr); }
		} catch (Usb::Session::Interface_not_found) {
			error("could not read interface descriptor");
			return;
		}

		// XXX check interface is indeed USB Audio

		try { _usb->claim_interface(p.interface.number); }
		catch (Usb::Session::Interface_already_claimed) {
			error("could not claim device");
			return;
		}

		try {
			_usb->endpoint_descriptor(p.interface.number,
			                          p.interface.alt_setting,
			                          _playback.ep,
			                          &ep_descr);
			if (dump_dt) { Util::Dump::ep(ep_descr); }
		} catch (Usb::Session::Interface_not_found) {
			error("could not read endpoint descriptor");
			return;
		}

		iface.state = Iface::State::ENABLED;
	}

	void handle_config_packet(Packet_descriptor &) { _claim_device(); }

	void handle_ctrl(Iface *iface, Packet_descriptor &p)
	{
		uint8_t const * const data = (uint8_t*)_usb->source()->packet_content(p);
		size_t           const len = p.control.actual_size > 0
		                           ? p.control.actual_size : 0;

		if (iface && (iface->state == Iface::State::CONFIGURE_PENDING)) {
			iface->state = Iface::State::CONFIGURED;
		}

		(void)data;
		(void)len;

		// TODO
	}

	void handle_isoc_packet(Iface &iface, Packet_descriptor &p)
	{
		if (!p.read_transfer()) {

			if (iface.state == Iface::State::USEABLE
			   || iface.state == Iface::State::ACTIVE) {
				iface.transfer(*this, _packet_queue, _samples, BPT);
			}
			return;
		}

		uint8_t const * const data = (uint8_t*)_usb->source()->packet_content(p);
		size_t           const len = p.transfer.actual_size > 0
		                           ? p.transfer.actual_size : 0;

		(void)data;
		(void)len;

		// TODO
	}

	void handle_irq_packet(Packet_descriptor &p)
	{
		if (!p.read_transfer()) { return; }

		uint8_t const * const data = (uint8_t*)_usb->source()->packet_content(p);
		size_t           const len = p.transfer.actual_size > 0
		                           ? p.transfer.actual_size : 0;

		(void)data;
		(void)len;

		// TODO
	}

	struct String_descr
	{
		Usb::Audio         &_audio;
		char const * const  _name;

		enum { MAX_STRING_LENGTH = 128, };
		char string[MAX_STRING_LENGTH] { };

		uint8_t index = 0xff; /* hopefully invalid */

		String_descr(Usb::Audio &audio, char const *name)
		: _audio { audio } , _name { name } { }

		void request(uint8_t i)
		{
			index = i;

			Usb::Packet_descriptor p = _audio.alloc_packet(MAX_STRING_LENGTH);

			p.type          = Usb::Packet_descriptor::STRING;
			p.string.index  = index;
			p.string.length = MAX_STRING_LENGTH;

			_audio.submit_packet(p);
		}

		char const *name() const { return _name; }
	};

	void handle_string_packet(Usb::Packet_descriptor &p)
	{
		String_descr *s = nullptr;
		if (p.string.index == manufactorer_string.index) {
			s = &manufactorer_string;
		} else if (p.string.index == product_string.index) {
			s = &product_string;
		} else if (p.string.index == serial_number_string.index) {
			s = &serial_number_string;
		}

		if (!s) { return; }

		uint16_t const * const u = (uint16_t*)_usb->source()->packet_content(p);

		int const len = min((unsigned)p.string.length,
		                    (unsigned)String_descr::MAX_STRING_LENGTH - 1);

		for (int i = 0; i < len; i++) { s->string[i] = u[i] & 0xff; }
		s->string[len] = 0;

		log(s->name(), ": ", (char const*)s->string);
	}

	String_descr manufactorer_string  { *this, "Manufactorer" };
	String_descr product_string       { *this, "Product" };
	String_descr serial_number_string { *this, "Serial_number" };

	enum State { INVALID, PARSE_CONFIG, SET_INTERFACE, SET_SPEED, COMPLETE };
	State _state { State::INVALID };

	struct Completion : Usb::Completion
	{
		Iface *iface { nullptr };

		enum State { VALID, FREE, CANCELED };
		State state { FREE };

		void complete(Usb::Packet_descriptor &) override { }

		void complete(Usb::Audio &audio, Usb::Packet_descriptor &p)
		{
			if (state != VALID)
				return;

			if (!p.succeded) {
				error("packet failed: ", p);
				return;
			}

			switch (p.type) {
			case Usb::Packet_descriptor::ISOC:
				audio.handle_isoc_packet(*iface, p);
				break;
			case Usb::Packet_descriptor::CTRL:
				audio.handle_ctrl(iface, p);
				break;
			case Usb::Packet_descriptor::STRING:
				audio.handle_string_packet(p);
				break;
			case Usb::Packet_descriptor::CONFIG:
				audio.handle_config_packet(p);
				break;
			case Usb::Packet_descriptor::ALT_SETTING:
				audio.handle_alt_setting(*iface, p);
				break;
			/* ignore other packets */
			case Usb::Packet_descriptor::BULK:
			case Usb::Packet_descriptor::IRQ:
			case Usb::Packet_descriptor::RELEASE_IF:
				break;
			}
		}
	} completions[MAX_TRANSFERS];

	void _handle_interface(Iface &iface)
	{
		switch (iface.state) {
		case Iface::State::ENABLED:
			Genode::log("freq: ",   (unsigned)FREQ,   " "
			            "period: ", (unsigned)PERIOD, " ms "
			            "bytes: ",  (unsigned)BPT);
			iface.configure(*this, FREQ);
			break;
		case Iface::State::CONFIGURED:
			Genode::log("start transmitting");
			iface.transfer(*this, _packet_queue, _samples, BPT);
			break;
		default:
			break;
		}
	}

	void _ack_avail()
	{
		while (_usb->source()->ack_avail()) {
			Usb::Packet_descriptor p = _usb->source()->get_acked_packet();
			dynamic_cast<Completion *>(p.completion)->complete(*this, p);
			free_packet(p);
		}

		_handle_interface(_playback);
	}

	Signal_handler<Audio> _ack_avail_sigh { _env.ep(), *this, &Audio::_ack_avail };

	/* hardcoded NewBee */
	Iface _playback { 1, 1, 1, Iface::State::DISABLED };
	Iface _record   { 2, 1, 2, Iface::State::DISABLED };

	void _probe_device()
	{
		try {
			_usb->config_descriptor(&device_descr, &config_descr);
		} catch (Usb::Session::Device_not_found) {
			error("cound not read config descriptor");
			throw -1;
		}

		Usb::Packet_descriptor p = alloc_packet(0);

		p.type   = Packet_descriptor::CONFIG;
		p.number = 1; /* XXX read from device */

		_usb->source()->submit_packet(p);
	}

	bool _claim_device()
	{
		try {
			_usb->config_descriptor(&device_descr, &config_descr);
			if (dump_dt) { Util::Dump::device(device_descr); }
		} catch (Usb::Session::Device_not_found) {
			error("cound not read config descriptor");
			return false;
		}

		/* batch information gathering */
		if (device_descr.manufactorer_index) {
			manufactorer_string.request(device_descr.manufactorer_index);
		}
		if (device_descr.product_index) {
			product_string.request(device_descr.product_index);
		}
		if (device_descr.serial_number_index) {
			serial_number_string.request(device_descr.serial_number_index);
		}

		_playback.enable(*this);

		return true;
	}

	Completion *_alloc_completion()
	{
		for (unsigned i = 0; i < Usb::Session::TX_QUEUE_SIZE; i++)
			if (completions[i].state == Completion::FREE) {
				completions[i].state = Completion::VALID;
				return &completions[i];
			}

		return nullptr;
	}

	struct Queue_full         : Genode::Exception { };
	struct No_completion_free : Genode::Exception { };

	/*****************************
	 ** Packet_stream interface **
	 *****************************/

	char *content(Usb::Packet_descriptor &p) override
	{
		return reinterpret_cast<char*>(_usb->source()->packet_content(p));
	}

	Usb::Packet_descriptor alloc(size_t length) override
	{
		if (!_usb->source()->ready_to_submit()) {
			throw Queue_full();
		}

		Usb::Packet_descriptor p = _usb->source()->alloc_packet(length);

		p.completion = _alloc_completion();
		if (!p.completion) {
			_usb->source()->release_packet(p);
			throw No_completion_free();
		}

		return p;
	}

	void submit(Usb::Packet_descriptor &p) override
	{
		_usb->source()->submit_packet(p);
	}

	void free(Usb::Packet_descriptor &p) override
	{
		Completion *c = dynamic_cast<Completion *>(p.completion);
		if (c) {
			c->state = Completion::FREE;
			c->iface = nullptr;
		}
		_usb->source()->release_packet(p);
	}

	template<typename T>
	T *packet_content(Usb::Packet_descriptor &p)
	{
		return reinterpret_cast<T*>(_usb->source()->packet_content(p));
	}

	Usb::Packet_descriptor alloc_packet(size_t length)
	{
		if (!_usb->source()->ready_to_submit()) {
			throw Queue_full();
		}

		Usb::Packet_descriptor usb_packet = _usb->source()->alloc_packet(length);

		usb_packet.completion = _alloc_completion();
		if (!usb_packet.completion) {
			_usb->source()->release_packet(usb_packet);
			throw No_completion_free();
		}

		return usb_packet;
	}

	void submit_packet(Usb::Packet_descriptor &p)
	{
		_usb->source()->submit_packet(p);
	}

	void free_packet(Usb::Packet_descriptor &packet)
	{
		Completion *c = dynamic_cast<Completion *>(packet.completion);
		if (c) {
			c->state = Completion::FREE;
			c->iface = nullptr;
		}
		_usb->source()->release_packet(packet);
	}

	Audio(Env &env, Genode::Allocator &alloc)
	:
		_env { env }, _usb_alloc { &alloc }
	{
		Genode::log("USB audio driver started");
		_usb_devices_rom.sigh(_usb_devices_sigh);
		_handle_usb_devices_report();
	}
};


struct Usb::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Usb::Audio _audio_drv { _env, _heap };

	Main(Env &env) : _env { env } { }
};


void Component::construct(Genode::Env &env)
{
	static Usb::Main main(env);
}

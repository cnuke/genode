/*
 * \brief  Oscilloscope showing audio input
 * \author Norman Feske
 * \date   2023-01-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <gui_session/connection.h>
#include <audio_in_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb888.h>
#include <polygon_gfx/line_painter.h>
#include <gems/gui_buffer.h>

#include <audio_in_session/rpc_object.h>
#include <root/component.h>
#include <base/heap.h>

namespace Osci {
	using namespace Genode;
	struct Main;
}


/***************
 ** Recording **
 ***************/

namespace Audio_in {

	class Session_component;
	class In;
	class Root;
	struct Root_policy;
	static Session_component *channel_acquired;
	enum Channel_number { LEFT, MAX_CHANNELS, INVALID = MAX_CHANNELS };

	using namespace Genode;
}


class Audio_in::Session_component : public Audio_in::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Genode::Env &env, Channel_number channel)
		: Session_rpc_object(env, Signal_context_capability()),
		  _channel(channel)
		{ channel_acquired = this; }

		~Session_component() { channel_acquired = nullptr; }
};


class Audio_in::In
{
	private:

		bool _active() { return channel_acquired && channel_acquired->active(); }

		Stream *stream() { return channel_acquired->stream(); }

	public:

		static bool channel_number(const char     *name,
		                           Channel_number *out_number)
		{
			static struct Names {
				const char     *name;
				Channel_number  number;
			} names[] = {
				{ "left", LEFT },
				{ 0, INVALID }
			};

			for (Names *n = names; n->name; ++n)
				if (!Genode::strcmp(name, n->name)) {
					*out_number = n->number;
					return true;
				}

			return false;
		}

		void record_packet(Audio_in::Packet &packet)
		{
			if (!_active()) return;
			/*
			 * Check for an overrun first and notify the client later.
			 */
			bool overrun = stream()->overrun();

			Packet *p = stream()->alloc();

			float       * const dst = p->content();
			float const * const src = packet.content();
			memcpy(dst, src, p->size());

			stream()->submit(p);

			channel_acquired->progress_submit();

			if (overrun) channel_acquired->overrun_submit();
		}
};


struct Audio_in::Root_policy
{
	void aquire(char const *args)
	{
		size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		size_t session_size = align_addr(sizeof(Audio_in::Session_component), 12);

		if ((ram_quota < session_size) ||
		    (sizeof(Stream) > (ram_quota - session_size))) {
			Genode::error("insufficient 'ram_quota', got ", ram_quota,
			              " need ", sizeof(Stream) + session_size,
			              ", denying '",Genode::label_from_args(args),"'");
			throw Genode::Insufficient_ram_quota();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!In::channel_number(channel_name, &channel_number)) {
			Genode::error("invalid input channel '",(char const *)channel_name,"' requested, "
			              "denying '",Genode::label_from_args(args),"'");
			throw Genode::Service_denied();
		}
		if (Audio_in::channel_acquired) {
			Genode::error("input channel '",(char const *)channel_name,"' is unavailable, "
			              "denying '",Genode::label_from_args(args),"'");
			throw Genode::Service_denied();
		}
	}

	void release() { }
};


namespace Audio_in {
	using namespace Genode;

	typedef Root_component<Session_component, Root_policy> Root_component;
}


/**
 * Root component, handling new session requests.
 */
class Audio_in::Root : public Audio_in::Root_component
{
	private:

		Genode::Env               &_env;

	protected:

		Session_component *_create_session(char const *args) override
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			In::channel_number(channel_name, &channel_number);
			return new (md_alloc()) Session_component(_env, channel_number);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc)
		: Root_component(env.ep(), md_alloc), _env(env) { }
};



struct Osci::Main
{
	Env &_env;

	using Point = Gui_buffer::Point;
	using Area  = Gui_buffer::Area;
	using Rect  = Gui_buffer::Rect;

	Area  _size       { };
	Color _background { };
	Color _color      { };
	int   _v_scale    { };

	Gui::Connection _gui { _env };

	Timer::Connection _timer { _env };

	Audio_in::Connection _audio_in { _env, "left" };

	Constructible<Gui_buffer> _gui_buffer { };

	struct View
	{
		Gui::Connection &_gui;

		Gui::Session::View_handle _handle { _gui.create_view() };

		View(Gui::Connection &gui, Point position, Area size) : _gui(gui)
		{
			using Command = Gui::Session::Command;
			_gui.enqueue<Command::Geometry>(_handle, Rect(position, size));
			_gui.enqueue<Command::To_front>(_handle, Gui::Session::View_handle());
			_gui.execute();
		}

		~View() { _gui.destroy_view(_handle); }
	};

	Constructible<View> _view { };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	struct Captured_audio
	{
		enum { SIZE_LOG2 = 10, SIZE = 1 << SIZE_LOG2, MASK = SIZE - 1 };

		float _samples[SIZE] { };

		unsigned _pos = 0;

		void _insert(float value)
		{
			_pos = (_pos + 1) & MASK;
			_samples[_pos] = value;
		}

		float past_value(unsigned past) const
		{
			return _samples[(_pos - past) & MASK];
		}

		void capture_from_audio_in(Audio_in::Session &audio_in)
		{
			Audio_in::Stream &stream = *audio_in.stream();

			while (!stream.empty()) {

				Audio_in::Packet &p = *stream.get(stream.pos());

				if (p.valid()) {
					float *data_ptr = p.content();

					for (unsigned i = 0; i < Audio_in::PERIOD; i++)
						_insert(*data_ptr++);

					p.invalidate();
					p.mark_as_recorded();
				}

				stream.increment_position();
			}
		}
	} _captured_audio { };

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_size       = Area::from_xml(config);
		_background = config.attribute_value("background", Color { 0, 0, 0 });
		_color      = config.attribute_value("color",      Color { 255, 255, 255 });
		_v_scale    = config.attribute_value("v_scale",    3000);

		_gui_buffer.construct(_gui, _size, _env.ram(), _env.rm(),
		                      Gui_buffer::Alpha::OPAQUE, _background);

		_view.construct(_gui, Point::from_xml(config), _size);

		// _timer.trigger_periodic(1000*config.attribute_value("period_ms",  20));
	}

	Line_painter const _line_painter { };

	void _render(Gui_buffer::Pixel_surface &pixel, Gui_buffer::Alpha_surface &)
	{
		/*
		 * Draw captured audio from right to left.
		 */

		Point const centered { 0, int(pixel.size().h()/2) };

		Point previous_p { };

		bool first_iteration = true;

		unsigned const w = pixel.size().w();

		for (unsigned i = 0; i < w; i++) {

			Point p { int(w - i),
			          int(float(_v_scale)*_captured_audio.past_value(i)) };

			p = p + centered;

			if (!first_iteration)
				_line_painter.paint(pixel, p, previous_p, _color);

			previous_p = p;
			first_iteration = false;
		}
	}

	void _handle_timer()
	{
		_captured_audio.capture_from_audio_in(_audio_in);

		_gui_buffer->reset_surface();
		_gui_buffer->apply_to_surface([&] (auto &pixel, auto &alpha) {
			_render(pixel, alpha); });

		_gui_buffer->flush_surface();

		_gui.framebuffer()->refresh(0, 0, _size.w(), _size.h());
	}

	Genode::Heap   _heap { _env.ram(), _env.rm() };
	Audio_in::In   in { };
	Audio_in::Root in_root { _env, _heap };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		env.parent().announce(env.ep().manage(in_root));

		// _timer.sigh(_timer_handler);

		_audio_in.progress_sigh(_timer_handler);
		_audio_in.start();
	}
};


void Component::construct(Genode::Env &env)
{
	static Osci::Main main(env);
}

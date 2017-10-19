/*
 * \brief  Genode gfxinit driver
 * \author Josef Soentgen
 * \date   2017-10-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <blit/blit.h>
#include <framebuffer_session/framebuffer_session.h>
#include <io_mem_session/connection.h>
#include <platform_device/client.h>
#include <root/component.h>
#include <timer_session/connection.h>
#include <util/arg_string.h>
#include <util/reconstructible.h>


/**
 * Make the linker happy
 */
extern "C" void __gnat_eh_personality()
{
	Genode::warning(__func__, " not implemented");
}


extern "C" void __gnat_last_chance_handler(char *source_location, int line)
{
	Genode::warning(__func__, " not implemented");
}


extern "C" void __gnat_rcheck_CE_Invalid_Data(char *source_location, int line)
{
	Genode::warning(__func__, " not implemented");
}


namespace Libhwbase {
	void init(Genode::Env&);
	bool handle_io_port(unsigned short, unsigned int);
	Platform::Device_capability pci_dev_cap();
}


static Genode::Dataspace_capability _screen_dataspace(Genode::Env &env)
{
	Platform::Device_capability cap = Libhwbase::pci_dev_cap();

	Platform::Device_client device(cap);

	enum { GMADR = 2, };
	Genode::size_t const screen_bytes = 1920*1080*4;

	Genode::uint8_t const vr = device.phys_bar_to_virt(GMADR);
	Genode::Io_mem_session_capability mem_cap =
		device.io_mem(vr, Genode::Cache_attribute::WRITE_COMBINED, 0, screen_bytes);

	static Genode::Io_mem_session_client mem(mem_cap);
	Genode::Dataspace_capability ds_cap = mem.dataspace();

	{
		void *dest = env.rm().attach(ds_cap);
		Genode::memset(dest, 0, screen_bytes);
		env.rm().detach(dest);
	}

	return ds_cap;
}


/*
 * Framebuffer
 */

namespace Framebuffer {
	struct Session_component;
	struct Root;
	struct Main;

	using Genode::size_t;
	using Genode::min;
	using Genode::max;
	using Genode::Allocator;
	using Genode::Attached_ram_dataspace;
	using Genode::Attached_rom_dataspace;
	using Genode::Dataspace_capability;
	using Genode::Env;
	using Genode::Heap;
}


class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Env &_env;

		Timer::Connection _timer { _env };

		unsigned const _width, _height, _depth;

		/* dataspace of physical frame buffer */
		Dataspace_capability  _fb_cap;
		void                 *_fb_addr;

		/* dataspace uses a back buffer (if '_buffered' is true) */
		Genode::Constructible<Attached_ram_dataspace> _bb;

		void _refresh_buffered(int x, int y, int w, int h)
		{
			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, (int)_width  - 1);
			int y2 = min(y + h - 1, (int)_height - 1);
			int x1 = max(x, 0);
			int y1 = max(y, 0);

			if (x1 > x2 || y1 > y2) { return; }

			/* bytes per pixel */
			int const bypp = 2;

			/* copy pixels from back buffer to physical frame buffer */
			char const *src = _bb->local_addr<char>() + bypp*(_width*y1 + x1);
			char       *dst = (char *)_fb_addr + bypp*(_width*y1 + x1);

			blit(src, bypp*_width, dst, bypp*_width,
			     bypp*(x2 - x1 + 1), y2 - y1 + 1);
		}

		bool _buffered() const { return _bb.constructed(); }

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env &env,
		                  unsigned width,
		                  unsigned height,
		                  unsigned depth,
		                  Dataspace_capability fb_cap,
		                  bool buffered)
		:
			_env(env),
			_width(width), _height(height), _depth(depth),
			_fb_cap(fb_cap)
		{
			if (!buffered) return;

			if (_depth != 16) {
				Genode::warning("buffered mode not supported for depth ", _depth);
				return;
			}

			try {
				size_t const bb_size = _width * _height * _depth / 8;
				_bb.construct(env.ram(), env.rm(), bb_size);
			} catch (...) {
				Genode::warning("could not allocate back buffer, disabled buffered output");
				return;
			}

			_fb_addr = env.rm().attach(_fb_cap);

			Genode::log("Using buffered output");
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			if (_buffered()) {
				_bb.destruct();
				_env.rm().detach(_fb_addr);
			}
		}


		/***********************************
		 ** Framebuffer session interface **
		 ***********************************/

		Dataspace_capability dataspace() override
		{
			return _buffered() ? Dataspace_capability(_bb->cap())
			                   : Dataspace_capability(_fb_cap);
		}

		Mode mode() const override
		{
			return Mode(_width, _height, _depth == 16 ? Mode::RGB565
			                                          : Mode::INVALID);
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (_buffered()) { _refresh_buffered(x, y, w, h); }
		}
};


/**
 * Shortcut for single-client root component
 */
typedef Genode::Root_component<Framebuffer::Session_component,
                               Genode::Single_client> Root_component;

class Framebuffer::Root : public Root_component
{
	private:

		Env &_env;

		Attached_rom_dataspace const &_config;

		Genode::Dataspace_capability _screen_cap;

		unsigned _session_arg(char const *attr_name, char const *args,
		                      char const *arg_name, unsigned default_value)
		{
			/* try to obtain value from config file */
			unsigned result = _config.xml().attribute_value(attr_name,
			                                                default_value);

			/* check session argument to override value from config file */
			return Genode::Arg_string::find_arg(args, arg_name).ulong_value(result);
		}

	protected:

		Session_component *_create_session(char const *args) override
		{
			unsigned       width  = _session_arg("width",  args, "fb_width",  1920);
			unsigned       height = _session_arg("height", args, "fb_height", 1080);
			unsigned const depth  = _session_arg("depth",  args, "fb_mode",   16);

			bool const buffered = _config.xml().attribute_value("buffered", false);

			// if (Framebuffer::set_mode(width, height, depth) != 0) {
			// 	Genode::warning("could not set mode ",
			// 	                width, "x", height, "@", depth);
			// 	throw Genode::Service_denied();
			// }

			Genode::log("Using mode: ", width, "x", height, "@", depth);

			return new (md_alloc())
				Session_component(_env, width, height, depth,
				                  _screen_cap, buffered);
		}

	public:

		Root(Env &env, Allocator &alloc,
		     Attached_rom_dataspace const &config)
		:
			Root_component(&env.ep().rpc_ep(), &alloc),
			_env(env), _config(config),
			_screen_cap(_screen_dataspace(env))
		{ }
};


struct Framebuffer::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Root root { _env, _heap, _config_rom };

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(root));
	}
};


// Ada
extern "C" int hw__gfx__gma__gfx__initialize();


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	Libhwbase::init(env);

	enum { VGA_SR_IDX = 0x3c4, VGA_SR_DAT = 0x3c5, };
	if (!Libhwbase::handle_io_port(VGA_SR_IDX, 1) ||
	    !Libhwbase::handle_io_port(VGA_SR_DAT, 1)) {
		Genode::error("could not initialize libhwbase");
		env.parent().exit(1);
		return;
	}

	int const err = hw__gfx__gma__gfx__initialize();
	if (err) {
		Genode::error("could not initialize GPU");
		env.parent().exit(err);
		return;
	}

	static Framebuffer::Main main(env);
}

/*
 * \brief  Dummy Framebuffer driver
 * \author Josef Soentgen
 * \date   2016-07-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/rpc_server.h>
#include <framebuffer_session/framebuffer_session.h>
#include <os/static_root.h>
#include <timer_session/connection.h>


namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		size_t                 const _width;
		size_t                 const _height;
		Attached_ram_dataspace       _fb_mem;
		Timer::Connection            _timer;

	public:

		Session_component(Genode::Env &env, size_t width, size_t height)
		:
			_width(width), _height(height),
			_fb_mem(env.ram(), env.rm(), _width * _height * 2)
		{ }

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override {
			return _fb_mem.cap(); }

		Mode mode() const override {
			return Mode(_width, _height, Mode::RGB565); }

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override { }
};


struct Main
{
	Genode::Env &env;

	Framebuffer::Session_component session;

	Genode::Static_root<Framebuffer::Session> root;

	Main(Genode::Env &env)
	:
		env(env),
		session(env, 1024, 768),
		root(env.ep().manage(session))
	{
		env.parent().announce(env.ep().manage(root));
	}
};


Genode::size_t Component::stack_size() { return 2*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main main(env); }

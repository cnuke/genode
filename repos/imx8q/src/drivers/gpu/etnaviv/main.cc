/*
 * \brief  i.MX8 framebuffer driver Linux port
 * \author Stefan Kalkowski
 * \date   2021-03-08
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/component.h>
#include <lx_kit/init.h>
#include <lx_kit/env.h>

namespace Framebuffer {
	using namespace Genode;
	struct Main;
}


#include <util/reconstructible.h>
#include <gui_session/connection.h>
#include <os/pixel_rgb888.h>

using uint8_t = Genode::uint8_t;

class Viewer
{
	private:

		Viewer(const Viewer &) = delete;
		const Viewer& operator=(const Viewer&) = delete;

		using PT = Genode::Pixel_rgb888;

		Genode::Env               &_env;
		Gui::Connection            _gui  { _env, "gpu" };
		Gui::Session::View_handle  _view { _gui.create_view() };
		Framebuffer::Mode const    _mode;

		Genode::Constructible<Genode::Attached_dataspace> _fb_ds { };
		uint8_t *_framebuffer { nullptr };

	public:

		Viewer(Genode::Env &env, Framebuffer::Mode mode)
		:
			_env    { env },
			_mode   { mode }
		{
			_gui.buffer(mode, false);

			_fb_ds.construct(_env.rm(), _gui.framebuffer()->dataspace());
			_framebuffer = _fb_ds->local_addr<uint8_t>();

			using Command = Gui::Session::Command;
			using namespace Gui;

			_gui.enqueue<Command::Geometry>(_view, Gui::Rect(Gui::Point(0, 0), _mode.area));
			_gui.enqueue<Command::To_front>(_view, Gui::Session::View_handle());
			_gui.enqueue<Command::Title>(_view, "webcam");
			_gui.execute();
		}

		uint8_t *framebuffer() { return _framebuffer; }

		void refresh() {
			_gui.framebuffer()->refresh(0, 0, _mode.area.w(), _mode.area.h());
		}

		Framebuffer::Mode const &mode() { return _mode; }
};


Viewer *viewer()
{
	static Genode::Constructible<Viewer> _inst { };
	if (!_inst.constructed()) {
		_inst.construct(Lx_kit::env().env, Framebuffer::Mode { .area = { 600, 600 } });
	}

	if (_inst.constructed()) {
		return &*_inst;
	}

	return nullptr;
}


template <typename FN> void with_constructed_viewer(FN const &func)
{
	Viewer *v = viewer();
	if (v) {
		func(*v);
	}
}


extern "C" void *viewer_fb_addr()
{
	void *fb = nullptr;
	with_constructed_viewer([&] (Viewer &v) {
		fb = v.framebuffer();
	});
	return fb;
}


extern "C" void viewer_refresh(void)
{
	with_constructed_viewer([&] (Viewer &v) {
		v.refresh();
	});
}


struct Framebuffer::Main
{
	Env & env;

	Heap  heap { env.ram(), env.rm() };

	Main(Env & env) : env(env)
	{
		Lx_kit::initialize(env, heap);
	}
};


void Component::construct(Genode::Env &env)
{
	static Framebuffer::Main main(env);
}

/*
 * \brief  Virtualized GUI session interface exposed to the subsystem
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GUI_H_
#define _GUI_H_

/* Genode includes */
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <base/component.h>
#include <base/attached_ram_dataspace.h>
#include <gui_session/connection.h>

/* local includes */
#include <input.h>

namespace Gui {
	using namespace Genode;
	class Session_component;
}


class Gui::Session_component : public Rpc_object<Session>
{
	private:

		/**
		 * Signal handler to be notified once the geometry of the view is
		 * known.
		 */
		Signal_context_capability _view_ready_sigh;

		Entrypoint &_ep;

		Area _max_size;

		Gui::Connection _gui;

		View_handle _parent_view_handle;

		/*
		 * Physical view
		 */
		View_handle _view_handle;
		Rect        _view_geometry { };
		Point       _view_offset   { };

		/*
		 * Geometry of virtual view presented to the loaded subsystem
		 */
		Rect  _virt_view_geometry { };
		Point _virt_view_offset   { };
		bool  _virt_view_geometry_defined = false;

		Input::Motion_delta _motion_delta { };

		Input::Session_component _proxy_input;

		static long _session_arg(const char *arg, const char *key) {
			return Arg_string::find_arg(arg, key).long_value(0); }

		/*
		 * Command buffer
		 */
		typedef Gui::Session::Command_buffer Command_buffer;
		Attached_ram_dataspace _command_ds;
		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		void _propagate_view_offset()
		{
			_gui.enqueue<Command::Offset>(_view_handle,
			                              _view_offset + _virt_view_offset);
		}

		void _update_motion_delta()
		{
			_motion_delta = _virt_view_geometry.p1() - _view_geometry.p1();
		}

		void _execute_command(Command const &command)
		{
			switch (command.opcode) {

			case Command::OP_GEOMETRY:
				{
					_virt_view_geometry = command.geometry.rect;

					if (!_virt_view_geometry_defined)
						Signal_transmitter(_view_ready_sigh).submit();

					_virt_view_geometry_defined = true;

					_update_motion_delta();
					return;
				}

			case Command::OP_OFFSET:
				{
					_virt_view_offset = command.offset.offset;
					_propagate_view_offset();
					_gui.execute();
					return;
				}

			case Command::OP_TO_FRONT:
				{
					_gui.enqueue<Command::To_front>(_view_handle, _parent_view_handle);
					_gui.execute();
					return;
				}

			case Command::OP_TO_BACK:
				{
					warning("OP_TO_BACK not implemented");
					return;
				}

			case Command::OP_BACKGROUND:
				{
					warning("OP_BACKGROUND not implemented");
					return;
				}

			case Command::OP_TITLE:
				{
					_gui.enqueue(command);
					_gui.execute();
					return;
				}

			case Command::OP_NOP:
				return;
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Entrypoint                &ep,
		                  Env                       &env,
		                  Region_map                &rm,
		                  Ram_allocator             &ram,
		                  Area                       max_size,
		                  Gui::View_capability       parent_view,
		                  Signal_context_capability  view_ready_sigh,
		                  char const                * /* args */)
		:
			_view_ready_sigh(view_ready_sigh),
			_ep(ep),
			_max_size(max_size),
			_gui(env),

			/* import parent view */
			_parent_view_handle(_gui.view_handle(parent_view)),

			/* create GUI view */
			_view_handle(_gui.create_view(_parent_view_handle)),

			_proxy_input(rm, _gui.input_session(), _motion_delta),

			_command_ds(ram, env.rm(), sizeof(Command_buffer))
		{
			_ep.manage(_proxy_input);
			_ep.manage(*this);
		}

		~Session_component()
		{ 
			_ep.dissolve(_proxy_input);
			_ep.dissolve(*this);
		}


		/***************************
		 ** GUI session interface **
		 ***************************/

		Framebuffer::Session_capability framebuffer_session() override
		{
			return _gui.framebuffer_session();
		}

		Input::Session_capability input_session() override
		{
			return _proxy_input.cap();
		}

		View_handle create_view(View_handle) override
		{
			return View_handle(1);
		}

		void destroy_view(View_handle) override { }

		View_handle view_handle(View_capability, View_handle) override
		{
			return View_handle();
		}

		View_capability view_capability(View_handle) override
		{
			return View_capability();
		}

		void release_view_handle(View_handle) override { }

		Dataspace_capability command_dataspace() override
		{
			return _command_ds.cap();
		}

		void execute() override
		{
			for (unsigned i = 0; i < _command_buffer.num(); i++)
				_execute_command(_command_buffer.get(i));
		}

		Framebuffer::Mode mode() override
		{
			unsigned const mode_width = _max_size.valid() ?
			                            _max_size.w() :
			                            _gui.mode().area.w();

			unsigned const mode_height = _max_size.valid() ?
			                             _max_size.h() :
			                             _gui.mode().area.h();

			return Framebuffer::Mode { .area = { mode_width, mode_height } };
		}

		void mode_sigh(Signal_context_capability) override { }

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			_gui.buffer(mode, use_alpha);
		}

		void focus(Capability<Session>) override { }

		/**
		 * Return geometry of loader view
		 */
		Area loader_view_size() const
		{
			int const  width = _max_size.valid()
			                 ? min(_virt_view_geometry.w(), _max_size.w())
			                 : _virt_view_geometry.w();

			int const height = _max_size.valid()
			                 ? min(_virt_view_geometry.h(), _max_size.h())
			                 : _virt_view_geometry.h();

			return Area(width, height);
		}

		/**
		 * Define geometry of loader view
		 */
		void loader_view_geometry(Rect rect, Point offset)
		{
			typedef Gui::Session::Command Command;

			_view_geometry = rect;
			_view_offset   = offset;

			_propagate_view_offset();
			_gui.enqueue<Command::Geometry>(_view_handle, _view_geometry);
			_gui.enqueue<Command::To_front>(_view_handle, _parent_view_handle);
			_gui.execute();

			_update_motion_delta();
		}
};

#endif /* _GUI_H_ */

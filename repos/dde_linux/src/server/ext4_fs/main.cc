/*
 * \brief  vfs_ext4 Link test
 * \author Josef Soentgen
 * \date   2016-04-26
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <file_system/node_handle_registry.h>
#include <file_system/util.h>
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <os/server.h>

/* Ext4 includes */
#include <ext4/ext4.h>
#include <ext4/directory.h>
#include <ext4/file.h>
#include <ext4/symlink.h>


namespace File_system {
	using namespace Genode;

	struct Root;
	struct Session_component;
	struct Main;
}


struct File_system::Session_component : public Session_rpc_object
{
	private:

		Server::Entrypoint &_ep;
		Genode::Allocator  &_alloc;

		Node_handle_registry  _handle_registry;

		/******************************
		 ** Packet-stream processing **
		 ******************************/

		struct Packet_completion : public Ext4::Completion
		{
			Packet_descriptor  packet;
			Tx::Sink          *sink { nullptr };

			void complete(Completion*, size_t length)
			{
				packet.length(length);
				packet.succeeded(!!length);

				sink->acknowledge_packet(packet);

				sink = nullptr;
			}
		};

		Packet_completion completion;

		void _process_packet_op(Packet_descriptor &packet, Node &node)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const offset  = packet.position();

			if (!content || (packet.length() > packet.size())) {
				packet.succeeded(false);
				return;
			}

			if (completion.sink) {
				PERR("Packet_completion already in use");
				throw -1;
			}

			completion.packet = packet;
			completion.sink   = tx_sink();

			switch (packet.operation()) {

				case Packet_descriptor::READ:
					node.read(&completion, (char *)content, length, offset);
					break;

				case Packet_descriptor::WRITE:
					node.write(&completion, (char const *)content, length, offset);
					break;
			}

			/* kick-off scheduler to make Ext4 task process our request */
			Ext4::schedule_task();
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			try {
				Node *node = _handle_registry.lookup(packet.handle());

				_process_packet_op(packet, *node);
			}
			catch (Invalid_handle)     { PERR("Invalid_handle");     }
			catch (Size_limit_reached) { PERR("Size_limit_reached"); }
		}

		void _process_packets(unsigned)
		{
			while (tx_sink()->packet_avail()) {

				if (!tx_sink()->ready_to_ack()) { return; }

				_process_packet();
			}
		}

		Signal_rpc_member<Session_component> _process_packet_dispatcher {
			_ep, *this, &Session_component::_process_packets };


		/***********************************
		 ** File system signal processing **
		 ***********************************/

		struct Fs_completion : public Ext4::Completion
		{
			File *file;
			int error;

			void complete(Completion *c, size_t res)
			{
				Fs_completion *fsc = dynamic_cast<Fs_completion*>(c);
				if (!fsc) {
					PERR("Completion invalid");
					return;
				}

				fsc->file = (File*)res;
			}
		};

	public:

		Session_component(Server::Entrypoint &ep,
		                  Genode::Allocator  &alloc,
		                  Genode::size_t     tx_buf_size,
		                  bool               writeable)
		:
			Session_rpc_object(Genode::env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_ep(ep), _alloc(alloc)
		{
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			Genode::env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string())) { throw Invalid_name(); }

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			Fs_completion completion;

			dir->file(&completion, name, mode, create);
			Ext4::schedule_task();

			PDBG("before");
			Server::wait_and_dispatch_one_signal();
			PDBG("after");
			if (!completion.file) {
				PERR("FOO");
				throw Invalid_name();
			}

			PINF("file: %p", completion.file);

			File *file = nullptr;
			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{
			Symlink *symlink = nullptr;
			return _handle_registry.alloc(symlink);
		}

		Dir_handle dir(Path const &path, bool create)
		{
			PDBG("path: '%s'", path.string());
			Directory *dir = Ext4::root_dir();
			return _handle_registry.alloc(dir);
		}

		Node_handle node(Path const &path)
		{
			Node *node = nullptr;
			return _handle_registry.alloc(node);
		}

		void close(Node_handle handle)
		{
			Node *node = nullptr;
			try { node = _handle_registry.lookup(handle); }
			catch (Invalid_handle) { return; }

			_handle_registry.free(handle);

			/* free all nodes but the root node */
			if (node &&
			    node != Ext4::root_dir()) {
			    destroy(&_alloc, node);
			}
		}

		Status status(Node_handle node_handle)
		{
			return Status();
		}

		void control(Node_handle, Control) { PLOG("%s not implemented", __func__); }

		void unlink(Dir_handle dir_handle, Name const &name)
		{
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle   to_dir_handle, Name const   &to_name)
		{
		}

		void sigh(Node_handle node_handle, Signal_context_capability sigh)
		{
		}

		void sync(Node_handle) override { PLOG("%s not implemented", __func__); }
};


struct File_system::Root : public Root_component<Session_component>
{
	private:

		Server::Entrypoint &_ep;
		Genode::Allocator  &_alloc;

	protected:

		Session_component *_create_session(const char *args)
		{
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			return new (&_alloc) Session_component(_ep, _alloc, tx_buf_size, false);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep        entrypoint
		 * \param alloc     meta-data allocator
		 * \param root_dir  root directory
		 */
		Root(Server::Entrypoint &ep, Allocator &alloc)
		:
			Root_component<Session_component>(&ep.rpc_ep(), &alloc),
			_ep(ep), _alloc(alloc)
		{ }
};


struct File_system::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap { env()->ram_session(), env()->rm_session() };

	Root fs_root { ep, sliced_heap };

	void handle_mounted(unsigned)
	{
		PINF("--- File system mounted successfully ---");
		env()->parent()->announce(ep.manage(fs_root));
	}

	Signal_rpc_member<Main> mounted_dispatcher {
		ep, *this, &Main::handle_mounted };

	Signal_transmitter fs_ready;

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		fs_ready.context(mounted_dispatcher);

		Ext4::init(ep, *Genode::env()->heap(), fs_ready);
	}
};


namespace Server {
	char const *name()             { return "ext4_fs_ep";               }
	size_t stack_size()            { return 8*1024*sizeof(long);        }
	void construct(Entrypoint &ep) { static File_system::Main main(ep); }
}

/*
 * \brief  Encrypted Block session to block session
 * \author Josef Soentgen
 * \date   2016-07-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/printf.h>
#include <block/component.h>
#include <block/driver.h>
#include <block_session/connection.h>
#include <util/list.h>

/* local includes */
#include <crypt.h>


class Driver : public Block::Driver
{
	private:

		Genode::Env &_env;

		Genode::Attached_rom_dataspace config_rom { _env, "config" };

		struct Request : Genode::List<Request>::Element
		{
			Block::Packet_descriptor server;
			Block::Packet_descriptor client;
			char * const             buffer;

			Request(Block::Packet_descriptor &server,
			        Block::Packet_descriptor &client,
			        char * const              buffer)
			: server(server), client(client), buffer(buffer) { }

			bool match(const Block::Packet_descriptor reply) const
			{
				return reply.operation()    == server.operation()    &&
				       reply.block_number() == server.block_number() &&
				       reply.block_count()  == server.block_count();
			}
		};

		enum {
			SLAB_SZ = Block::Session::TX_QUEUE_SIZE * sizeof(Request)
		};

		Genode::Tslab<Request, SLAB_SZ> _req_slab;
		Genode::List<Request>           _req_list;

		Genode::Allocator_avl      _block_alloc;
		Block::Connection          _block;
		Block::Session::Operations _block_ops;
		Genode::size_t             _block_size;
		Block::sector_t            _block_count;

		void _handle_request(Block::Packet_descriptor &p, Request *r)
		{
			if (p.operation() == Block::Packet_descriptor::READ) {
				/* decrypt */
				Crypt::process(p, r->buffer, _block.tx()->packet_content(p), false);

				/* copy decrypted block(s) back */
				Genode::memcpy(_block.tx()->packet_content(p), r->buffer,
				               _block_size * p.block_count());
			}

			ack_packet(p, true);
		}

		void _ack_avail()
		{
			/* ack_avail */
			while (_block.tx()->ack_avail()) {
				Block::Packet_descriptor p = _block.tx()->get_acked_packet();

				try {
					for (Request *r = _req_list.first(); r; r = r->next()) {
						if (r->match(p)) {
							_handle_request(p, r);
							_req_list.remove(r);
							Genode::destroy(&_req_slab, r);
							break;
						}
					}

				} catch (Block::Driver::Request_congestion) {
					PWRN("Request_congestion"); }

				_block.tx()->release_packet(p);
			}
		}

		Server::Signal_handler<Driver> _ack_avail_dispatcher {
			_env.ep(), *this, &Driver::_ack_avail };

		void _ready_to_submit() { }

		Server::Signal_handler<Driver> _ready_to_submit_dispatcher {
			_env.ep(), *this, &Driver::_ready_to_submit };

		/**
		 * Make new I/O request to backend session
		 *
		 * \param write   do write request
		 * \param nr      block number
		 * \param count   number of blocks
		 * \param buffer  buffer for packet content
		 * \param packet  original packet request
		 */
		void _io(bool write, Block::sector_t nr, Genode::size_t count,
		         char * const buffer,
		         Block::Packet_descriptor &packet)
		{
			Block::Packet_descriptor p;

			try {
				if (!_block.tx()->ready_to_submit()) {
					PWRN("Not ready to submit packet");
					throw Request_congestion();
				}

				Block::Packet_descriptor::Opcode const op = write
					? Block::Packet_descriptor::WRITE
					: Block::Packet_descriptor::READ;

				Genode::size_t const size    = _block_size * count;
				Block::Packet_descriptor tmp = _block.dma_alloc_packet(size);

				p = Block::Packet_descriptor(tmp, op, nr, count);

				Request *r = new (&_req_slab) Request(packet, p, buffer);
				_req_list.insert(r);

				if (write) {
					/* encrypt */
					Crypt::process(p, _block.tx()->packet_content(p), buffer, write);
				}

				_block.tx()->submit_packet(p);

			} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
				throw Request_congestion();
			} catch (Genode::Allocator::Out_of_memory) {
				if (p.size())
					_block.tx()->release_packet(p);
				throw Request_congestion();
			}
		}

	public:

		Driver(Genode::Env &env, Genode::Allocator &alloc)
		:
			_env(env),
			_req_slab(&alloc), _block_alloc(&alloc),
			_block(_env, &_block_alloc, 4 * 1024 * 1024)
		{
			_block.info(&_block_count, &_block_size, &_block_ops);

			PLOG("session block_count: %llu block_size: %zu",
			     _block_count, _block_size);

			_block.tx_channel()->sigh_ack_avail(_ack_avail_dispatcher);
			_block.tx_channel()->sigh_ready_to_submit(_ready_to_submit_dispatcher);

			try {
				Crypt::initialize(config_rom.xml(), _block_size);
			} catch (Crypt::Could_not_initialize) {
				throw;
			}
		}

		~Driver()
		{
			Crypt::cleanup();
		}


		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		Genode::size_t      block_size() { return _block_size;  }
		Block::sector_t    block_count() { return _block_count; }
		Block::Session::Operations ops() { return _block_ops;   }

		void read(Block::sector_t           block_number,
		          Genode::size_t            block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet)
		{
			if (!_block_ops.supported(Block::Packet_descriptor::READ))
				throw Io_error();

			_io(false, block_number, block_count, buffer, packet);
		}

		void write(Block::sector_t           block_number,
		           Genode::size_t            block_count,
		           char const               *buffer,
		           Block::Packet_descriptor &packet)
		{
			if (!_block_ops.supported(Block::Packet_descriptor::WRITE))
				throw Io_error();

			char * const b = const_cast<char* const>(buffer);
			_io(true, block_number, block_count, b, packet);
		}

		void sync() { }
};


struct Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		::Driver driver;

		Factory(Genode::Env &env, Genode::Allocator &alloc)
		: driver(env, alloc) { }

		~Factory() { }

		Block::Driver *create() { return &driver; }

		void destroy(Block::Driver *driver) { }
	} factory { env, heap };

	Block::Root root;

	Main(Genode::Env &env)
	: env(env), root(env.ep(), &heap, factory)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


Genode::size_t      Component::stack_size() { return 2*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main server(env);    }

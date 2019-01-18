/*
 * \brief  Example block service
 * \author Norman Feske
 * \date   2018-12-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <block/request_stream.h>
#include <block_session/connection.h>
#include <root/root.h>

namespace Cbe {

	using Tag                  = Genode::uint32_t;
	using Number_of_primitives = Genode::size_t;

	enum { BLOCK_SIZE = 4096u };

	struct Primitive
	{
		using Number = Genode::uint64_t;
		using Index  = Genode::uint64_t;

		enum class Operation : Genode::uint32_t { INVALID, READ, WRITE, SYNC };
		enum class Success   : Genode::uint32_t { FALSE, TRUE };

		Operation operation { Operation::INVALID };
		Success success     { Success::FALSE };
		Tag tag             { 0 };
		Number block_number { 0 };
		Index index         { 0 };

		bool valid() const { return operation != Operation::INVALID; }
		bool read()  const { return operation == Operation::READ; }
		bool write() const { return operation == Operation::WRITE; }
		bool sync()  const { return operation == Operation::SYNC; }
	};

	struct Data_block
	{
		Genode::addr_t base { 0 };
		Genode::size_t size { 0 };
	};

	struct Block_session_component;
	template <unsigned, Genode::size_t> struct Io;
	struct Splitter;
	template <unsigned> struct Request_pool;

	struct Main;

	using namespace Genode;
}


struct Cbe::Block_session_component : Rpc_object<Block::Session>,
                                      Block::Request_stream
{
	Entrypoint &_ep;

	static constexpr size_t BLOCK_SIZE = 4096;
	static constexpr size_t NUM_BLOCKS = 16;

	Block_session_component(Region_map               &rm,
	                        Dataspace_capability      ds,
	                        Entrypoint               &ep,
	                        Signal_context_capability sigh)
	:
		Request_stream(rm, ds, ep, sigh, BLOCK_SIZE), _ep(ep)
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	void info(Block::sector_t *count, size_t *block_size, Operations *ops) override
	{
		*count      = NUM_BLOCKS;
		*block_size = BLOCK_SIZE;
		*ops        = Operations();

		ops->set_operation(Block::Packet_descriptor::Opcode::READ);
		ops->set_operation(Block::Packet_descriptor::Opcode::WRITE);
	}

	void sync() override { }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
};


template <unsigned N, Genode::size_t BS>
struct Cbe::Io : Noncopyable
{
	struct Entry
	{
		Cbe::Primitive primitive { };
		Cbe::Data_block data { };

		Block::Packet_descriptor packet { };

		enum State { UNUSED, PENDING, IN_PROGRESS, COMPLETE } state { UNUSED };

		void print(Genode::Output &out) const
		{
			auto state_string = [](State state) {
				switch (state) {
				case State::UNUSED:      return "unused";
				case State::PENDING:     return "pending";
				case State::IN_PROGRESS: return "in_progress";
				case State::COMPLETE:    return "complete";
				}
				return "unknown";
			};
			Genode::print(out, "primitve.block_number: ", primitive.block_number,
			              " state: ", state_string(state));
		}
	};

	Entry    _entries[N]   {   };
	unsigned _used_entries { 0 };

	struct Geometry
	{
		Block::sector_t block_count { 0 };
		Genode::size_t  block_size  { 0 };
	};

	Geometry           _geom { };
	Block::Connection &_block;

	struct Fake_sync_primitive     { };
	struct Invalid_block_operation { };

	Block::Packet_descriptor _convert_from(Cbe::Primitive const &primitive)
	{
		auto operation = [] (Cbe::Primitive::Operation op) {
			switch (op) {
			case Cbe::Primitive::Operation::READ:  return Block::Packet_descriptor::READ;
			case Cbe::Primitive::Operation::WRITE: return Block::Packet_descriptor::WRITE;
			case Cbe::Primitive::Operation::SYNC:  throw Fake_sync_primitive();
			default:                               throw Invalid_block_operation();
			}
		};

		return Block::Packet_descriptor(_block.tx()->alloc_packet(BS),
		                                operation(primitive.operation),
		                                primitive.block_number,
		                                BS / _geom.block_size);
	}

	bool _equal_packets(Block::Packet_descriptor const &p1,
	                    Block::Packet_descriptor const &p2) const
	{
		return p1.block_number() == p2.block_number() && p1.operation() == p2.operation();
	}

	Io(Block::Connection &block) : _block(block)
	{
		Block::Session::Operations block_ops { };
		_block.info(&_geom.block_count, &_geom.block_size, &block_ops);

		if (_geom.block_size > BS) {
			Genode::error("back end block size must be equal to or be a multiple of ", BS);
			throw -1;
		}
	}

	bool acceptable() const
	{
		return _used_entries < N;
	}

	void submit_primitive(Primitive const &p, Data_block const &d)
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::UNUSED) {
				_entries[i].primitive = p;
				_entries[i].data      = d;
				_entries[i].state     = Entry::PENDING;

				_used_entries++;
				return;
			}
		}

		error("failed to accept request");
	}

	bool execute()
	{
		bool progress = false;

		/* first mark all finished I/O ops */
		while (_block.tx()->ack_avail()) {
			Block::Packet_descriptor packet = _block.tx()->get_acked_packet();

			for (unsigned i = 0; i < N; i++) {
				if (_entries[i].state != Entry::IN_PROGRESS) { continue; }
				if (!_equal_packets(_entries[i].packet, packet)) { continue; }

				if (_entries[i].primitive.read()) {
					void const * const src = _block.tx()->packet_content(packet);
					Genode::size_t    size = _entries[i].data.size;
					void      * const dest = reinterpret_cast<void*>(_entries[i].data.base);
					Genode::memcpy(dest, src, size);
				}

				_entries[i].state = Entry::COMPLETE;
				_entries[i].primitive.success = packet.succeeded() ? Primitive::Success::TRUE
				                                                   : Primitive::Success::FALSE;

				_block.tx()->release_packet(_entries[i].packet);
				progress = true;
			}
		}

		/* second submit new I/O ops */
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::PENDING) {

				if (!_block.tx()->ready_to_submit()) { break; }

				try {
					Block::Packet_descriptor packet = _convert_from(_entries[i].primitive);

					if (_entries[i].primitive.write()) {
						void const * const src = reinterpret_cast<void*>(_entries[i].data.base);
						Genode::size_t    size = _entries[i].data.size;
						void      * const dest = _block.tx()->packet_content(packet);
						Genode::memcpy(dest, src, size);
					}

					_entries[i].state  = Entry::IN_PROGRESS;
					_entries[i].packet = packet;

					_block.tx()->submit_packet(_entries[i].packet);
					progress = true;
				}
				catch (Fake_sync_primitive) {
					_entries[i].state = Entry::COMPLETE;
					_entries[i].primitive.success = Primitive::Success::TRUE;
					break;
				}
				catch (Invalid_block_operation) {
					_entries[i].state = Entry::COMPLETE;
					_entries[i].primitive.success = Primitive::Success::FALSE;
					break;
				}
				catch (Block::Session::Tx::Source::Packet_alloc_failed) { break; }
			}
		}

		return progress;
	}

	bool peek_completed_primitive()
	{
		for (unsigned i = 0; i < N; i++)
			if (_entries[i].state == Entry::COMPLETE)
				return true;

		return false;
	}

	Primitive take_completed_primitive()
	{
		Primitive p { };

		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::COMPLETE) {
				_entries[i].state = Entry::UNUSED;
				_used_entries--;

				p = _entries[i].primitive;
				break;
			}
		}
		return p;
	}
};


struct Cbe::Splitter
{
	Block::Request       _current_request   { };
	Primitive            _current_primitive { };
	Number_of_primitives _num_primitives    { 0 };

	bool request_acceptable() const
	{
		return !_current_request.operation_defined();
	}

	Number_of_primitives number_of_primitives(Block::Request const request) const
	{
		return request.count;
	}

	Number_of_primitives submit_request(Block::Request const &request, Tag const tag)
	{
		auto operation = [] (Block::Request::Operation op)
		{
			switch (op) {
			case Block::Request::Operation::READ:  return Primitive::Operation::READ;
			case Block::Request::Operation::WRITE: return Primitive::Operation::WRITE;
			case Block::Request::Operation::SYNC:  return Primitive::Operation::SYNC;
			default:                               return Primitive::Operation::INVALID;
			}
		};

		_current_primitive.tag          = tag;
		_current_primitive.block_number = request.block_number;
		_current_primitive.operation    = operation(request.operation);
		_current_primitive.index        = 0;

		_current_request = request;
		_num_primitives  = request.count;

		return _num_primitives;
	}

	Primitive peek_generated_primitive() const
	{
		return _current_primitive;
	}

	Primitive take_generated_primitive()
	{
		Primitive p { };

		if (_current_primitive.index < _num_primitives) {
			p = _current_primitive;
			_current_primitive.block_number++;
			_current_primitive.index++;

			if (_current_primitive.index == _num_primitives) {
				_current_primitive = Primitive { };
				_current_request   = Block::Request { };
			}
		}

		return p;
	}
};


template <unsigned N>
struct Cbe::Request_pool
{
	struct Entry
	{
		Block::Request       request    {   };
		Tag                  tag        { 0 };
		Number_of_primitives primitives { 0 };
		Number_of_primitives done       { 0 };

		enum State { UNUSED, PENDING, IN_PROGRESS, COMPLETE } state { State::UNUSED };

		bool unused()   const { return state == State::UNUSED; }
		bool pending()  const { return state == State::PENDING; }
		bool complete() const { return state == State::COMPLETE; }
	};

	Entry    _entries[N]   {   };
	unsigned _used_entries { 0 };

	Block::Request request_for_tag(Tag const tag) const
	{
		return _entries[tag].request;
	}

	bool acceptable() const { return _used_entries < N; }

	void submit_request(Block::Request request)
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].unused()) {

				_entries[i].request    = request;
				_entries[i].state      = Entry::State::PENDING;
				_entries[i].done       = 0;
				_entries[i].tag        = i;

				/* assume success, might be overriden in process_primitive */
				_entries[i].request.success = Block::Request::Success::TRUE;

				_used_entries++;
				break;
			}
		}
	}

	void set_primitive_count(Tag const tag, Number_of_primitives num)
	{
		_entries[tag].primitives = num;
	}

	bool peek_request_pending() const
	{
		for (unsigned i = 0; i < N; i++)
			if (_entries[i].pending())
				return true;

		return false;
	}

	Entry take_pending_request()
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].pending()) {
				_entries[i].state = Entry::State::IN_PROGRESS;
				return _entries[i];
			}
		}

		/* should never be reached */
		return Entry();
	}

	void mark_completed_primitive(Primitive const &p)
	{
		Tag const &tag = p.tag;

		if (p.success == Primitive::Success::FALSE &&
		    _entries[tag].request.success == Block::Request::Success::TRUE) {
			_entries[tag].request.success = Block::Request::Success::FALSE;
		}

		_entries[tag].done++;

		if (_entries[tag].done == _entries[tag].primitives)
			_entries[tag].state = Entry::State::COMPLETE;
	}

	Block::Request take_completed_request()
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].complete()) {
				_entries[i].state = Entry::State::UNUSED;
				_used_entries--;

				return _entries[i].request;
			}
		}

		/* should never be reached */
		return Block::Request { };
	}

	bool peek_completed_request()
	{
		for (unsigned i = 0; i < N; i++)
			if (_entries[i].complete())
				return true;

		return false;
	}
};


struct Cbe::Main : Rpc_object<Typed_root<Block::Session> >
{
	Env &_env;

	Constructible<Attached_ram_dataspace>  _block_ds { };
	Constructible<Block_session_component> _block_session { };

	enum {
		BLOCK_TX_BUFFER_SIZE = Block::Session::TX_QUEUE_SIZE * BLOCK_SIZE,
	};

	Heap              _heap        { _env.ram(), _env.rm() };
	Allocator_avl     _block_alloc { &_heap };
	Block::Connection _block       { _env, &_block_alloc, BLOCK_TX_BUFFER_SIZE };

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };

	using Pool = Request_pool<16>;

	Pool              _request_pool { };
	Splitter          _splitter     { };
	Io<1, BLOCK_SIZE> _io           { _block };

	Data_block _data_for_primitive(Block::Request_stream::Payload const &payload,
	                         Pool const &pool, Primitive const p)
	{
		Block::Request const client_req = pool.request_for_tag(p.tag);
		Block::Request request { };
		request.offset = client_req.offset + (p.index * BLOCK_SIZE);
		request.count  = 1;

		Data_block data { };
		payload.with_content(request, [&] (Genode::addr_t addr, Genode::size_t size) {
			data.base = addr;
			data.size = size;
		});

		return data;
	}

	void _handle_requests()
	{
		if (!_block_session.constructed())
			return;

		Block_session_component &block_session = *_block_session;
		Block::Request_stream::Payload payload = block_session.payload();

		for (;;) {

			bool progress = false;

			/* import new requests */
			block_session.with_requests([&] (Block::Request request) {

				if (!_request_pool.acceptable())
					return Block_session_component::Response::RETRY;

				_request_pool.submit_request(request);
				progress |= true;
				return Block_session_component::Response::ACCEPTED;
			});

			while (_request_pool.peek_request_pending()) {

				if (!_splitter.request_acceptable()) { break; }

					Pool::Entry e = _request_pool.take_pending_request();
					Number_of_primitives const num = _splitter.number_of_primitives(e.request);

					_request_pool.set_primitive_count(e.tag, num);
					_splitter.submit_request(e.request, e.tag);

					progress |= true;
			}

			while (_splitter.peek_generated_primitive().valid()) {

				if (!_io.acceptable()) { break; }

				Primitive p  = _splitter.take_generated_primitive();
				Data_block d = _data_for_primitive(payload, _request_pool, p);
				_io.submit_primitive(p, d);

				progress = true;
			}

			/* process I/O */
			progress |= _io.execute();

			/* acknowledge finished I/O jobs */
			while (_io.peek_completed_primitive()) {
				Primitive p = _io.take_completed_primitive();

				_request_pool.mark_completed_primitive(p);
				progress |= true;
			}

			/* acknowledge finished requests */
			block_session.try_acknowledge([&] (Block_session_component::Ack &ack) {

				if (_request_pool.peek_completed_request()) {
					Block::Request request = _request_pool.take_completed_request();
					ack.submit(request);
					progress |= true;
				}
			});

			if (!progress) { break; }
		}

		block_session.wakeup_client();
	}


	/*
	 * Root interface
	 */

	Capability<Session> session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		log("new block session: ", args.string());

		size_t const ds_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (ds_size >= ram_quota.value) {
			warning("communication buffer size exceeds session quota");
			throw Insufficient_ram_quota();
		}

		_block_ds.construct(_env.ram(), _env.rm(), ds_size);
		_block_session.construct(_env.rm(), _block_ds->cap(), _env.ep(),
		                         _request_handler);

		_block.tx_channel()->sigh_ack_avail(_request_handler);
		_block.tx_channel()->sigh_ready_to_submit(_request_handler);

		return _block_session->cap();
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session>) override
	{
		_block.tx_channel()->sigh_ack_avail(Signal_context_capability());
		_block.tx_channel()->sigh_ready_to_submit(Signal_context_capability());

		_block_session.destruct();
		_block_ds.destruct();
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Cbe::Main inst(env); }

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
#include <block/request_stream.h>
#include <base/component.h>
#include <base/attached_ram_dataspace.h>
#include <root/root.h>

namespace Cbe {

	using Tag                  = Genode::uint32_t;
	using Number_of_primitives = Genode::size_t;

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
	};

	struct Data_block
	{
		Genode::addr_t base { 0 };
		Genode::size_t size { 0 };
	};

	struct Block_session_component;
	template <unsigned> struct Io;
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


template <unsigned N>
struct Cbe::Io : Noncopyable
{
	struct Entry
	{
		Cbe::Primitive primitive { };
		Cbe::Data_block data { };

		enum State { UNUSED, IN_PROGRESS, COMPLETE } state { UNUSED };
	};

	Entry    _entries[N]   {   };
	unsigned _used_entries { 0 };

	bool acceptable() const { return _used_entries < N; }

	void submit_primitive(Primitive const &p, Data_block const &d)
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::UNUSED) {
				_entries[i].primitive = p;
				_entries[i].data      = d;
				_entries[i].state     = Entry::IN_PROGRESS;

				_used_entries++;
				return;
			}
		}

		error("failed to accept request");
	}

	bool execute()
	{
		bool progress = false;
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::IN_PROGRESS) {
				_entries[i].state = Entry::COMPLETE;

				void          *dest = reinterpret_cast<void*>(_entries[i].data.base);
				Genode::size_t size = _entries[i].data.size;
				Genode::memset(dest, 0x55, size);

				_entries[i].primitive.success = Primitive::Success::TRUE;
				progress = true;
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

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };

	using Pool = Request_pool<16>;

	enum { BLOCK_SIZE = 4096u };

	Pool      _request_pool { };
	Splitter  _splitter     { };
	Io<1>     _io           { };

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

			/* acknowledge finished jobs */
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

		return _block_session->cap();
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session>) override
	{
		_block_session.destruct();
		_block_ds.destruct();
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Cbe::Main inst(env); }

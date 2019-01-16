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

namespace Test {

	using Tag = Genode::uint64_t;
	using Number_of_primitives = Genode::size_t;

	struct Primitive
	{
		using Number = Genode::uint64_t;
		using Offset = Genode::uint64_t;

		enum class Operation : Genode::uint32_t { INVALID, READ, WRITE, SYNC };
		enum class Success   : Genode::uint32_t { FALSE, TRUE };

		Operation operation { Operation::INVALID };
		Success success     { Success::FALSE };
		Tag tag             { 0 };
		Number block_number { 0 };
		Offset offset       { 0 };

		bool valid() const { return operation != Operation::INVALID; }
	};

	struct Block_session_component;
	template <unsigned> struct Jobs;
	template <Genode::size_t> struct Splitter;

	struct Main;

	using namespace Genode;
}


struct Test::Block_session_component : Rpc_object<Block::Session>,
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
struct Test::Jobs : Noncopyable
{
	struct Entry
	{
		Test::Primitive primitive { };

		enum State { UNUSED, IN_PROGRESS, COMPLETE } state;
	};

	Entry _entries[N] { };

	bool acceptable(Primitive const &) const
	{
		for (unsigned i = 0; i < N; i++)
			if (_entries[i].state == Entry::UNUSED)
				return true;

		return false;
	}

	void submit(Primitive const &p)
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::UNUSED) {
				_entries[i] = Entry { .primitive = p,
				                      .state     = Entry::IN_PROGRESS };
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
				_entries[i].p.success = Primitive::Success::TRUE;
				progress = true;
			}
		}

		return progress;
	}

	void _completed_job(Primitive &out)
	{
		out = Primitive { };

		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::COMPLETE) {
				out = _entries[i].primitive;
				_entries[i].state = Entry::UNUSED;
				return;
			}
		}
	}

	/**
	 * Apply 'fn' with completed job, reset job
	 */
	template <typename FN>
	void with_any_completed_job(FN const &fn)
	{
		Primitive p { };

		_completed_job(p);

		if (p.valid())
			fn(p);
	}
};

template <Genode::size_t BS>
struct Test::Splitter
{
	Block::Request       _current_request   { };
	Primitive            _current_primitive { };
	Number_of_primitives _num_primitives    { 0 };
	Number_of_primitives _current           { 0 };

	bool request_acceptable(Block::Request const request) const
	{
		return !_current_request.operation_defined();
	}

	Number_of_primitives number_of_primitives(Block::Request const request) const
	{
		return request.count;
	}

	Number_of_primitives submit_request(Block::Request const request, Tag const tag)
	{
		_current_request  = request;
		_current_primitive = Test::Primitive {
			.tag          = tag;
			.block_number = request.block_number;
			.operation    = request.operation;
			.offset       = request.offset;
		};
		_num_primitives = request.count;
		_current = 1;

		return _num_primitives;
	}

	Primitive peek_generated_primitive() const
	{
		if (_current <= _num_primitives)
			return _current_primitive;

		return Primitive();
	}

	Primitive take_generated_primitive()
	{
		Primitive p = _current_primitive;

		if (_current <= _num_primitives) {
			_current_primitive.block_number++;
			_current_primitive.offset += BS;
		}

		return p;
	}
};


template <unsigned N, typename T>
struct Pool
{
	T                    _entries[N] { };
	Test::Number_of_primitives _used       { 0 };

	bool acceptable(Test::Number_of_primitives const num) const
	{
		return num <= (N - _used);
	}

	void submit(T const &p)
	{
		for (unsigned i = 0; i < N; i++) {
			if (!_entries[i].valid()) {
				_entries[i] = p;
			}
		}
	}

	/**
	 * Apply 'fn' with completed job, reset job
	 */
	template <typename FN>
	void for_each_valid_entry(FN const &fn)
	{
		for (unsigned i = 0; i < N; i++)
			if (_entries[i].valid())
				fn(_entries[i]);
	}
};

struct Test::Main : Rpc_object<Typed_root<Block::Session> >
{
	Env &_env;

	Constructible<Attached_ram_dataspace> _block_ds { };

	Constructible<Block_session_component> _block_session { };

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };

	Jobs<10> _jobs { };
	Splitter<4096> _splitter { };

	Pool<16, Test::Primitive> _splitter_pool { };

	struct Prims_for_tag
	{
		Tag const tag;
		Number_of_primitives const num;


		Prims_for_tag() : tag(0), num (0) { }
		Prims_for_tag(Tag const &tag, Number_of_primitives const num)
		: tag(tag), num(num) { }

		bool valid() const { return num != 0; }
	};

	Pool<16, Prims_for_tag> _tag_pool { };

	Block::Request _current_request { };
	Tag _current_tag { 0 };
	Number_of_primitives _current_num { 0 };
	Number_of_primitives _done_num    { 0 };

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

				Number_of_primitives const num = _splitter.number_of_primitives(request);

				if (!_splitter_pool.acceptable(num) || !_splitter.request_acceptable(request))
					return Block_session_component::Response::RETRY;

				_current_num = num;
				_done_num    = 0;

				_current_request = request;
				_current_tag++;
				progress = true;

				_splitter.submit_request(_current_request, _current_tag);
				return Block_session_component::Response::ACCEPTED;
			});

			while (true) {
				Splitter::Primitive p = _splitter.peek_generated_primitive();
				if (!p.valid()) { break; }

				if (!_jobs.acceptable(request)) { break; }

				p = _splitter.take_generated_primitive();
				_splitter_pool.submit(p);

				_jobs.submit(p);
				progress = true;
			}

			/* process I/O */
			progress |= _jobs.execute();


			/* acknowledge finished jobs */
			block_session.try_acknowledge([&] (Block_session_component::Ack &ack) {

				/* acknowledge finished I/O jobs */
				_jobs.with_any_completed_job([&] (Primitive const &p) {

					progress |= true;

					_done_num ++;
				});

				if (_done_num == _current_num)
					ack.submit(_current_request);
			});

			if (!progress)
				break;
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


void Component::construct(Genode::Env &env) { static Test::Main inst(env); }

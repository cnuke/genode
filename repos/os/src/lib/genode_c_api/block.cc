/*
 * \brief  Genode block service provider C-API
 * \author Stefan Kalkowski
 * \date   2021-07-10
 */

/*
 * Copyright (C) 2006-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <block/request_stream.h>
#include <root/component.h>
#include <os/buffered_xml.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <util/bit_array.h>

#include <genode_c_api/block.h>

using namespace Genode;

namespace {
	using namespace Genode;

	template <uint16_t ENTRIES>
	struct Bitmap
	{
		Genode::Bit_array<ENTRIES> _bitmap { };

		uint16_t capacity() const { return ENTRIES; }

		uint16_t _bitmap_find_free() const
		{
			for (uint16_t i = 0; i < ENTRIES; i++) {
				if (_bitmap.get(i, 1)) { continue; }
				return i;
			}
			return ENTRIES;
		}

		void reserve(uint16_t const id) {
			_bitmap.set(id, 1); }

		bool used(uint16_t const cid) const {
			return _bitmap.get(cid, 1); }

		uint16_t alloc()
		{
			uint16_t const id = _bitmap_find_free();
			_bitmap.set(id, 1);
			return id;
		}

		void free(uint16_t id)
		{
			_bitmap.clear(id, 1);
		}
	};

	using Session_space = Id_space<genode_block_session>;

	struct Device_info {
		using Name = String<64>;

		Name                 const name;
		Block::Session::Info const info;

		Device_info(const char * name, Block::Session::Info info)
		: name(name), info(info) { }
	};

} /* anonymous namespace */


class genode_block_session : public Rpc_object<Block::Session>
{
	private:

		friend class Root;

		Session_space::Element const _elem;

		Device_info::Name const _device_name;

		Block::block_number_t const _block_range_offset;

		bool _device_gone { false };

		enum { MAX_REQUESTS = 32 };

		struct Request {
			enum State { FREE, IN_FLIGHT, DONE };

			State                state    { FREE };
			genode_block_request dev_req  { 0, GENODE_BLOCK_UNAVAIL,
			                                0, 0, nullptr };
			Block::Request       peer_req {};
		};

		genode_shared_dataspace * _ds;
		Block::Request_stream     _rs;
		Request                   _requests[MAX_REQUESTS];

		template <typename FUNC>
		void _first_request(Request::State state, FUNC const & fn)
		{
			for (unsigned idx = 0; idx < MAX_REQUESTS; idx++) {
				if (_requests[idx].state == state) {
					fn(_requests[idx]);
					return;
				}
			}
		}

		template <typename FUNC>
		void _for_each_request(Request::State state, FUNC const & fn)
		{
			for (unsigned idx = 0; idx < MAX_REQUESTS; idx++) {
				if (_requests[idx].state == state)
					fn(_requests[idx]);
			}
		}

		/*
		 * Non_copyable
		 */
		genode_block_session(const genode_block_session&);
		genode_block_session & operator=(const genode_block_session&);

	public:

		genode_block_session(Session_space           & space,
		                     uint16_t                  session_id_value,
		                     Env                     & env,
		                     Block::Range              block_range,
		                     Device_info       const & device_info,
		                     Signal_context_capability cap,
		                     size_t                    buffer_size);

		Info info() const override { return _rs.info(); }

		Capability<Tx> tx_cap() override { return _rs.tx_cap(); }

		genode_block_request * request();
		void ack(genode_block_request * req, bool success);

		void notify_peers() { _rs.wakeup_client_if_needed(); }

		Block::block_number_t offset() const { return _block_range_offset; }

		Session_space::Id session_id() const { return _elem.id(); }

		Device_info::Name const & device_name() const { return _device_name; }

		void mark_device_gone() { _device_gone = true; }
};


class Root : public Root_component<genode_block_session>
{
	private:

		enum { MAX_BLOCK_DEVICES = 32 };

		Session_space _session_space { };

		static const uint16_t MAX_SESSIONS = 64u;
		using Session_map = Bitmap<MAX_SESSIONS>;
		Session_map _session_map { };

		uint16_t _first_id = 0;
		uint16_t _id_array[MAX_SESSIONS] { };

		// XXX manage sessions per device
		void _for_each_session(auto const &fn)
		{
			uint16_t max_ids = 0;
			for (uint16_t i = 0; i < _session_map.capacity(); i++)
				if (_session_map.used(i)) {
					_id_array[max_ids] = i;
					++max_ids;
				}

			for (uint16_t i = 0; i < max_ids; i++) {
				uint16_t const id = _id_array[(_first_id + i) % max_ids];

				fn(Session_space::Id { .value = id });
			}
			_first_id++;
		}

		Env                         & _env;
		Signal_context_capability     _sigh_cap;
		Constructible<Buffered_xml>   _config   { };
		Reporter                      _reporter { _env, "block_devices" };
		Constructible<Device_info>    _devices[MAX_BLOCK_DEVICES];
		bool                          _announced     { false };
		bool                          _report_needed { false };

		Root(const Root&);
		Root & operator=(const Root&);

		genode_block_session * _create_session(const char * args,
		                                       Affinity const &) override;

		void _destroy_session(genode_block_session * session) override;

		template <typename FUNC>
		void _for_each_device_info(FUNC const & fn)
		{
			for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++)
				if (_devices[idx].constructed())
					fn(*_devices[idx]);
		}

		void _report();

	public:

		struct Invalid_block_device_id {};

		Root(Env & env, Allocator & alloc, Signal_context_capability);

		void announce_device(const char * name, Block::Session::Info info);
		void discontinue_device(const char * name);
		genode_block_session * session(const char * name);
		void for_each_session(const char * name, auto const & session_fn);
		void notify_peers();
		void apply_config(Xml_node const &);
};


static ::Root                               * _block_root        = nullptr;
static genode_shared_dataspace_alloc_attach_t _alloc_peer_buffer = nullptr;
static genode_shared_dataspace_free_t         _free_peer_buffer  = nullptr;


genode_block_request * genode_block_session::request()
{
	using Response = Block::Request_stream::Response;

	genode_block_request * ret = nullptr;

	_rs.with_requests([&] (Block::Request request) {

		if (_device_gone)
			return Response::REJECTED;

		if (ret)
			return Response::RETRY;

		/* ignored operations */
		if (request.operation.type == Block::Operation::Type::TRIM ||
		    request.operation.type == Block::Operation::Type::INVALID) {
			request.success = true;
			return Response::REJECTED;
		}

		Response response = Response::RETRY;

		_first_request(Request::FREE, [&] (Request & r) {

			r.state    = Request::IN_FLIGHT;
			r.peer_req = request;

			Block::Operation const op = request.operation;
			switch(op.type) {
			case Block::Operation::Type::SYNC:
				r.dev_req.op = GENODE_BLOCK_SYNC;
				break;
			case Block::Operation::Type::READ:
				r.dev_req.op = GENODE_BLOCK_READ;
				break;
			case Block::Operation::Type::WRITE:
				r.dev_req.op = GENODE_BLOCK_WRITE;
				break;
			default:
				r.dev_req.op = GENODE_BLOCK_UNAVAIL;
			};

			r.dev_req.id      = _elem.id().value;
			r.dev_req.blk_nr  = op.block_number + _block_range_offset;
			r.dev_req.blk_cnt = op.count;
			r.dev_req.addr    = (void*)
				(genode_shared_dataspace_local_address(_ds) + request.offset);

			ret = &r.dev_req;
			response = Response::ACCEPTED;
		});

		return response;
	});

	return ret;
}


void genode_block_session::ack(genode_block_request * req, bool success)
{
	if (req->id != _elem.id().value)
		return;

	_for_each_request(Request::IN_FLIGHT, [&] (Request & r) {
		if (&r.dev_req == req)
			r.state = Request::DONE;
	});

	/* Acknowledge any pending packets */
	_rs.try_acknowledge([&](Block::Request_stream::Ack & ack) {
		_first_request(Request::DONE, [&] (Request & r) {
			r.state = Request::FREE;
			r.peer_req.success = success;
			ack.submit(r.peer_req);
		});
	});
}


genode_block_session::genode_block_session(Session_space           & space,
                                           uint16_t                  session_id_value,
                                           Env                     & env,
                                           Block::Range              block_range,
                                           Device_info       const & device_info,
                                           Signal_context_capability cap,
                                           size_t                    buffer_size)
:
	_elem(*this, space, Session_space::Id { .value = session_id_value }),
	_device_name(device_info.name),
	_block_range_offset(block_range.offset),
	_ds(_alloc_peer_buffer(buffer_size)),
	_rs(env.rm(), genode_shared_dataspace_capability(_ds), env.ep(), cap,
	    sanitize_info(device_info.info, block_range))
{ }


genode_block_session * ::Root::_create_session(const char * args,
                                               Affinity const &)
{
	if (!_config.constructed())
		throw Service_denied();

	Session_label      const label = label_from_args(args);
	Session_policy     const policy(label, _config->xml);
	Device_info::Name const device =
		policy.attribute_value("device", Device_info::Name());

	Ram_quota const ram_quota = ram_quota_from_args(args);
	size_t const tx_buf_size =
		Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

	if (!tx_buf_size)
		throw Service_denied();

	if (tx_buf_size > ram_quota.value) {
		error("insufficient 'ram_quota' from '", label, "',"
		      " got ", ram_quota, ", need ", tx_buf_size);
		throw Insufficient_ram_quota();
	}

	genode_block_session * ret = nullptr;

	uint16_t const new_session_id = _session_map.alloc();
	if (new_session_id == _session_map.capacity()) {
		_session_map.free(new_session_id);
		throw Service_denied();
	}

	_for_each_device_info([&] (Device_info & di) {
		if (di.name != device)
			return;

		bool const writeable_arg =
			Arg_string::find_arg(args, "writeable").bool_value(true);

		Block::Range const block_range {
			.offset     = Arg_string::find_arg(args, "offset")
			                                   .ulonglong_value(0),
			.num_blocks = Arg_string::find_arg(args, "num_blocks")
			                                   .ulonglong_value(0),
			.writeable  = di.info.writeable && writeable_arg
		};

		ret = new (md_alloc()) genode_block_session(_session_space, new_session_id,
		                                            _env, block_range, di, _sigh_cap,
		                                            tx_buf_size);
	});

	if (!ret) {
		_session_map.free(new_session_id);
		throw Service_denied();
	}

	return ret;
}


void ::Root::_destroy_session(genode_block_session * session)
{
	genode_shared_dataspace * ds = session->_ds;
	Session_space::Id const session_id = session->session_id();
	_session_map.free(uint16_t(session_id.value));

	Genode::destroy(md_alloc(), session);
	_free_peer_buffer(ds);
}


void ::Root::_report()
{
	if (!_report_needed)
		return;

	_reporter.enabled(true);
	Reporter::Xml_generator xml(_reporter, [&] () {
		_for_each_device_info([&] (Device_info & di) {
			xml.node("device", [&] {
				xml.attribute("label",       di.name);
				xml.attribute("block_size",  di.info.block_size);
				xml.attribute("block_count", di.info.block_count);
			});
		});
	});
}


void ::Root::announce_device(const char * name, Block::Session::Info info)
{
	for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++) {
		if (_devices[idx].constructed())
			continue;

		_devices[idx].construct(name, info);
		if (!_announced) {
			_env.parent().announce(_env.ep().manage(*this));
			_announced = true;
		}
		_report();
		return;
	}

	error("Could not announce driver for device ", name, ", no slot left!");
}


void ::Root::discontinue_device(const char * name)
{
	for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++) {
		if (!_devices[idx].constructed() || _devices[idx]->name != name)
			continue;

		_for_each_session([&] (Session_space::Id session_id) {
			_session_space.apply<genode_block_session>(session_id,
				[&] (genode_block_session & session) {
					if (session.device_name() == name)
						session.mark_device_gone();
				},
				[&] { });
		});

		_devices[idx].destruct();
		_report();
		return;
	}
}


genode_block_session * ::Root::session(const char *)
{
	return nullptr;
}


void ::Root::for_each_session(const char * name, auto const & session_fn)
{
	_for_each_session([&] (Session_space::Id session_id) {
		_session_space.apply<genode_block_session>(session_id,
			[&] (genode_block_session & session) {
				if (session.device_name() == name)
					session_fn(&session);
			},
			[&] { error("session ", session_id.value, " not found"); });
	});
}


void ::Root::notify_peers()
{
	_session_space.for_each<genode_block_session>(
		[&] (genode_block_session & session) {
			session.notify_peers();
		});
}


void ::Root::apply_config(Xml_node const & config)
{
	_config.construct(*md_alloc(), config);
	_report_needed = config.attribute_value("report", false);
}


::Root::Root(Env & env, Allocator & alloc, Signal_context_capability cap)
:
	Root_component<genode_block_session>(env.ep(), alloc),
	_env(env), _sigh_cap(cap) { }


extern "C" void
genode_block_init(genode_env                           * env_ptr,
                  genode_allocator                     * alloc_ptr,
                  genode_signal_handler                * sigh_ptr,
                  genode_shared_dataspace_alloc_attach_t alloc_func,
                  genode_shared_dataspace_free_t         free_func)
{
	static ::Root root(*static_cast<Env*>(env_ptr),
	                   *static_cast<Allocator*>(alloc_ptr),
	                   cap(sigh_ptr));
	_alloc_peer_buffer = alloc_func;
	_free_peer_buffer  = free_func;
	_block_root        = &root;

}


extern "C" void genode_block_announce_device(const char *       name,
                                             unsigned long long sectors,
                                             int                writeable)
{
	enum { SIZE_LOG2_512 = 9 };

	if (!_block_root)
		return;

	_block_root->announce_device(name, { 1UL << SIZE_LOG2_512,
	                             sectors, SIZE_LOG2_512,
	                             (writeable != 0) ? true : false });
}


extern "C" void genode_block_discontinue_device(const char * name)
{
	if (_block_root)
		_block_root->discontinue_device(name);
}


extern "C" struct genode_block_session *
genode_block_session_by_name(const char * name)
{
	return _block_root ? _block_root->session(name) : nullptr;
}


extern "C" void
genode_block_session_for_each_by_name(const char * name,
                                      genode_block_session_context *ctx,
                                      genode_block_session_one_session_t session_fn)
{
	if (!_block_root)
		return;

	_block_root->for_each_session(name,
		[&] (genode_block_session * session) {
			session_fn(ctx, session); });
}


extern "C" struct genode_block_request *
genode_block_request_by_session(struct genode_block_session * session)
{
	return session ? session->request() : nullptr;
}


extern "C" void genode_block_ack_request(struct genode_block_session * session,
                                         struct genode_block_request * req,
                                         int success)
{
	if (session)
		session->ack(req, success ? true : false);
}


extern "C" void genode_block_notify_peers()
{
	if (_block_root) _block_root->notify_peers();
}


void genode_block_apply_config(Xml_node const & config)
{
	if (_block_root) _block_root->apply_config(config);
}

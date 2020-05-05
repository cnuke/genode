/*
 * \brief  File to Block session
 * \author Josef Soentgen
 * \date   2020-05-05
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block/request_stream.h>
#include <util/string.h>
#include <vfs/simple_env.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>


using namespace Genode;
using Vfs::file_offset;
using Vfs::file_size;


class Block_file
{
	private:

		Block_file(const Block_file&) = delete;
		Block_file& operator=(const Block_file&) = delete;

		Env &_env;
		Heap                    _heap       { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config_rom { _env, "config" };

		Vfs::Simple_env   _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs") };
		Vfs::File_system &_vfs     { _vfs_env.root_dir() };

		Vfs::Vfs_handle mutable *_vfs_handle;

		bool _verbose;

		struct Request
		{
			enum State {
				NONE,
				READ_PENDING,  READ_IN_PROGRESS,  READ_COMPLETE,
				WRITE_PENDING, WRITE_IN_PROGRESS, WRITE_COMPLETE,
				SYNC_PENDING,  SYNC_IN_PROGRESS,  SYNC_COMPLETE,
				ERROR,
			};

			static char const *state_to_string(State s)
			{
				switch (s) {
					case State::NONE:              return "NONE";
					case State::READ_PENDING:      return "READ_PENDING";
					case State::READ_IN_PROGRESS:  return "READ_IN_PROGRESS";
					case State::READ_COMPLETE:     return "READ_COMPLETE";
					case State::WRITE_PENDING:     return "WRITE_PENDING";
					case State::WRITE_IN_PROGRESS: return "WRITE_IN_PROGRESS";
					case State::WRITE_COMPLETE:    return "WRITE_COMPLETE";
					case State::SYNC_PENDING:      return "SYNC_PENDING";
					case State::SYNC_IN_PROGRESS:  return "SYNC_IN_PROGRESS";
					case State::SYNC_COMPLETE:     return "SYNC_COMPLETE";
					case State::ERROR:             return "ERROR";
				}
				return "<unknown>";
			}

			Block::Request  block_request;
			char           *data;
			State           state;
			file_offset     offset;
			file_size       count;
			file_size       out_count;

			file_offset current_offset;
			file_size   current_count;

			bool success;
			bool complete;

			bool pending()   const { return state != NONE; }
			bool idle()      const { return state == NONE; }
			bool completed() const { return complete; }

			void print(Genode::Output &out) const
			{
				Genode::print(out, "(", block_request.operation, ")",
					" state: ",          state_to_string(state),
					" offset: ",         offset,
					" count: ",          count,
					" out_count: ",      out_count,
					" current_offset: ", current_offset,
					" current_count: ",  current_count,
					" success: ",        success,
					" complete: ",       complete);
			}
		};

		Request _current_request { };

		bool _read(Request &request)
		{
			bool progress = false;

			switch (request.state) {
			case Request::State::READ_PENDING:

				_vfs_handle->seek(request.offset + request.current_offset);
				if (!_vfs_handle->fs().queue_read(_vfs_handle, request.current_count)) {
					return progress;
				}

				request.state = Request::State::READ_IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case Request::State::READ_IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Read_result;

				bool completed = false;
				file_size out = 0;

				Result const result =
					_vfs_handle->fs().complete_read(_vfs_handle,
					                                request.data + request.current_offset,
					                                request.current_count, out);
				if (   result == Result::READ_QUEUED
				    || result == Result::READ_ERR_INTERRUPT
				    || result == Result::READ_ERR_AGAIN
				    || result == Result::READ_ERR_WOULD_BLOCK) {
					return progress;
				}

				if (result == Result::READ_OK) {
					request.current_offset += out;
					request.current_count  -= out;
					request.success = true;
				} else

				if (   result == Result::READ_ERR_IO
				    || result == Result::READ_ERR_INVALID) {
					request.success = false;
					completed = true;
				}

				if (request.current_count == 0 || completed) {
					request.state = Request::State::READ_COMPLETE;
				} else {
					request.state = Request::State::READ_PENDING;
					return progress;
				}
				progress = true;
			}
			[[fallthrough]];
			case Request::State::READ_COMPLETE:

				request.state    = Request::State::NONE;
				request.complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _write(Request &request)
		{
			bool progress = false;

			switch (request.state) {
			case Request::State::WRITE_PENDING:

				_vfs_handle->seek(request.offset + request.current_offset);

				request.state = Request::State::WRITE_IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case Request::State::WRITE_IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Write_result;

				bool completed = false;
				file_size out = 0;

				Result result = Result::WRITE_ERR_INVALID;
				try {
					result = _vfs_handle->fs().write(_vfs_handle,
					                                 request.data + request.current_offset,
					                                 request.current_count, out);
				} catch (Vfs::File_io_service::Insufficient_buffer) {
					return progress;
				}
				if (   result == Result::WRITE_ERR_AGAIN
				    || result == Result::WRITE_ERR_INTERRUPT
				    || result == Result::WRITE_ERR_WOULD_BLOCK) {
					return progress;
				}
				if (result == Result::WRITE_OK) {
					request.current_offset += out;
					request.current_count  -= out;
					request.success = true;
				}

				if (   result == Result::WRITE_ERR_IO
				    || result == Result::WRITE_ERR_INVALID) {
					request.success = false;
					completed = true;
				}
				if (request.current_count == 0 || completed) {
					request.state = Request::State::WRITE_COMPLETE;
				} else {
					request.state = Request::State::WRITE_PENDING;
					return progress;
				}
				progress = true;
			}
			[[fallthrough]];
			case Request::State::WRITE_COMPLETE:

				request.state = Request::State::NONE;
				request.complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _sync(Request &request)
		{
			bool progress = false;

			switch (request.state) {
			case Request::State::SYNC_PENDING:

				if (!_vfs_handle->fs().queue_sync(_vfs_handle)) {
					return progress;
				}
				request.state = Request::State::SYNC_IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case Request::State::SYNC_IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Sync_result;
				Result const result = _vfs_handle->fs().complete_sync(_vfs_handle);

				if (result == Result::SYNC_QUEUED) {
					return progress;
				}

				if (result == Result::SYNC_ERR_INVALID) {
					request.success = false;
				}

				if (result == Result::SYNC_OK) {
					request.success = true;
				}

				request.state = Request::State::SYNC_COMPLETE;
				progress = true;
			}
			[[fallthrough]];
			case Request::State::SYNC_COMPLETE:

				request.state = Request::State::NONE;
				request.complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _invalid(Request &request)
		{
			request.block_request.success = false;
			request.complete = true;
			return true;
		}

		struct Io_response_handler : Vfs::Io_response_handler
		{
			Signal_context_capability sigh { };

			void read_ready_response() override { }

			void io_progress_response() override
			{
				if (sigh.valid()) {
					Signal_transmitter(sigh).submit();
				}
			}
		};
		Io_response_handler _io_response_handler { };

		Block::Session::Info _info { };

	public:

		Block_file(Env &env, Signal_context_capability sigh)
		:
			_env        { env },
			_vfs_handle { nullptr },
			_verbose    { _config_rom.xml().attribute_value("verbose", false) }
		{
			using File_name = Genode::String<64>;
			File_name const file_name =
				_config_rom.xml().attribute_value("file", File_name());
			if (!file_name.valid()) {
				Genode::error("config 'file' attribute invalid");
				throw Genode::Exception();
			}

			bool const writeable =
				_config_rom.xml().attribute_value("writeable", false);

			// _env.ep().register_io_progress_handler(*this);

			using Open_result = Vfs::Directory_service::Open_result;
			Open_result res = _vfs.open(file_name.string(),
			                            writeable ? Vfs::Directory_service::OPEN_MODE_RDWR
			                                      : Vfs::Directory_service::OPEN_MODE_RDONLY,
			                            &_vfs_handle, _heap);
			if (res != Open_result::OPEN_OK) {
				error("Could not open '", file_name.string(), "'");
				throw Genode::Exception();
			}

			using Stat_result = Vfs::Directory_service::Stat_result;
			Vfs::Directory_service::Stat stat { };
			Stat_result stat_res = _vfs.stat(file_name.string(), stat);
			if (stat_res != Stat_result::STAT_OK) {
				_vfs.close(_vfs_handle);
				throw Genode::Exception();
			}

			size_t const block_size =
				_config_rom.xml().attribute_value("block_size", 512u);
			Block::block_number_t const block_count = block_size / stat.size;

			_info = Block::Session::Info {
				.block_size  = block_size,
				.block_count = block_count,
				.align_log2  = log2(block_size),
				.writeable   = true,
			};

			_io_response_handler.sigh = sigh;
			_vfs_handle->handler(&_io_response_handler);
		}

		Block::Session::Info info() const { return _info; }

		static Request::State initial_state(Block::Operation::Type type)
		{
			using Type = Block::Operation::Type;

			switch (type) {
			case Type::READ:  return Request::State::READ_PENDING;
			case Type::WRITE: return Request::State::WRITE_PENDING;
			case Type::SYNC:  return Request::State::SYNC_PENDING;
			default:          return Request::State::ERROR;
			}
		}

		bool execute()
		{
			if (_current_request.idle()) { return false; }

			if (_verbose) {
				log("Execute: ", _current_request);
			}

			using Type = Block::Operation::Type;

			switch (_current_request.block_request.operation.type) {
			case Type::READ:  return _read(_current_request);
			case Type::WRITE: return _write(_current_request);
			case Type::SYNC:  return _sync(_current_request);
			default:          return _invalid(_current_request);
			}
		}

		bool acceptable(Block::Request const &) const
		{
			return !_current_request.pending();
		}

		void submit(Block::Request req, void *ptr, size_t length)
		{
			// assert req.operation.count * block_size == length
			file_offset const offset =
				req.operation.block_number * _info.block_size;

			_current_request = Request {
				.block_request  = req,
				.data           = reinterpret_cast<char*>(ptr),
				.state          = initial_state(req.operation.type),
				.offset         = offset,
				.count          = length,
				.out_count      = 0,
				.current_offset = 0,
				.current_count  = length,
				.success        = false,
				.complete       = false,
			};

			if (_verbose) {
				log("Submit: ", _current_request);
			}
		}

		template <typename FN>
		void with_any_completed_job(FN const &fn)
		{
			if (_current_request.completed()) {
				_current_request.block_request.success =
					_current_request.success;

				fn(_current_request.block_request);

				_current_request.state    = Request::State::NONE;
				_current_request.complete = false;
			}
		}
};


struct Block_session_component : Rpc_object<Block::Session>,
                                 private Block::Request_stream
{
	Entrypoint &_ep;

	using Block::Request_stream::with_requests;
	using Block::Request_stream::with_content;
	using Block::Request_stream::try_acknowledge;
	using Block::Request_stream::wakeup_client_if_needed;

	Block_session_component(Region_map                 &rm,
	                        Dataspace_capability        ds,
	                        Entrypoint                 &ep,
	                        Signal_context_capability   sigh,
	                        Block::Session::Info const &info)
	:
		Request_stream(rm, ds, ep, sigh, info),
		_ep(ep)
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
};


struct Main : Rpc_object<Typed_root<Block::Session>>
{
	Env &_env;

	Signal_handler<Main> _request_handler {
		_env.ep(), *this, &Main::_handle_requests };

	Block_file _block_file { _env, _request_handler };

	Constructible<Attached_ram_dataspace>  _block_ds { };
	Constructible<Block_session_component> _block_session { };

	void _handle_requests()
	{
		if (!_block_session.constructed()) {
			return;
		}

		Block_session_component &block_session = *_block_session;

		for (;;) {

			bool progress = false;

			/* import new requests */
			block_session.with_requests([&] (Block::Request request) {

				if (!_block_file.acceptable(request))
				return Block::Request_stream::Response::RETRY;

				/* access content of the request */
				block_session.with_content(request,
				[&] (void *ptr, size_t size) {
					_block_file.submit(request, ptr, size);
				});

				progress |= true;
				return Block::Request_stream::Response::ACCEPTED;
			});

			/* process I/O */
			progress |= _block_file.execute();

			/* acknowledge finished jobs */
			block_session.try_acknowledge([&] (Block::Request_stream::Ack &ack) {

				_block_file.with_any_completed_job([&] (Block::Request request) {
					progress |= true;
					ack.submit(request);
				});
			});

			if (!progress) { break; }
		}

		block_session.wakeup_client_if_needed();
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
		                         _request_handler, _block_file.info());

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


void Component::construct(Genode::Env &env) { static Main main(env); }

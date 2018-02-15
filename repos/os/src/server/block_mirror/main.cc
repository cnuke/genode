/*
 * \brief  Block mirror component
 * \author Josef Soentgen
 * \date   2018-02-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <block/component.h>
#include <block/driver.h>
#include <block_session/connection.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/bit_array.h>
#include <util/list.h>
#include <util/interface.h>

/* local includes */
#include <util.h>


enum { MIRRORS_MAX = 8, };

namespace {

using uint64_t          = Genode::uint64_t;
using size_t            = Genode::size_t;
using Packet_descriptor = Block::Packet_descriptor;

struct Info
{
	Block::sector_t bcount { 0 };
	size_t          bsize  { 0 };
	uint64_t        rx     { 0 };
	uint64_t        tx     { 0 };

	enum State { HEALTHY, FAULTY, } state { HEALTHY };
	enum Error { NONE, IO_ERROR, IO_TIMEOUT, } error { NONE };
};

} /* anonymous namespace */

class Mirror : Genode::Interface
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		unsigned const                     _id;
		Genode::Signal_context_capability  _done_signal;
		Timer::Connection                 &_timer;
		Genode::Microseconds const         _timeout;

	public:

		typedef Timer::One_shot_timeout<Mirror> Io_timeout;

		struct Request : Genode::List<Request>::Element
		{
			Io_timeout &_io_timeout;

			void    const * const preq;
			Block::sector_t const lba;
			size_t          const count;
			char          * const buffer;
			bool            const write;

			bool succeeded { false };

			Request(Io_timeout &timeout,
			        void const * const preq,
			        Block::sector_t    lba,
			        size_t             count,
			        char       * const buffer,
			        bool               write)
			:
				_io_timeout(timeout), preq(preq), lba(lba), count(count),
				buffer(buffer), write(write) { }

			bool match(const Packet_descriptor reply) const
			{
				return reply.operation()    == (write ? Packet_descriptor::WRITE
				                                      : Packet_descriptor::READ) &&
				       reply.block_number() == lba &&
				       reply.block_count()  == count;
			}

			void schedule_io_timeout(Genode::Microseconds us) { _io_timeout.schedule(us); }

			void discard_io_timeout() { _io_timeout.discard(); }
		};

	private:

		enum { TIMEOUT_SLAB_SIZE = Block::Session::TX_QUEUE_SIZE * sizeof(Io_timeout), };
		Genode::Tslab<Io_timeout, TIMEOUT_SLAB_SIZE> _timeout_slab { &_alloc };

		void _handle_io_timeout(Genode::Duration)
		{
			if (_offline) { return; }

			Genode::error("I/O timeout triggered on mirror ", _id);
			_info.error = Info::Error::IO_TIMEOUT;
			_mark_offline();
		}

		Info _info { };

		enum {
			SLAB_SIZE = Block::Session::TX_QUEUE_SIZE * sizeof(Request),
			SHM_SIZE  = 4 * 1024 * 1024,
		};

		Genode::Tslab<Request, SLAB_SIZE> _req_slab { &_alloc };
		Genode::List<Request>             _req_list { };
		size_t                            _reqs_pending { 0 };
		Genode::List<Request>             _req_done_list { };

		Genode::Allocator_avl      _block_alloc { &_alloc };
		Block::Connection          _block;
		Block::Session::Operations _block_ops   {   };
		Genode::size_t             _block_size  { 0 };
		Block::sector_t            _block_count { 0 };

		bool _offline { false };

		void _mark_offline()
		{
			_offline = true;
			_info.state = Info::State::FAULTY;
		}

		void _handle_request(Packet_descriptor &p, Request *r)
		{
			/*
			 * Stop processing, either marked as offline from the Driver
			 * or offline because an I/O timeout has occurred.
			 */
			if (_offline) { return; }

			r->discard_io_timeout();

			if (p.operation() == Packet_descriptor::READ) {
				Genode::memcpy(r->buffer, _block.tx()->packet_content(p),
				               _block_size * p.block_count());
			}

			if (!p.succeeded()) {
				Genode::error(_id, ": error handle request ",
				              p.block_number(), " ", p.block_count(), " ",
				              p.operation() == Packet_descriptor::READ ? "read" : "write");
				_info.error = Info::Error::IO_ERROR;
				_mark_offline();
			}

			r->succeeded = p.succeeded();
		}

		void _ack_avail()
		{
			bool request_handled = false;
			while (_block.tx()->ack_avail()) {
				Packet_descriptor p = _block.tx()->get_acked_packet();

				try {
					for (Request *r = _req_list.first(); r; /* */) {

						Request *next = r->next();

						if (r->match(p)) {
							request_handled = true;

							_reqs_pending--;

							_handle_request(p, r);
							_req_list.remove(r);
							_req_done_list.insert(r);

							Genode::destroy(&_timeout_slab, &r->_io_timeout);
							break;
						}

						r = next;
					}

				} catch (Block::Driver::Request_congestion) {
					Genode::warning("request congestion");
				}

				_block.tx()->release_packet(p);
			}

			/* notify front end */
			if (request_handled) {
				Genode::Signal_transmitter(_done_signal).submit();
			}
		}

		Genode::Signal_handler<Mirror> _ack_avail_sigh {
			_env.ep(), *this, &Mirror::_ack_avail };

		void _ready_to_submit() { }

		Genode::Signal_handler<Mirror> _ready_to_submit_sigh {
			_env.ep(), *this, &Mirror::_ready_to_submit };

	public:

		/**
		 * Constructor
		 */
		Mirror(Genode::Env &env, Genode::Allocator &alloc, Util::Label label,
		       unsigned id, Genode::Signal_context_capability done_signal,
		       Timer::Connection &timer, unsigned io_timeout,
		       size_t block_size, Block::sector_t block_count)
		:
			_env(env), _alloc(alloc), _id(id), _done_signal(done_signal),
			_timer(timer), _timeout(io_timeout*1000u),
			_block(_env, &_block_alloc, SHM_SIZE, label.string())
		{
			_block.info(&_block_count, &_block_size, &_block_ops);

			if (_block_size > block_size) {
				Genode::error("block size too large");
				throw -1;
			}

			if (_block_count < block_count) {
				Genode::error("block count too small");
				throw -1;
			}

			_info.bsize  = _block_size;
			_info.bcount = _block_count;

			Genode::log("(", _id, "):", " block_count: ", _block_count,
			                            " block_size: ",  _block_size,
			                            " online");

			_block.tx_channel()->sigh_ack_avail(_ack_avail_sigh);
			_block.tx_channel()->sigh_ready_to_submit(_ready_to_submit_sigh);
		}

		~Mirror()
		{
			if (_reqs_pending) {
				Genode::warning("(", _id, ") might leak ",
				                _reqs_pending, " requests");
			}
		}

		/**
		 * Return id of mirror
		 */
		unsigned id() const { return _id; }

		/**
		 * Mark mirror as offline
		 *
		 * The front end will call this method when a request was not
		 * successful.
		 */
		void mark_offline() { _mark_offline(); }

		/**
		 * Check if mirror is offline
		 */
		bool offline() const { return _offline; }

		/**
		 * Check if mirror is ready to perform operation
		 *
		 * \param count  number of blocks
		 *
		 * \returns true if the operation will succeeded, otherwise
		 *          false will be returned
		 */
		bool ready(size_t count)
		{
			if (_offline) { return false; }

			if (!_block.tx()->ready_to_submit()) {
				Genode::warning(__func__, "(", _id, "): not ready to submit packet");
				return false;
			}

			try {
				Genode::size_t const size = _block_size * count;
				/* XXX peeking into the packet stream would be nice */
				Packet_descriptor tmp = _block.tx()->alloc_packet(size);
				_block.tx()->release_packet(tmp);
				return true;
			} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
				Genode::warning(__func__, "(", _id, "): packet alloc failed");
				return false;
			}

			return false; /* never reached */
		}

		/**
		 * Perform I/O operation
		 *
		 * The operation is expected to succeed.
		 *
		 * \param write   true when operation is write requeste, otherwise
		 *                false in case of read request
		 * \param lba     logical block number to start from
		 * \param count   number of blocks
		 * \param buffer  target buffer
		 * \param preq    opaque r/o pointer to parent request
		 */
		bool io(bool write, Block::sector_t nr, Genode::size_t count,
		         char * const buffer, void const * const preq)
		{
			if (_offline) { return false; }

			Packet_descriptor p;

			Packet_descriptor::Opcode const op = write
				? Packet_descriptor::WRITE
				: Packet_descriptor::READ;

			Genode::size_t const size    = _block_size * count;
			Packet_descriptor tmp = _block.tx()->alloc_packet(size);

			p = Packet_descriptor(tmp, op, nr, count);

			/* XXX move Io_timeout to Request */
			Io_timeout *timeout =
				new (&_timeout_slab) Io_timeout(_timer, *this, &Mirror::_handle_io_timeout);
			Request *r = new (&_req_slab) Request(*timeout, preq, nr, count, buffer, write);
			_req_list.insert(r);

			if (write) {
				char * const dest = _block.tx()->packet_content(p);
				Genode::memcpy(dest, buffer, size);
			}

			_reqs_pending++;
			_block.tx()->submit_packet(p);

			r->schedule_io_timeout(_timeout);

			_info.rx += !write * count;
			_info.tx +=  write * count;

			return true;
		}

		/**
		 * Synchronize underlying buffers
		 */
		void sync() { _block.sync(); }

		/**
		 * Iterate over all request that are finished
		 *
		 * The called function is expected to return true in case the
		 * request was acknowledged by the front end and may be removed.
		 * It must return false in case the request should be kept.
		 *
		 * \param func  function to be called for every done request
		 */
		template <typename FUNC>
		void requests_done(FUNC const &func)
		{
			for (Request *r = _req_done_list.first(); r; /* */) {

				if (!func(*r)) {
					r = r->next();
					continue;
				}

				Request *next = r->next();
				_req_done_list.remove(r);
				Genode::destroy(&_req_slab, r);
				r = next;
			}
		}

		Info const info() const { return _info; }
};


/*
 * This class provides the front end Block session
 *
 * It parses the configuration and creates a back end Block session
 * for every 'block' node.
 */
class Driver : public Block::Driver
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		Util::Id_allocator<MIRRORS_MAX> _ids { };
		Genode::Registry<Genode::Registered<Mirror>> _mirror_registry { };
		unsigned _mirrors { 0 };

		struct Request : Genode::List<Request>::Element
		{
			Util::Id_allocator<MIRRORS_MAX> pending   { };
			Util::Id_allocator<MIRRORS_MAX> succeeded { };

			Packet_descriptor request;
			bool const        write;

			char *read_buffer;

			Request(Packet_descriptor &request, bool write, char *buffer = nullptr)
			: request(request), write(write), read_buffer(buffer) { }

			bool match(const Packet_descriptor reply) const
			{
				return reply.operation()    == request.operation()    &&
				       reply.block_number() == request.block_number() &&
				       reply.block_count()  == request.block_count();
			}
		};

		enum {
			SLAB_SIZE = Block::Session::TX_QUEUE_SIZE * sizeof(Request),
		};

		Genode::Tslab<Request, SLAB_SIZE> _req_slab { &_alloc };
		Genode::List<Request>             _req_list { };
		size_t                            _reqs_pending { 0 };

		bool _requeue_read_request(Request &r)
		{
			unsigned const read_id = _ids.next_id();
			auto mirror_io = [&] (Mirror &mirror) {
				if (mirror.offline() || mirror.id() != read_id) { return; }

				mirror.io(false, r.request.block_number(),
				                 r.request.block_count(), r.read_buffer, &r);
				r.pending.set(mirror.id());
			};
			_mirror_registry.for_each(mirror_io);

			return r.pending.empty() ?  false : true;
		}

		void _request_done()
		{
			bool io_error = false;
			for (Request *r = _req_list.first(); r; /* */) {

				auto check_done = [&] (Mirror &mirror) {
					mirror.requests_done([&] (Mirror::Request &req) {
						if (r != req.preq) { return false; }

						if (req.succeeded) {
							r->succeeded.set(mirror.id());
						} else {
							if (!mirror.offline()) {
								Genode::error("mark mirror (", mirror.id(),
								              ") offline, request failed");
								mirror.mark_offline();
							}
							io_error = true;
							_info.state = Info::Info::FAULTY;
						}

						r->pending.clear(mirror.id());
						return true;
					});
				};
				_mirror_registry.for_each(check_done);

				bool const pending = (r->pending.empty() == false);
				if (pending) {
					r = r->next();
					continue;
				}

				bool const succeeded = (r->succeeded.empty() == false);
				if (!succeeded && !r->write) {
					/* try to use another mirror session for read request */
					if (_requeue_read_request(*r)) {
						r = r->next();
						continue;
					}
				}

				/* generate report in case of I/O errors */
				if (io_error) {
					Genode::Duration d(Genode::Microseconds(0));
					_handle_report_timeout(d);
				}

				--_reqs_pending;

				if (!succeeded) {
					Genode::error("request: ", r->request.block_number(), " failed");
				}

				/* we are finished here, notify client and clean up */
				ack_packet(r->request, succeeded);

				Request *next = r->next();

				_req_list.remove(r);
				Genode::destroy(&_req_slab, r);

				r = next;
			}
		}

		Genode::Signal_handler<Driver> _request_done_sigh {
			_env.ep(), *this, &Driver::_request_done };

		Block::Session::Operations _block_ops   {   };
		Genode::size_t             _block_size  { 0 };
		Block::sector_t            _block_count { 0 };

		Genode::Reporter _stats_reporter { _env, "mirror", "info" };

		bool                 _report_total      { false };
		bool                 _report_individual { false };
		Genode::Microseconds _report_interval { 0 };

		Info _info { };

		Timer::Connection _timer    { _env };
		Timer::Connection _timer_io { _env };

		void _handle_report_timeout(Genode::Duration)
		{
			try {
				Genode::Reporter::Xml_generator xml(_stats_reporter, [&] () {
					if (_report_total) {
						xml.attribute("rx",     _info.rx);
						xml.attribute("tx",     _info.tx);
						xml.attribute("bsize",  _info.bsize);
						xml.attribute("bcount", _info.bcount);
						xml.attribute("degraded", _info.state != Info::State::HEALTHY);
					}
					if (_report_individual) {
						auto collect_stats = [&] (Mirror const & mirror) {
							Info const info = mirror.info();
							xml.node("block", [&] () {
								xml.attribute("rx",     info.rx);
								xml.attribute("tx",     info.tx);
								xml.attribute("bsize",  info.bsize);
								xml.attribute("bcount", info.bcount);

								bool const online = info.state == Info::State::HEALTHY;
								xml.attribute("online", online);

								if (!online) {
									switch (info.error) {
									case Info::Error::IO_ERROR:
										xml.attribute("error", "io");
										break;
									case Info::Error::IO_TIMEOUT:
										xml.attribute("error", "timeout");
										break;
									default: break;
									}
								}
							});
						};
						_mirror_registry.for_each(collect_stats);
					}
				});
			} catch (...) { Genode::warning("could not report info"); }

			if ((_report_total || _report_individual) && _report_interval.value) {
				_report_timeout.schedule(_report_interval);
			}
		}

		Timer::One_shot_timeout<Driver> _report_timeout {
			_timer, *this, &Driver::_handle_report_timeout };

		Genode::Attached_rom_dataspace _config_rom { _env, "config" };
		bool                           _initial_config { true };

		void _handle_config_update()
		{
			_config_rom.update();

			if (!_config_rom.valid()) { return; }

			Genode::Xml_node config = _config_rom.xml();

			/* report handling */
			bool initial_report = false;
			try {
				Genode::Xml_node report = config.sub_node("report");
				bool     const report_total      = report.attribute_value("total", false);
				bool     const report_individual = report.attribute_value("individual", false);
				unsigned const report_interval   = report.attribute_value("interval", 1000u);
				bool     const enable_reporting  = report_total || report_individual;

				_stats_reporter.enabled(enable_reporting);

				_report_total      = report_total;
				_report_individual = report_individual;
				_report_interval   = Genode::Microseconds(report_interval*1000u);

				if (enable_reporting && _report_interval.value) {
					_report_timeout.schedule(_report_interval);
				} else { _report_timeout.discard(); }

				initial_report = report.attribute_value("initial", false);
			} catch (...) { }

			/* mirror settings cannot be changed at run-time */
			if (!_initial_config) { return; }

			_initial_config = false;

			/* mirror handling */
			Genode::Xml_node mirror = config.sub_node("mirror");

			size_t   const block_size  = mirror.attribute_value("block_size", 0UL);
			size_t   const block_count = mirror.attribute_value("block_count", 0UL);
			bool     const writeable   = mirror.attribute_value("writeable", false);
			unsigned const io_timeout  = mirror.attribute_value("timeout", 3000u);

			_block_size  = block_size;
			_block_count = block_count;
			_block_ops.set_operation(Packet_descriptor::READ);

			if (writeable) {
				_block_ops.set_operation(Packet_descriptor::WRITE);
			}

			_info.bsize  = _block_size;
			_info.bcount = _block_count;

			_mirrors = 0;
			mirror.for_each_sub_node("block", [&] (Genode::Xml_node node) {
				unsigned const id = _ids.alloc();
				if (id == Util::Id_allocator<MIRRORS_MAX>::INVALID) { return; }

				Util::Label label = node.attribute_value("label", Util::Label(""));

				if (!label.valid()) { return; }

				try {
					new (&_alloc) Genode::Registered<Mirror>(_mirror_registry, _env, _alloc,
					                                         label, id, _request_done_sigh,
					                                         _timer_io, io_timeout,
					                                         _block_size, _block_count);
				} catch (...) { return; }
				++_mirrors;
			});

			if (_mirrors < 1) {
				Genode::error("no mirror configured");
				throw Genode::Exception();
			}

			if (initial_report) {
				Genode::Duration d(Genode::Microseconds(0));
				_handle_report_timeout(d);
			}
		}

		Genode::Signal_handler<Driver> _config_sigh {
			_env.ep(), *this, &Driver::_handle_config_update };

		bool _check_ready(size_t count) {
			bool ready = true;
			auto mirror_ready = [&] (Mirror &mirror) {
				ready = mirror.ready(count);
			};
			_mirror_registry.for_each(mirror_ready);
			return ready ? true : false;
		}

	public:

		/**
		 * Constructor
		 */
		Driver(Genode::Env &env, Genode::Allocator &alloc)
		:
			Block::Driver(env.ram()), _env(env), _alloc(alloc)
		{
			_config_rom.sigh(_config_sigh);
			_handle_config_update();

			Genode::log("Provide", " block_count:", _block_count,
			                       " block_size:",  _block_size,
			                       " mirrors:", _mirrors);
		}

		~Driver()
		{
			if (_reqs_pending) {
				Genode::warning("might leak ", _reqs_pending, " requests");
			}

			/* XXX beware of dangling signals etc. pp */
			auto destroy = [&] (Mirror &mirror) {
				Genode::destroy(&_alloc, &mirror);
			};
			_mirror_registry.for_each(destroy);
		}

		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		Genode::size_t      block_size() override { return _block_size;  }
		Block::sector_t    block_count() override { return _block_count; }
		Block::Session::Operations ops() override { return _block_ops;   }

		void read(Block::sector_t lba, Genode::size_t count,
		          char *buffer, Packet_descriptor &p) override
		{
			if (!_block_ops.supported(Packet_descriptor::READ)) {
				throw Io_error();
			}

			if (!_check_ready(count)) { throw Request_congestion(); }

			Request *r = new (&_req_slab) Request(p, false, buffer);
			_req_list.insert(r);

			/* there can be only one... for reading that is */
			unsigned const read_id = _ids.next_id();
			auto mirror_io = [&] (Mirror &mirror) {
				if (mirror.id() != read_id) { return; }

				if (mirror.offline()) {
					Genode::warning("select mirror ", mirror.id(), " offline");
					return;
				}

				mirror.io(false, lba, count, buffer, r);
				r->pending.set(mirror.id());
			};
			_mirror_registry.for_each(mirror_io);

			/* request was not queued */
			if (r->pending.empty()) { throw Io_error(); }

			++_reqs_pending;
			_info.rx += count;
		}

		void write(Block::sector_t lba, Genode::size_t count,
		           char const *buffer, Packet_descriptor &p) override
		{

			if (!_block_ops.supported(Packet_descriptor::WRITE)) {
				throw Io_error();
			}

			if (!_check_ready(count)) { throw Request_congestion(); }

			Request *r = new (&_req_slab) Request(p, true);
			_req_list.insert(r);

			char *b = const_cast<char*>(buffer);
			auto mirror_io = [&] (Mirror &mirror) {

				if (mirror.offline()) {
					Genode::warning("ignore offline mirror ", mirror.id());
					return;
				}

				mirror.io(true, lba, count, b, r);
				r->pending.set(mirror.id());
			};
			_mirror_registry.for_each(mirror_io);

			/* request was not queued */
			if (r->pending.empty()) { throw Io_error(); }

			++_reqs_pending;
			_info.tx += count;
		}

		void sync() override
		{
			auto mirror_sync = [&] (Mirror &mirror) { mirror.sync(); };
			_mirror_registry.for_each(mirror_sync);
		}
};


struct Main
{
	Genode::Env &_env;
	Genode::Heap _heap { _env.ram(), _env.rm() };

	struct Factory : Block::Driver_factory
	{
		Genode::Env       &_env;
		Genode::Allocator &_alloc;
		Genode::Constructible<::Driver> _driver { };

		Factory(Genode::Env &env, Genode::Allocator &alloc)
		: _env(env), _alloc(alloc) { }

		~Factory() { }

		Block::Driver *create()
		{
			Genode::error("ram avail: ", _env.pd().avail_ram());

			_driver.construct(_env, _alloc);
			return &*_driver;
		}
		void destroy(Block::Driver *) { _driver.destruct(); }
	};

	bool const writeable { true };

	Factory     _factory { _env, _heap };
	Block::Root _root    { _env.ep(), _heap, _env.rm(), _factory, writeable };

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }

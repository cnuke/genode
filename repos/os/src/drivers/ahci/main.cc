/**
 * \brief  Block driver session creation
 * \author Sebastian Sumpf
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <block/request_stream.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>
#include <os/reporter.h>

/* local includes */
#include <ahci.h>
#include <ata_protocol.h>

namespace Ahci {
	struct Dispatch;
	class  Driver;
	struct Main;
	struct Block_session_handler;
	struct Block_session_component;
}


struct Ahci::Dispatch
{
	virtual void session(unsigned index) = 0;
};


class Ahci::Driver : Noncopyable
{
	public:

		enum {  MAX_PORTS = 32 };

	private:

		Env      &_env;
		Dispatch &_dispatch;

	/* read device signature */
		enum Signature {
			ATA_SIG        = 0x101,
			ATAPI_SIG      = 0xeb140101,
			ATAPI_SIG_QEMU = 0xeb140000, /* will be fixed in Qemu */
		};


		struct Timer_delayer : Mmio::Delayer, Timer::Connection
		{
			Timer_delayer(Env &env)
			: Timer::Connection(env) { }

			void usleep(uint64_t us) override { Timer::Connection::usleep(us); }
		} _delayer { _env };

		Hba  _hba { _env, _delayer };

		Constructible<Ata::Protocol> _ata[MAX_PORTS];
		Constructible<Port>          _ports[MAX_PORTS];

		Signal_handler<Driver> _irq { _env.ep(), *this, &Driver::handle_irq };
		bool                   _enable_atapi;


	/*
	 * Least significant bit
	 */
	unsigned _lsb(unsigned bits) const
	{
		for (unsigned i = 0; i < 32; i++)
			if (bits & (1u << i)) {
				return i;
			}

		return 0;
	}

	void _info()
	{
		log("version: "
		    "major=", Hex(_hba.read<Hba::Version::Major>()), " "
		    "minor=", Hex(_hba.read<Hba::Version::Minor>()));
		log("command slots: ", _hba.command_slots());
		log("native command queuing: ", _hba.ncq() ? "yes" : "no");
		log("64-bit support: ", _hba.supports_64bit() ? "yes" : "no");
	}

	void _scan_ports(Region_map &rm)
	{
		log("number of ports: ", _hba.port_count(), " pi: ",
		    Hex(_hba.read<Hba::Pi>()));

		unsigned available = _hba.read<Hba::Pi>();
		for (unsigned i = 0; i < _hba.port_count(); i++) {

			/* check if port is implemented */
			if (!available) break;
			unsigned index = _lsb(available);
			available ^= (1u << index);

			bool enabled = false;
			switch (Port_base(index, _hba).read<Port_base::Sig>()) {
				case ATA_SIG:
					try {
						_ata[index].construct();
						log("try to construct port: ", index);
						_ports[index].construct(*_ata[index], rm, _hba, index);
						log("constructed port: ", index);
						enabled = true;
					} catch (...) { }

					log("\t\t#", index, ":", enabled ? " ATA" : " off (ATA)");
					break;

				case ATAPI_SIG:
				case ATAPI_SIG_QEMU:
					if (_enable_atapi)
					/*
						try {
							ports[index] = new (&alloc)
								Atapi_driver(ram, root, ready_count, rm, hba,
								             platform_hba, index);
							enabled = true;
						} catch (...) { }*/

					log("\t\t#", index, ":", enabled ? " ATAPI" : " off (ATAPI)");
					break;

				default:
					log("\t\t#", index, ": off (unknown device signature)");
			}
		}
	}
	public:

		Driver(Env &env, Dispatch &dispatch, bool support_atapi)
		: _env(env), _dispatch(dispatch), _enable_atapi(support_atapi)
		{
			_info();

			/* register irq handler */
			_hba.sigh_irq(_irq);

			/* initialize HBA (IRQs, memory) */
			_hba.init();

			/* search for devices */
			_scan_ports(env.rm());
		}

		/**
		 * Forward IRQs to ports/block sessions
		 */
		void handle_irq()
		{
			unsigned port_list = _hba.read<Hba::Is>();

			log(__func__, ":", __LINE__);
			while (port_list) {
				unsigned port = log2(port_list);
				log(__func__, ":", __LINE__, "port_list: ", Hex(port_list), " port: ", port);
				port_list    &= ~(1U << port);

				/* handle (pending) requests */
				_dispatch.session(port);

				/* ack irq */
				log(__func__, ":", __LINE__);
				if (_ports[port].constructed()) {
					log("handle_irq for port: ", port);
					_ports[port]->handle_irq();
				} else {
					log("port: ", port, " not constructed");
				}
				log(__func__, ":", __LINE__);
			}
			log(__func__, ":", __LINE__);

			/* clear status register */
			_hba.ack_irq();
		}

		Port &port(long device, char const *model_num, char const *serial_num)
		{
			/* check for model/device */
			if (model_num && serial_num) {
				for (long index = 0; index < MAX_PORTS; index++) {
					if (!_ata[index].constructed()) continue;

					Ata::Protocol &protocol = *_ata[index];
					if (*protocol.model == model_num && *protocol.serial == serial_num)
						return *_ports[index];
				}
			}

			/* check for device number */
			if (device >= 0 && device < MAX_PORTS && _ports[device].constructed())
				return *_ports[device];

			throw -1;
		}

		template <typename FN> void for_each_port(FN const &fn)
		{
			for (unsigned index = 0; index < MAX_PORTS; index++) {
				if (!_ports[index].constructed()) continue;
				fn(*_ports[index], index, !_ata[index].constructed());
			}
		}

		void report_ports(Reporter &reporter)
		{
			auto report = [&](Port const &port, unsigned index, bool atapi) {

				Block::Session::Info info = port.info();
				Reporter::Xml_generator xml(reporter, [&] () {

					xml.node("port", [&] () {
						xml.attribute("num", index);
						xml.attribute("type", atapi ? "ATAPI" : "ATA");
						xml.attribute("block_count", info.block_count);
						xml.attribute("block_size", info.block_size);
						if (!atapi) {
							xml.attribute("model", _ata[index]->model->cstring());
							xml.attribute("serial", _ata[index]->serial->cstring());
						}
					});
				});
			};

			for_each_port(report);
		}
};


struct Ahci::Block_session_handler : Interface
{
	Env                     &env;
	Port                    &port;
	Ram_dataspace_capability ds;

	Signal_handler<Block_session_handler> request_handler
	  { env.ep(), *this, &Block_session_handler::handle};

	Block_session_handler(Env &env, Port &port, size_t buffer_size)
	: env(env), port(port), ds(port.alloc_buffer(buffer_size))
	{ }

	~Block_session_handler()
	{
		port.free_buffer(ds);
	}

	virtual void handle_requests()= 0;

	void handle()
	{
		handle_requests();
	}
};

struct Ahci::Block_session_component : Rpc_object<Block::Session>,
                                       Block_session_handler,
                                       Block::Request_stream
{
	Block_session_component(Env &env, Port &port, size_t buffer_size)
	:
	  Block_session_handler(env, port, buffer_size),
	  Request_stream(env.rm(), ds, env.ep(), request_handler, port.info())
	{
		env.ep().manage(*this);
	}

	~Block_session_component()
	{
		env.ep().dissolve(*this);
	}

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }

	void handle_requests() override
	{
		while (true) {

			bool progress = false;

			/*
			 * Acknowledge any pending packets before sending new request to the
			 * controller.
			 */
			try_acknowledge([&](Ack &ack) {
				port.for_one_completed_request([&] (Block::Request request) {
					progress = true;
					ack.submit(request);
				});
			});

			with_requests([&] (Block::Request request) {

				Response response = Response::RETRY;

				/* only READ/WRITE requests, others are noops for now */
				if (Block::Operation::has_payload(request.operation.type) == false) {
					request.success = true;
					progress = true;
					return Response::REJECTED;
				}

				if ((response = port.submit(request)) != Response::RETRY)
					progress = true;

				return response;
			});

			if (progress == false) break;
		}

		/* poke */
		wakeup_client_if_needed();
	}
};


struct Ahci::Main : Rpc_object<Typed_root<Block::Session>>,
                    Dispatch
{
	Env  &env;

	Attached_rom_dataspace config { env, "config" };

	Constructible<Ahci::Driver> driver { };
	Constructible<Reporter> reporter { };
	Constructible<Block_session_component> block_session[Driver::MAX_PORTS];

	Main(Env &env)
	: env(env)
	{
		log("--- Starting AHCI driver ---");
		bool support_atapi  = config.xml().attribute_value("atapi", false);
		try {
			driver.construct(env, *this, support_atapi);
			report_ports();
		} catch (Ahci::Missing_controller) {
			error("no AHCI controller found");
			env.parent().exit(~0);
		} catch (Service_denied) {
			error("hardware access denied");
			env.parent().exit(~0);
		}

		env.parent().announce(env.ep().manage(*this));
	}

	void session(unsigned index)
	{
		if (index > Driver::MAX_PORTS || !block_session[index].constructed()) return;
		block_session[index]->handle_requests();
	}

	Session_capability session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		log("new block session: ", args.string(), " size: ", sizeof(Block_session_component));
		Session_label const label = label_from_args(args.string());
		Session_policy const policy(label, config.xml());

		Ram_quota const ram_quota = ram_quota_from_args(args.string());
		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		if (!tx_buf_size)
			throw Service_denied();

		if (tx_buf_size > ram_quota.value) {
			error("insufficient 'ram_quota' from '", label, "',"
			      " got ", ram_quota, ", need ", tx_buf_size);
			throw Insufficient_ram_quota();
		}

		/* try read device port number attribute */
		long device = policy.attribute_value("device", -1L);

		/* try read device model and serial number attributes */
		auto const model  = policy.attribute_value("model",  String<64>());
		auto const serial = policy.attribute_value("serial", String<64>());

		try {
			log(__func__, ":", __LINE__);
			Port &port = driver->port(device, model.string(), serial.string());
			log(__func__, ":", __LINE__);

			if (block_session[port.index].constructed()) {
				error("Device with number=", port.index, " is already in use");
				throw Service_denied();
			}

			log(__func__, ":", __LINE__);
			block_session[port.index].construct(env, port, tx_buf_size);
			log(__func__, ":", __LINE__);
			return block_session[port.index]->cap();
		} catch (...) {
			error("rejecting session request, no matching policy for '", label, "'",
			      " (model=", model, " serial=", serial, " device index=", device, ")");
		}

		throw Service_denied();
	}

	void upgrade(Session_capability, Root::Upgrade_args const&) override { }

	void close(Session_capability cap) override
	{
		for (int index = 0; index < Driver::MAX_PORTS; index++) {
			if (!block_session[index].constructed() || !(cap == block_session[index]->cap()))
				continue;

			block_session[index].destruct();
		}
	}

	void report_ports()
	{
		try {
			Xml_node report = config.xml().sub_node("report");
			if (report.attribute_value("ports", false)) {
				reporter.construct(env, "ports");
				reporter->enabled(true);
				driver->report_ports(*reporter);
			}
		} catch (Xml_node::Nonexistent_sub_node) { }
	}
};

void Component::construct(Genode::Env &env) { static Ahci::Main server(env); }

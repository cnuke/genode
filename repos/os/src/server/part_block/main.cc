/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block_session/rpc_object.h>
#include <block/request_stream.h>
#include <os/session_policy.h>
#include <os/reporter.h>
#include <util/bit_allocator.h>

#include "gpt.h"
#include "mbr.h"
#include "ahdi.h"
#include "disk.h"

namespace Block {
	class Wrapped_session;
	class Main;
};


struct Block::Wrapped_session
{
	Env &_env;

	Parent::Client parent_client { };
	Id_space<Parent::Client>::Element id;

	Block::Session_capability cap;

	Wrapped_session(Env                      &env,
	                Root::Session_args const &args,
	                Affinity           const &affinity)
	:
		_env { env },
		 id  { parent_client, _env.id_space() },
		 cap { _env.session<Block::Session>(id.id(), args, affinity) }
	{ }

	virtual ~Wrapped_session()
	{
		_env.close(id.id());
	}
};


class Block::Main : Rpc_object<Typed_root<Session>>,
                    public Sync_read::Handler
{
	private:

		Partition_table & _table();

		Env &_env;

		Attached_rom_dataspace _config { _env, "config" };

		Sliced_heap                       _heap     { _env.ram(), _env.rm() };
		Constructible<Expanding_reporter> _reporter { };

		Allocator_avl    _block_alloc { &_heap };
		Block_connection _block       { _env, &_block_alloc, 64u << 10 };
		Session::Info    _info        { _block.info() };
		Mbr              _mbr         { *this, _heap, _info };
		Gpt              _gpt         { *this, _heap, _info };
		Ahdi             _ahdi        { *this, _heap, _info };

		Constructible<Disk> _disk { };

		Partition_table & _partition_table { _table() };

		static constexpr unsigned MAX_SESSIONS = 128;
		Wrapped_session *_sessions[MAX_SESSIONS] { };

		Main(Main const &);
		Main &operator = (Main const &);

	public:

		struct No_partition_table : Exception { };
		struct Ambiguous_tables   : Exception { };
		struct Invalid_config     : Exception { };

		Main(Env &env) : _env(env)
		{
			/* announce at parent */
			env.parent().announce(env.ep().manage(*this));
		}

		/***********************
		 ** Session interface **
		 ***********************/

		Genode::Session_capability session(Root::Session_args const &args,
		                                   Affinity const &affinity) override
		{
			long num = -1;
			bool writeable = false;

			Session_label const label = label_from_args(args.string());
			try {
				Session_policy policy(label, _config.xml());

				/* read partition attribute */
				num = policy.attribute_value("partition", -1L);

				/* sessions are not writeable by default */
				writeable = policy.attribute_value("writeable", false);

			} catch (Session_policy::No_policy_defined) {
				error("rejecting session request, no matching policy for '",
				      label, "'");
				throw Service_denied();
			}

			if (num == -1) {
				error("policy does not define partition number for for '", label, "'");
				throw Service_denied();
			}

			if (!_partition_table.partition_valid(num)) {
				error("Partition ", num, " unavailable for '", label, "'");
				throw Service_denied();
			}

			if (num >= MAX_SESSIONS || _sessions[num]) {
				error("Partition ", num, " already in use or session limit reached for '",
				      label, "'");
				throw Service_denied();
			}

			Ram_quota const ram_quota = ram_quota_from_args(args.string());
			size_t tx_buf_size =
				Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

			if (!tx_buf_size)
				throw Service_denied();

			size_t const object_size = sizeof(Block::Wrapped_session);
			size_t const needed = object_size + _heap.overhead(object_size);
			Ram_quota const remaining_ram_quota { ram_quota.value - needed };

			if (tx_buf_size > remaining_ram_quota.value) {
				error("insufficient 'ram_quota', got ", remaining_ram_quota, ", need ",
				     tx_buf_size);
				throw Insufficient_ram_quota();
			}

			Block::Range const range {
				.offset     = size_t(_partition_table.partition_lba(num)),
				.num_blocks = size_t(_partition_table.partition_sectors(num)),
				.writeable  = writeable
			};

			char argbuf[Parent::Session_args::MAX_SIZE];
			copy_cstring(argbuf, args.string(), sizeof(argbuf));

			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota",   remaining_ram_quota.value);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "tx_buf_size", tx_buf_size);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "offset",      range.offset);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "num_blocks",  range.num_blocks);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "writeable",   range.writeable);

			try {
				_sessions[num] = new (_heap) Block::Wrapped_session(_env, argbuf, affinity);
				return _sessions[num]->cap;
			} catch (...) {
				throw Service_denied();
			}
		}

		void close(Genode::Session_capability cap) override
		{
			for (unsigned i = 0; i < MAX_SESSIONS; i++) {
				if (!_sessions[i] || !(cap == _sessions[i]->cap))
					continue;

				destroy(_heap, _sessions[i]);
				_sessions[i] = nullptr;
			}
		}

		void upgrade(Genode::Session_capability, Root::Upgrade_args const&) override
		{
			/*
			 * As the capability is used by the client directly there should be no
			 * upgrades here.
			 */
			Genode::warning("Unexpected session upgrade");
		}

		/************************
		 ** Sync_read::Handler **
		 ************************/

		Block_connection & connection() override { return _block; }

		void block_for_io() override
		{
			_env.ep().wait_and_dispatch_one_io_signal();
		}
};


Block::Partition_table & Block::Main::_table()
{
	Xml_node const config = _config.xml();

	bool const ignore_gpt = config.attribute_value("ignore_gpt", false);
	bool const ignore_mbr = config.attribute_value("ignore_mbr", false);

	bool valid_mbr  = false;
	bool valid_gpt  = false;
	bool pmbr_found = false;
	bool valid_ahdi = false;
	bool report     = false;

	if (ignore_gpt && ignore_mbr) {
		error("invalid configuration: cannot ignore GPT as well as MBR");
		throw Invalid_config();
	}

	config.with_optional_sub_node("report", [&] (Xml_node const &node) {
		report = node.attribute_value("partitions", false); });

	try {
		if (report)
			_reporter.construct(_env, "partitions", "partitions");
	} catch (...) {
		error("cannot construct partitions reporter: abort");
		throw;
	}

	/*
	 * The initial signal handler can be empty as it's only used to deblock
	 * wait_and_dispatch_one_io_signal() in Sync_read.
	 */

	struct Io_dummy { void fn() { }; } io_dummy;
	Io_signal_handler<Io_dummy> handler(_env.ep(), io_dummy, &Io_dummy::fn);
	_block.sigh(handler);

	/*
	 * Try to parse MBR as well as GPT first if not instructued
	 * to ignore either one of them.
	 */

	if (!ignore_mbr) {
		using Parse_result = Mbr::Parse_result;

		switch (_mbr.parse()) {
		case Parse_result::MBR:
			valid_mbr = true;
			break;
		case Parse_result::PROTECTIVE_MBR:
			pmbr_found = true;
			break;
		case Parse_result::NO_MBR:
			break;
		}
	}

	if (!ignore_gpt)
		valid_gpt = _gpt.parse();

	valid_ahdi = _ahdi.parse();

	/*
	 * Both tables are valid (although we would have expected a PMBR in
	 * conjunction with a GPT header - hybrid operation is not supported)
	 * and we will not decide which one to use, it is up to the user.
	 */
	if (valid_mbr && valid_gpt) {
		error("ambigious tables: found valid MBR as well as valid GPT");
		throw Ambiguous_tables();
	}

	if (valid_gpt && !pmbr_found) {
		warning("will use GPT without proper protective MBR");
	}

	if (pmbr_found && ignore_gpt) {
		warning("found protective MBR but GPT is to be ignored");
	}

	auto pick_final_table = [&] () -> Partition_table & {
		if (valid_gpt)  return _gpt;
		if (valid_mbr)  return _mbr;
		if (valid_ahdi) return _ahdi;

		/* fall back to entire disk in partition 0 */
		_disk.construct(*this, _heap, _info);

		return *_disk;
	};

	Partition_table &table = pick_final_table();

	/* generate appropriate report */
	if (_reporter.constructed()) {
		_reporter->generate([&] (Xml_generator &xml) {
			table.generate_report(xml);
		});
	}

	return table;
}

void Component::construct(Genode::Env &env) { static Block::Main main(env); }

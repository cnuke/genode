/*
 * \brief  PRNG VFS plugin employing Xoroshiro128+
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2024-12-09
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/internal/xoroshiro.h>
#include <os/vfs.h>
#include <vfs/single_file_system.h>

using namespace Genode;


struct Entropy_source : Interface
{
	struct Collect_ok    { };
	struct Collect_error { };
	using Collect_result = Attempt<Collect_ok, Collect_error>;

	virtual Collect_result collect(Byte_range_ptr const &dst) = 0;
};


/*
 * A wrapper for the Xoroshiro128+ PRNG that reseeds the PRNG around every
 * 1024 * 1024 + random(0..4095) bytes of generated output.
 */
struct Xoroshiro_128_plus_reseeding
{
	public:

		enum class Query_error : uint32_t { RESEED_FAILED };
		struct     Query_ok { size_t produced_bytes; };

		using Query_result = Attempt<Query_ok, Query_error>;

	private:

		enum : unsigned { CONSUME_THRESHOLD = 1024 * 1024 };

		Entropy_source                    &_entropy_src;
		uint64_t                           _seed            { 0 };
		size_t                             _consumed        { 0 };
		size_t                             _consumed_limit  { 0 };
		Constructible<Xoroshiro_128_plus>  _xoroshiro       { };

		bool _copy_value(char *dst, size_t len)
		{
			/*
			 * The unconsumed state is used as hook to reseed
			 * Xoroshiro_128_plus, thus reset it when hitting
			 * the limit.
			 */

			if (_consumed >= _consumed_limit)
				_consumed = 0;

			if (_consumed == 0) {
				_entropy_src.collect(Byte_range_ptr { (char*)&_seed,
				                                      sizeof(_seed) }).with_result(
					[&] (Entropy_source::Collect_ok) {
						_consumed_limit = CONSUME_THRESHOLD + (_seed & 0xfffu);
						_xoroshiro.construct(_seed);
					},
					[&] (Entropy_source::Collect_error) {
						_xoroshiro.destruct();
					});
			}

			if (!_xoroshiro.constructed())
				return false;

			_consumed += 8;
			uint64_t const value    = _xoroshiro->value();
			size_t   const copy_len = min(len, sizeof(value));

			memcpy(dst, &value, copy_len);
			return true;
		}

	public:

		Xoroshiro_128_plus_reseeding(Entropy_source &entropy_src)
		: _entropy_src { entropy_src } { }

		Query_result query(Byte_range_ptr const &range)
		{
			/*
			 * For now we fill the supplied buffer up one 8 byte value at
			 * a time and pay for the overhead.
			 */

			uint64_t const rounds    = range.num_bytes / sizeof(uint64_t);
			uint64_t const remainder = range.num_bytes % sizeof(uint64_t);

			char *dst = range.start;
			for (uint64_t i = 0; i < rounds; i++) {
				if (!_copy_value(dst, sizeof(uint64_t)))
					return Query_error::RESEED_FAILED;

				dst += sizeof(uint64_t);
			}

			if (remainder) {
				if (!_copy_value(dst, remainder))
					return Query_error::RESEED_FAILED;

				dst += remainder;
			}

			return Query_ok { .produced_bytes = range.num_bytes };
		}
};


namespace Vfs {
	struct Xoroshiro_file_system;
} /* namespace Vfs */


struct Vfs::Xoroshiro_file_system : Single_file_system
{
	Genode::Allocator &_alloc;

	struct Invalid_seed_file_path : Genode::Exception { };

	using File_path = String<256>;
	static File_path _get_seed_file_path(Xml_node const config)
	{
		File_path const path =
			config.attribute_value("seed_file", File_path());
		if (!path.valid()) {
			error("invalid 'seed_file' attribute");
			throw Invalid_seed_file_path();
		}

		return path;
	}

	struct File_entropy_source : Entropy_source
	{
		Readonly_file const _seed_file;

		File_entropy_source(Directory       &root_dir,
		                    File_path const &file_path)
		: _seed_file { root_dir, file_path } { }

		Collect_result collect(Byte_range_ptr const &dst) override
		{
			if (_seed_file.read(Readonly_file::At { 0 }, dst) >= dst.num_bytes)
				return Collect_ok();

			return Collect_error();
		}
	};

	Directory       _root_dir;
	File_path const _seed_file_path;

	struct Xoroshiro_vfs_handle : Single_vfs_handle
	{
		File_entropy_source          _entropy_src;
		Xoroshiro_128_plus_reseeding _xoroshiro;

		Xoroshiro_vfs_handle(Directory_service    &ds,
		                     File_io_service      &fs,
		                     Genode::Allocator    &alloc,
		                     Directory            &root_dir,
		                     File_path      const &seed_file)
		:
			Single_vfs_handle { ds, fs, alloc, 0 },
			_entropy_src      { root_dir, seed_file },
			_xoroshiro        { _entropy_src }
		{ }

		Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
		{
			using Query_ok    = Xoroshiro_128_plus_reseeding::Query_ok;
			using Query_error = Xoroshiro_128_plus_reseeding::Query_error;

			return _xoroshiro.query(dst).convert<Read_result>(
				[&] (Query_ok ok) {
					out_count = ok.produced_bytes;
					return READ_OK;
				},
				[&] (Query_error e) {
					if (e == Query_error::RESEED_FAILED)
						error("xoroshiro reseeding failed");
					return READ_ERR_IO;
				});
		}

		Write_result write(Const_byte_range_ptr const &, size_t &) override {
			return WRITE_ERR_IO; }

		bool read_ready()  const override { return true; }
		bool write_ready() const override { return false; }
	};

	Xoroshiro_file_system(Vfs::Env &vfs_env, Genode::Xml_node config)
	:
		Single_file_system { Node_type::CONTINUOUS_FILE, name(),
		                     Node_rwx::ro(), config },
		_alloc             { vfs_env.alloc() },
		_root_dir          { Directory(vfs_env) },
		_seed_file_path    { _get_seed_file_path(config) }
	{ }

	static char const *name()   { return "xoroshiro"; }
	char const *type() override { return "xoroshiro"; }

	/*********************************
	 ** Directory service interface **
	 *********************************/

	Open_result open(char const  *path, unsigned,
	                 Vfs_handle **out_handle,
	                 Allocator   &alloc) override
	{
		if (!_single_file(path))
			return OPEN_ERR_UNACCESSIBLE;

		try {
			/*
			 * The primary reason for openening the seed-file implicitly
			 * while creating the vfs-handle is done to prevent accessing
			 * it during VFS construction, which will fail.
			 */

			*out_handle =
				new (alloc) Xoroshiro_vfs_handle(*this, *this, alloc,
				                                 _root_dir,
				                                 _seed_file_path);
			return OPEN_OK;
		}
		catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
		catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		/* the seed file cannot be opened */
		catch (Directory::Open_failed) { return OPEN_ERR_UNACCESSIBLE; }
	}

	Stat_result stat(char const *path, Stat &out) override
	{
		Stat_result const result = Single_file_system::stat(path, out);
		/* set an arbitrary size to acommodate users that require one  */
		out.size = 4096;
		return result;
	}
};


struct Xoroshiro_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override
	{
		return new (env.alloc()) Vfs::Xoroshiro_file_system(env, node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Xoroshiro_factory factory;
	return &factory;
}

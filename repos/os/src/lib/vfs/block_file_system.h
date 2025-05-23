/*
 * \brief  Block device file system
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2013-12-20
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <util/xml_generator.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>


namespace Vfs {
	class Block_file_system;
}


struct Vfs::Block_file_system
{
	using Name = String<64>;
	using block_count_t  = Block::block_count_t;
	using block_number_t = Block::block_number_t;
	using off_t = Block::off_t;

	static block_count_t round_to_block_size(size_t const block_size,
	                                         size_t const bytes)
	{
		if (bytes < block_size)
			return 1;

		return Genode::align_addr(bytes, (int)Genode::log2(block_size)) / block_size;
	}

	struct Block_job : Block::Connection<Block_job>::Job
	{
		Byte_range_ptr const range;
		file_size      const seek_offset;

		size_t bytes_handled { 0 };

		bool done    { false };
		bool success { false };

		Block_job(Block::Connection<Block_job> &conn,
		           char *           const  start,
		           size_t           const  num_bytes,
		           file_size        const  seek_offset,
		           Block::Operation const &op)
		:
			Block::Connection<Block_job>::Job {conn, op },
			range       { start, num_bytes },
			seek_offset { seek_offset }
		{ }

		size_t bytes_remaining() const {
			return range.num_bytes - bytes_handled; }
	};

	struct Block_connection : Block::Connection<Block_job>
	{
		Block_connection(auto &&... args) : Block::Connection<Block_job>(args...) { }

		/*****************************************************
		 ** Block::Connection::Update_jobs_policy interface **
		 *****************************************************/

		void produce_write_content(Block_job & job, off_t offset,
		                           char *dst, size_t length)
		{
			size_t sz = min(length, job.bytes_remaining());
			if (!sz)
				return;

			memcpy(dst, (char const *)job.range.start + offset, sz);
			job.bytes_handled += sz;
		}

		void consume_read_result(Block_job & job, off_t offset,
		                         char const *src, size_t length)
		{
			size_t sz = Genode::min(length, job.bytes_remaining());
			if (!sz)
				return;

			memcpy((char *)job.range.start + offset, src, sz);
			job.bytes_handled += sz;
		}

		void completed(Block_job &job, bool success)
		{
			job.success = success;
			job.done    = true;
		}
	};

	struct Local_factory;
	struct Data_file_system;
	struct Compound_file_system;
};


class Vfs::Block_file_system::Data_file_system : public Single_file_system
{
	private:

		/*
		 * Noncopyable
		 */
		Data_file_system(Data_file_system const &);
		Data_file_system &operator = (Data_file_system const &);

		Vfs::Env &_env;

		Block_connection &_block;

		class Block_vfs_handle : public Single_vfs_handle
		{
			private:

				/*
				 * Noncopyable
				 */
				Block_vfs_handle(Block_vfs_handle const &);
				Block_vfs_handle &operator = (Block_vfs_handle const &);

				Block_connection           &_block;
				Block::Session::Info const  _info;
				size_t               const  _request_size_limit;

				Genode::Constructible<Block_job> _job { };

			public:

				Block_vfs_handle(Directory_service          &ds,
				                 File_io_service            &fs,
				                 Allocator                  &alloc,
				                 Block_connection           &block,
				                 Block::Session::Info const &info,
				                 size_t               const  request_size_limit)
				:
					Single_vfs_handle   { ds, fs, alloc, 0 },
					_block              { block },
					_info               { info },
					_request_size_limit { request_size_limit }
				{ }

				bool queue_read(size_t)
				{
					return true;
				}

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					if (_job.constructed() && !_job->done)
						return READ_QUEUED;

					if (_job.constructed() && _job->done) {
						out_count = _job->bytes_handled;
						_job.destruct();
						return READ_OK;
					}

					file_size      const seek_offset  = seek();
					block_number_t const block_number = seek_offset / _info.block_size;
					if (block_number >= _info.block_count) {
						Genode::error(__func__, ": block: ", block_number, " out of reach");
						return READ_ERR_INVALID;
					}

					if (seek_offset % _info.block_size != 0) {
						Genode::error(__func__, ": seek offset: ", Genode::Hex(seek_offset), " "
						                "not aligned to block size: ", Genode::Hex(_info.block_size));
						return READ_ERR_INVALID;
					}

					size_t const size_limit = min(_request_size_limit, dst.num_bytes);

					Block::Operation const op {
						.type         = Block::Operation::Type::READ,
						.block_number = block_number,
						.count        = round_to_block_size(_info.block_size, size_limit)
					};

					_job.construct(_block, dst.start, size_limit, seek_offset, op);
					_block.update_jobs(_block);

					return READ_QUEUED;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					if (!_info.writeable) {
						Genode::error("block device is not writeable");
						return WRITE_ERR_INVALID;
					}

					if (_job.constructed() && !_job->done)
						return WRITE_ERR_WOULD_BLOCK;

					if (_job.constructed() && _job->done) {
						out_count = _job->bytes_handled;
						_job.destruct();
						return WRITE_OK;
					}

					file_size      const seek_offset  = seek();
					block_number_t const block_number = seek_offset / _info.block_size;
					if (block_number >= _info.block_count) {
						Genode::error(__func__, ": block: ", block_number, " out of reach");
						return WRITE_ERR_INVALID;
					}

					if (seek_offset % _info.block_size != 0) {
						Genode::error(__func__, ": seek offset: ", Genode::Hex(seek_offset), " "
						                "not aligned to block size: ", Genode::Hex(_info.block_size));
						return WRITE_ERR_INVALID;
					}

					size_t const size_limit = min(_request_size_limit, src.num_bytes);

					Block::Operation const op {
						.type         = Block::Operation::Type::WRITE,
						.block_number = block_number,
						.count        = round_to_block_size(_info.block_size, size_limit)
					};

					_job.construct(_block, const_cast<char*>(src.start), size_limit, seek_offset, op);
					_block.update_jobs(_block);

					// XXX force libc to call use again and hopefully nobody uses O_NONBLOCK
					return WRITE_ERR_WOULD_BLOCK;
				}

				Sync_result sync() override
				{
					return SYNC_OK;
				}

				bool read_ready()  const override
				{
					return _job.constructed();
				}

				bool write_ready() const override
				{
					return _job.constructed();
				}
		};

		Block::Session::Info const _info;
		size_t               const _io_buffer_size;

	public:

		Data_file_system(Vfs::Env                &env,
		                 Block_connection        &block,
		                 Name              const &name,
		                 size_t                   io_buffer_size)
		:
			Single_file_system { Node_type::CONTINUOUS_FILE, name.string(),
			                     block.info().writeable ? Node_rwx::rw() : Node_rwx::ro(),
			                     Genode::Xml_node("<data/>") },
			_env            { env },
			_block          { block },
			_info           { _block.info() },
			_io_buffer_size { io_buffer_size }
		{ }

		~Data_file_system() { }

		static char const *name()   { return "data"; }
		char const *type() override { return "data"; }

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
				*out_handle = new (alloc) Block_vfs_handle(*this, *this, alloc,
				                                           _block, _info,
				                                           _io_buffer_size / 2);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);
			out.size = _info.block_count * _info.block_size;
			return result;
		}

		Unlink_result unlink(char const *path) override {
			return _single_file(path) ? UNLINK_OK : UNLINK_ERR_NO_ENTRY; }

		/********************************
		 ** File I/O service interface **
		 ********************************/

		virtual bool queue_read(Vfs_handle *vfs_handle, size_t size) override
		{
			Block_vfs_handle *handle =
				static_cast<Block_vfs_handle*>(vfs_handle);
			return handle->queue_read(size);
		}

		Read_result complete_read(Vfs_handle *vfs_handle, Byte_range_ptr const &dst,
		                          size_t &out_count) override
		{
			Block_vfs_handle *handle =
				static_cast<Block_vfs_handle*>(vfs_handle);
			return handle->read(dst, out_count);
		}

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs::Block_file_system::Local_factory : File_system_factory
{
	using Label = Genode::String<64>;
	Label const _label;

	Name const _name;

	Vfs::Env &_env;

	Genode::Allocator_avl _tx_block_alloc { &_env.alloc() };

	Block_connection _block;

	Block::Session::Info const _info { _block.info() };

	Genode::Io_signal_handler<Local_factory> _block_signal_handler {
		_env.env().ep(), *this, &Local_factory::_handle_block_signal };

	void _handle_block_signal()
	{
		_block.update_jobs(_block);
		_env.user().wakeup_vfs_user();
	}

	Data_file_system _data_fs;
	
	struct Info : Block::Session::Info
	{
		void print(Genode::Output &out) const
		{
			char buf[128] { };
			Genode::Xml_generator::generate({ buf, sizeof(buf) }, "block",
				[&] (Genode::Xml_generator &xml) {
					xml.attribute("count", Block::Session::Info::block_count);
					xml.attribute("size",  Block::Session::Info::block_size);
			}).with_error([] (Genode::Buffer_error) {
				Genode::warning("VFS-block info exceeds maximum buffer size");
			});
			Genode::print(out, Genode::Cstring(buf));
		}
	};

	Readonly_value_file_system<Info>             _info_fs        { "info",        Info { } };
	Readonly_value_file_system<Genode::uint64_t> _block_count_fs { "block_count", 0 };
	Readonly_value_file_system<size_t>           _block_size_fs  { "block_size",  0 };

	static Name name(Xml_node const &config) {
		return config.attribute_value("name", Name("block")); }

	/* payload + packstream metadata */
	static constexpr size_t DEFAULT_IO_BUFFER_SIZE = (4u << 20);

	static size_t io_buffer(Xml_node const &config) {
		return config.attribute_value("io_buffer", DEFAULT_IO_BUFFER_SIZE); }

	Local_factory(Vfs::Env &env, Xml_node const &config)
	:
		_label   { config.attribute_value("label", Label("")) },
		_name    { name(config) },
		_env     { env },
		_block   { _env.env(), &_tx_block_alloc, io_buffer(config) + (64u << 10),
		           _label.string() },
		_data_fs { _env, _block, name(config), io_buffer(config) }
	{
		_block.sigh(_block_signal_handler);
		_info_fs       .value(Info { _info });
		_block_count_fs.value(_info.block_count);
		_block_size_fs .value(_info.block_size);
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node const &node) override
	{
		if (node.has_type("data"))        return &_data_fs;
		if (node.has_type("info"))        return &_info_fs;
		if (node.has_type("block_count")) return &_block_count_fs;
		if (node.has_type("block_size"))  return &_block_size_fs;
		return nullptr;
	}
};


class Vfs::Block_file_system::Compound_file_system : private Local_factory,
                                                     public  Vfs::Dir_file_system
{
	private:

		using Name = Block_file_system::Name;

		using Config = String<200>;
		static Config _config(Name const &name)
		{
			char buf[Config::capacity()] { };

			/*
			 * By not using the node type "dir", we operate the
			 * 'Dir_file_system' in root mode, allowing multiple sibling nodes
			 * to be present at the mount point.
			 */
			Genode::Xml_generator::generate({ buf, sizeof(buf) }, "compound",
				[&] (Genode::Xml_generator &xml) {

					xml.node("data", [&] { xml.attribute("name", name); });

					xml.node("dir", [&] {
						xml.attribute("name", Name(".", name));
						xml.node("info");
						xml.node("block_count");
						xml.node("block_size");
					});

			}).with_error([&] (Genode::Buffer_error) {
				Genode::warning("VFS-block compound exceeds maximum buffer size");
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Compound_file_system(Vfs::Env &vfs_env, Genode::Xml_node const &node)
		:
			Local_factory { vfs_env, node },
			Vfs::Dir_file_system { vfs_env,
			                       Xml_node(_config(Local_factory::name(node)).string()),
			                       *this }
		{ }

		static const char *name() { return "block"; }

		char const *type() override { return name(); }
};

#endif /* _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_ */

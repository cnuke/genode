/*
 * \brief  Resolvconf filesystem
 * \author Josef Soentgen
 * \date   2023-11-08
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <os/vfs.h>
#include <vfs/file_system_factory.h>
#include <vfs/single_file_system.h>


namespace Vfs {

	using namespace Genode;

	class Resolvconf_file_system;
}


struct Vfs::Resolvconf_file_system : Single_file_system
{
	Vfs::Env          &_vfs_env;

	Resolvconf_file_system(Resolvconf_file_system const &) = delete;
	Resolvconf_file_system &operator=(Resolvconf_file_system const&) = delete;

	static constexpr char const * const _nameserver_string = "nameserver ";

	using Local_path = String<255 + 1>;
	using File_name  = String< 31 + 1>;

	static Local_path nameserver_file(Xml_node config)
	{
		return config.attribute_value("nameserver_file",
		                              Local_path("/socket/nameserver"));
	}

	Local_path const _nameserver_file;

	static File_name name(Xml_node config)
	{
		return config.attribute_value("name",
		                              File_name("resolv.conf"));
	}

	Resolvconf_file_system(Vfs::Env & env, Xml_node config)
	:
		Single_file_system(Node_type::TRANSACTIONAL_FILE,
		                   name(config).string(),
		                   Node_rwx::ro(), config),
		_vfs_env { env },
		_nameserver_file { nameserver_file(config) }
	{ }

	static char const *name()   { return "resolvconf"; }
	char const *type() override { return "resolvconf"; }

	struct Resolvconf_vfs_handle : Single_vfs_handle
	{
		Allocator        &_alloc;
		Directory  const  _dir;
		Local_path const &_file_path;

		Resolvconf_vfs_handle(Directory_service &ds,
		                      File_io_service   &fs,
		                      Genode::Allocator &alloc,
		                      Vfs::Env          &vfs_env,
		                      Local_path  const &file_path)
		:
			Single_vfs_handle(ds, fs, alloc, 0),
			_alloc { alloc }, _dir { vfs_env },
			_file_path { file_path }
		{ }

		Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
		{
			if (seek() != 0)
				return Read_result::READ_ERR_INVALID;

			Read_result result = Read_result::READ_ERR_IO;

			try {
				Genode::File_content const content {
					_alloc, _dir, _file_path.string(),
					Genode::File_content::Limit { 4096 } };

				/* the nameserver file must only contain exactly * 1 entry  */
				content.bytes([&] (char const *ptr, size_t size) {

					String<160> const content { _nameserver_string,
					                            Cstring { ptr, size } };

					/* omit trailing NUL */
					size_t const content_size = content.length() - 1;

					if (content_size > dst.num_bytes) {
						out_count = 0;
						result = Read_result::READ_ERR_INVALID;
						return;
					}

					memcpy(dst.start, content.string(), content_size);

					out_count = content_size;
					result    = Read_result::READ_OK;
				});
			} catch (...) {
				/* Open_failed, Nonexistent_file and Truncated_during_read */
			}

			return result;
		}

		Write_result write(Const_byte_range_ptr const &, size_t &) override
		{
			return WRITE_ERR_INVALID;
		}

		bool read_ready()  const override { return true; }
		bool write_ready() const override { return false; }
	};

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
			*out_handle =
				new (alloc) Resolvconf_vfs_handle(*this, *this, alloc,
				                                  _vfs_env,
				                                  _nameserver_file);
			return OPEN_OK;
		}
		catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
		catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
	}

	Stat_result stat(char const *path, Stat &out) override
	{
		Stat_result const result = Single_file_system::stat(path, out);

		Stat nameserver_file_stat;
		Stat_result const nameserver_file_result =
			_vfs_env.root_dir().stat(_nameserver_file.string(),
			                         nameserver_file_stat);

		if (nameserver_file_result == Stat_result::STAT_OK)
			out.size = nameserver_file_stat.size;

		out.size += strlen(_nameserver_string);

		return result;
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc())
				Vfs::Resolvconf_file_system(env, config);
		}
	};

	static Factory f;
	return &f;
}

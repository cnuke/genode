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
#include <vfs/readonly_value_file_system.h>


namespace Vfs {

	using namespace Genode;

	class Resolvconf_file_system;
}


class Vfs::Resolvconf_file_system : public Single_file_system
{
	private:

		Resolvconf_file_system(Resolvconf_file_system const &) = delete;
		Resolvconf_file_system &operator=(Resolvconf_file_system const&) = delete;

		using Local_path = String<256>;
		using Name       = String<64>;
		using Value      = String<64>;

		static Local_path file_path(Xml_node config) {
			return config.attribute_value("nameserver_file",
			                              Local_path("/socket/nameserver")); }

		static Name name(Xml_node config) {
			return config.attribute_value("name", Name("resolv.conf")); }

		Vfs::Env                                 &_vfs_env;
		Local_path                         const  _file_path;
		Readonly_value_file_system<Value, 64>     _ro_value_fs;

	public:

		Resolvconf_file_system(Vfs::Env & env, Xml_node config)
			:
				Single_file_system(Node_type::TRANSACTIONAL_FILE,
				                   name(config).string(),
				                   Node_rwx::ro(), config),
				_vfs_env     { env },
				_file_path   { file_path(config) },
				_ro_value_fs { name(config).string(), "" }
		{
			/*
			 * We rely on the called VFS plugin to block until it is
			 * able to satisfy our request.
			 */

			try {
				/*
				 * The nameserver file must only contain exactly
				 * one nameserver entry in the form of 'x.y.z.w\n'.
				 */
				Genode::File_content const content {
					_vfs_env.alloc(), Directory { _vfs_env },
					_file_path.string(),
					Genode::File_content::Limit { 32 } };

				content.bytes([&] (char const *ptr, size_t size) {

					Value const value { "nameserver ",
						Cstring { ptr, size } };

					_ro_value_fs.value(value);
				});
			} catch (...) {
				/* Open_failed, Nonexistent_file and Truncated_during_read */
			}
		}

		static char const *name()   { return "resolvconf"; }
		char const *type() override { return "resolvconf"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override {
			return _ro_value_fs.open(path, 0, out_handle, alloc); }

		Stat_result stat(char const *path, Stat &out) override {
			return _ro_value_fs.stat(path, out); }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc()) Vfs::Resolvconf_file_system(env, config);
		}
	};

	static Factory f;
	return &f;
}

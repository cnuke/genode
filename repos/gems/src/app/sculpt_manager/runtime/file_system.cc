/*
 * \brief  XML configuration for file-system server
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

static void gen_vfs_lwext4(Xml_generator        &xml,
                           Storage_target const &target)
{
	size_t const ex_cache_size = 32u << 20;
	gen_common_start_content(xml, target.fs(),
	                         Cap_quota{150},
	                         /* 16MiB to be on the safe side because of journal growth */
	                         Ram_quota{16*1024*1024 + ex_cache_size},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "vfs");

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] () {
		xml.attribute("ld_verbose", true);
		xml.node("vfs", [&] () {
			xml.node("dir", [&] () {
				xml.attribute("name", "dev");
				xml.node("block", [&] () {
					xml.attribute("block_buffer_count", 128);
				});
			});
			xml.node("lwext4", [&] () {
				xml.attribute("block_device", "/dev/block");
				xml.attribute("expand_via_io", "yes");
				xml.attribute("reporting", "yes");
					xml.attribute("report_cache", "yes");
				xml.attribute("cache_write_back", "yes");
				xml.attribute("writeable", "yes");
				xml.attribute("external_cache_size", ex_cache_size);
			});
		});
		xml.node("default-policy", [&] () {
			xml.attribute("root", "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] () {
		target.gen_block_session_route(xml);
		gen_parent_rom_route(xml, "vfs");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "vfs_lwext4.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Rm_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
		gen_parent_route<Report::Session>(xml);
	});
}

void Sculpt::gen_fs_start_content(Xml_generator        &xml,
                                  Storage_target const &target,
                                  File_system::Type     fs_type)
{
	if (fs_type == File_system::EXT4) {
		gen_vfs_lwext4(xml, target);
		return;
	}

	gen_common_start_content(xml, target.fs(),
	                         Cap_quota{400}, Ram_quota{64*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "vfs");

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] () {
		xml.node("vfs", [&] () {
			xml.node("rump", [&] () {
				switch (fs_type) {
				case File_system::EXT2:  xml.attribute("fs", "ext2fs"); break;
				case File_system::EXT4:  /* handled above */            break;
				case File_system::FAT32: xml.attribute("fs", "msdos");  break;
				case File_system::GEMDOS:
					xml.attribute("fs",     "msdos");
					xml.attribute("gemdos", "yes");
					break;
				case File_system::UNKNOWN: break;
				};
				xml.attribute("ram", "48M");
				xml.attribute("writeable", "yes");
			});
		});
		xml.node("default-policy", [&] () {
			xml.attribute("root", "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] () {
		target.gen_block_session_route(xml);
		gen_parent_rom_route(xml, "vfs");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "vfs_rump.lib.so");
		gen_parent_rom_route(xml, "rump.lib.so");
		gen_parent_rom_route(xml, "rump_fs.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Rm_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
	});
}

/*
 * \brief  Ext4_fs test
 * \author Josef Soentgen
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <file_system/util.h>
#include <file_system_session/connection.h>
#include <os/server.h>


static char const *type(File_system::Directory_entry::Type t)
{
	switch (t) {
		case File_system::Directory_entry::Type::TYPE_FILE:      return "f";
		case File_system::Directory_entry::Type::TYPE_DIRECTORY: return "d";
		case File_system::Directory_entry::Type::TYPE_SYMLINK:   return "s";
	}   
	return "-";
}


struct Test
{
	File_system::Connection &fs;

	Test(File_system::Connection &fs) : fs(fs) { }

	bool readdir()
	{
		File_system::Dir_handle dir = fs.dir("/", false);
		File_system::Handle_guard dir_guard(fs, dir);

		enum { BUFFER_SIZE = 16384 };
		char *buffer = (char*)Genode::env()->heap()->alloc(BUFFER_SIZE);
		File_system::seek_off_t offset = 0;

		PDBG("bar");
		Genode::size_t res = File_system::read(fs, dir, buffer, BUFFER_SIZE, offset);
		if (res > 0) {
			using namespace File_system;

			Directory_entry *de = reinterpret_cast<Directory_entry*>(buffer);
			size_t const entries = res / sizeof(Directory_entry);
			for (size_t i = 0; i < entries; i++) {
				PLOG("%s '%s'", type(de[i].type), de[i].name);
			}
		}

		Genode::env()->heap()->free(buffer, BUFFER_SIZE);

		return !!res;
	}

	bool readfile()
	{
		// using namespace File_system;

		File_system::Dir_handle dir = fs.dir("/", false);
		File_system::Handle_guard dir_guard(fs, dir);

		File_system::File_handle file = fs.file(dir, "UnixEditionZero.txt",
                                                File_system::Mode::READ_ONLY, false);
		File_system::Handle_guard file_guard(fs, file);

        return false;
	}
};


struct Main
{
	Server::Entrypoint &ep;

	Genode::Allocator_avl   fs_alloc { Genode::env()->heap() };
	File_system::Connection fs {
		fs_alloc,
		File_system::DEFAULT_TX_BUF_SIZE, "", "/", false };

	Test test { fs };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		PINF("--- Ext4_fs test ---");

		if (!test.readdir()) {
			PERR("Readdir test failed");
			return;
		}

		if (!test.readfile()) {
			PERR("Readfile test failed");
			return;
		}
	}
};


namespace Server {
	char const *name()             { return "ext4_fs_ep";        }
	size_t stack_size()            { return 8*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main main(ep);       }
}

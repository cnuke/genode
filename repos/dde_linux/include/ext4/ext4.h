/*
 * \brief  Ext4 frontend
 * \author Josef Soentgen
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE_EXT4_EXT4_H_
#define _INCLUDE_EXT4_EXT4_H_

/* Genode includes */
#include <base/stdint.h>
#include <os/server.h>

namespace File_system {
	struct Directory;
	struct File;
}


/* Linux forward decl */
struct inode;
struct dentry;


namespace Ext4 {

	using namespace Genode;

	/********************
	 ** Initialization **
	 ********************/

	bool init(Server::Entrypoint &, Allocator &, Signal_transmitter &);

	void schedule_task();

	struct Completion
	{
		virtual void complete(Completion*,size_t) = 0;
	};

	File_system::Directory *root_dir();

	void free_dentry(struct dentry*);

	void read_directory(Completion*, struct inode *, uint64_t, char *, size_t);
	void open_file(Completion*, struct inode *, char const *, unsigned, bool);

}

#endif /* _INCLUDE_EXT4_EXT4_H_ */

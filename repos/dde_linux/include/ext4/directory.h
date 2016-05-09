/*
 * \brief  File-system directory node
 * \author Josef Soentgen
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* local includes */
#include <lx_emul.h>

/* Ext4 includes */
#include <ext4/node.h>
#include <ext4/file.h>


namespace File_system {
	using namespace Genode;
	class Directory;
}


class File_system::Directory : public Node
{
	private:

		Allocator     &_alloc;
		struct dentry *_dentry;

	public:

		Directory(Allocator &alloc, struct dentry *dentry)
		: Node(dentry->d_inode), _alloc(alloc), _dentry(dentry) { }

		virtual ~Directory() { }

		void file(Ext4::Completion *completion, Name const &name, Mode mode, bool create)
		{
			struct inode *inode = _dentry->d_inode;
			Ext4::open_file(completion, inode, name.string(), mode, create);
		}

		/********************
		 ** Node interface **
		 ********************/

		void read(Ext4::Completion *completion, char *dst, size_t len, seek_off_t seek_offset)
		{
			struct inode *inode = _dentry->d_inode;

			Ext4::read_directory(completion, inode, seek_offset, dst, len);
		}

		void write(Ext4::Completion *completion, char const *src, size_t len, seek_off_t seek_offset)
		{
			/* writing to directory nodes is not supported */
			completion->complete(completion, 0);
		}

		/**
		 * Get number of directory entries
		 *
		 * Used by File_system::status()
		 */
		size_t num_entries() const { return 0; }
};

#endif /* _DIRECTORY_H_ */

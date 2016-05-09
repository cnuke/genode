/*
 * \brief  Symlink-system file node
 * \author Josef Soentgen
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* local includes */
#include <lx_emul.h>

#include <ext4/node.h>


namespace File_system {
	using namespace Genode;
	class Symlink;
}


class File_system::Symlink : public Node
{
	private:

		struct dentry *_dentry;

	public:

		Symlink(struct dentry *dentry)
		: Node(dentry->d_inode), _dentry(dentry) { }

		virtual ~Symlink() { }

		/********************
		 ** Node interface **
		 ********************/

		void read(Ext4::Completion *completion, char *dst, size_t len, seek_off_t seek_offset)
		{
			completion->complete(completion, 0);
		}

		void write(Ext4::Completion *completion, char const *src, size_t len, seek_off_t seek_offset)
		{
			completion->complete(completion, 0);
		}
};

#endif /* _SYMLINK_H_ */

/*
 * \brief  File-system file node
 * \author Josef Soentgen
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FILE_H_
#define _FILE_H_

/* local includes */
#include <lx_emul.h>

#include <ext4/node.h>


namespace File_system {
	using namespace Genode;
	class File;
}


class File_system::File : public Node
{
	private:

		struct dentry *_dentry;

	public:

		File(struct dentry *dentry)
		: Node(dentry->d_inode), _dentry(dentry) { }

		virtual ~File() { Ext4::free_dentry(_dentry); }

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

#endif /* _FILE_H_ */

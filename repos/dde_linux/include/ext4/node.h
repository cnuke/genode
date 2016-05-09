/*
 * \brief  File-system node
 * \author Josef Soentgen
 * \date   2016-05-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* General includes */
#include <file_system/node.h>
#include <util/list.h>

/* Ext4 include */
#include <ext4/ext4.h>

namespace File_system {
	using namespace Genode;
	class Node;
}

struct inode;

class File_system::Node : public Node_base, public List<Node>::Element
{
	private:

		struct inode *_inode;

	public:

		Node(struct inode *inode) : _inode(inode) { }

		struct inode const *inode() { return _inode; }

		/********************
		 ** Node interface **
		 ********************/

		virtual void read(Ext4::Completion*, char *, size_t, seek_off_t) = 0;
		virtual void write(Ext4::Completion*, char const *, size_t, seek_off_t) = 0;
};

#endif /* _NODE_H_ */

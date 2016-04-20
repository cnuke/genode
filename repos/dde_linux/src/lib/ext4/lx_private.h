/**
 * \brief  Linux emulation private code
 * \author Josef Soentgen
 * \date   2016-04-28
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_PRIVATE_H_
#define _LX_PRIVATE_H_

namespace Lx {

	enum { MAX_FS_LIST = 4 };
	extern struct file_system_type *fs_list[MAX_FS_LIST];

	extern struct block_device *block_device;
	int read_block(struct super_block *, sector_t, unsigned, char *, unsigned);
}

#endif /* _LX_PRIVATE_H_ */

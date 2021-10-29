/*
 * \brief  Replaces fs/namespace.c
 * \author Josef Soentgen
 * \date   2021-10-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/mount.h>
#include <linux/fs.h>
#include <linux/slab.h>

struct vfsmount * kern_mount(struct file_system_type * type)
{
	struct vfsmount *m;

	m = kzalloc(sizeof (struct vfsmount), 0);
	if (!m) {
		return (struct vfsmount*)ERR_PTR(-ENOMEM);
	}

	return m;
}

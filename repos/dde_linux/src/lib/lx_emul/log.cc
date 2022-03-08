/*
 * \brief  Linux Kernel log messages
 * \author Stefan Kalkowski
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/log.h>

struct task_struct;

extern "C" struct task_struct * lx_emul_task_get_current(void);
extern "C" char const * lx_emul_task_get_name2(struct task_struct const * t, char name_buffer[64]);

// static char name_buffer[64];

extern "C" void lx_emul_vprintf(char const *fmt, va_list va) {
	// char name_buffer[64];
	// if (lx_emul_task_get_name2(lx_emul_task_get_current(), name_buffer))
	// 	Genode::log((char const*)name_buffer);

	 Lx_kit::env().console.vprintf(fmt, va); }

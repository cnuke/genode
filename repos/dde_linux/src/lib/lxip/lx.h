/*
 * \brief  Lx env
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2014-10-17
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_H_
#define _LX_H_

#ifdef __cplusplus

#include <timer/timeout.h>
#include <base/signal.h>

namespace Lx_kit { class Env; }

namespace Lx {

	void nic_client_init(Genode::Env &env,
	                     Genode::Allocator &alloc,
	                     void (*ticker)());

	void timer_init(Genode::Entrypoint  &ep,
	                ::Timer::Connection &timer,
	                Genode::Allocator   &alloc,
	                void (*ticker)());

	void timer_update_jiffies();

	void lxcc_emul_init(Lx_kit::Env &env);
}

#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef void * lxip_config_info_context_t;
typedef void (*lxip_config_info_callback_t)(lxip_config_info_context_t);

void lxip_init(lxip_config_info_callback_t, lxip_config_info_context_t);

void lxip_configure_static(char const *addr,
                               char const *netmask,
                               char const *gateway,
                               char const *nameserver);
void lxip_configure_dhcp();
void lxip_configure_mtu(unsigned mtu);

bool lxip_do_dhcp();

#ifdef __cplusplus
}
#endif

#endif /* _LX_H_ */

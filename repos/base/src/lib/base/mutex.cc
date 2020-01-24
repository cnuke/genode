/*
 * \brief  Mutex primitives
 * \author Alexander Boettcher
 * \date   2020-01-24
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/mutex.h>
#include <base/log.h>

void Genode::Mutex::acquire()
{
	if (_lock.lock_owner())
		Genode::error("deadlock ahead, mutex=", this, ", return ip=",
			      __builtin_return_address(0));
	_lock.lock();
}

void Genode::Mutex::release()
{
	if (!_lock.lock_owner()) {
		Genode::error("denied non mutex owner the release, mutex=",
		              this, ", return ip=",
			      __builtin_return_address(0));
		return;
	}
	_lock.unlock();
}

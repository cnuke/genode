/*
 * \brief  Pthread handling
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__PTHREAD_POOL_H_
#define _LIBC__INTERNAL__PTHREAD_POOL_H_

/* libc-internal includes */
#include <internal/suspend.h>

namespace Libc { class Pthread_pool; }


struct Libc::Pthread_pool
{
	struct Pthread : Timeout_handler
	{
		Blockade blockade;

		Pthread *next { nullptr };

		Timer_accessor         &_timer_accessor;
		Constructible<Timeout>  _timeout;

		void _construct_timeout_once()
		{
			if (!_timeout.constructed())
				_timeout.construct(_timer_accessor, *this);
		}

		Pthread(Timer_accessor &timer_accessor, Genode::Microseconds timeout)
		: _timer_accessor(timer_accessor)
		{
			if (timeout.value > 0) {
				_construct_timeout_once();
				_timeout->start(timeout);
			}
		}

		Genode::Microseconds duration_left()
		{
			_construct_timeout_once();
			return _timeout->duration_left();
		}

		void handle_timeout() override
		{
			blockade.wakeup();
		}
	};

	Mutex           mutex;
	Pthread        *pthreads = nullptr;
	Timer_accessor &timer_accessor;


	Pthread_pool(Timer_accessor &timer_accessor)
	: timer_accessor(timer_accessor) { }

	void resume_all()
	{
		Mutex::Guard g(mutex);

		for (Pthread *p = pthreads; p; p = p->next)
			p->blockade.wakeup();
	}

	Genode::Microseconds suspend_myself(Suspend_functor & check, Genode::Microseconds timeout)
	{
		Pthread myself { timer_accessor, timeout };
		{
			Mutex::Guard g(mutex);

			myself.next = pthreads;
			pthreads    = &myself;
		}

		if (check.suspend())
			myself.blockade.block();

		{
			Mutex::Guard g(mutex);

			/* address of pointer to next pthread allows to change the head */
			for (Pthread **next = &pthreads; *next; next = &(*next)->next) {
				if (*next == &myself) {
					*next = myself.next;
					break;
				}
			}
		}

		return timeout.value > 0 ? myself.duration_left() : Genode::Microseconds { 0 };
	}
};

#endif /* _LIBC__INTERNAL__PTHREAD_POOL_H_ */

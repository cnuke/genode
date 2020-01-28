/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/tslab.h>
#include <timer_session/connection.h>

/* local includes */
#include <list.h>
#include <bsd.h>
#include <bsd_emul.h>


static Genode::uint64_t microseconds;
static Bsd::Task *_sleep_task;


namespace Bsd {
	class Timer;
}

/**
 * Bsd::Timer
 */
class Bsd::Timer
{
	private:

		::Timer::Connection                   _timer_conn_old;
		::Timer::Connection                   _timer_conn;
		::Timer::One_shot_timeout<Bsd::Timer> _delay_timeout;

		/**
		 * Handle trigger_once signal
		 */
		void _handle_delay_timeout(Genode::Duration)
		{
			update_time();

			// Genode::error(__func__, ":", __LINE__);
			if (_sleep_task) {
				_sleep_task->unblock();
				_sleep_task = nullptr;
			}
			Bsd::scheduler().schedule();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Env &env)
		:
			_timer_conn_old(env),
			_timer_conn(env),
			_delay_timeout(_timer_conn, *this, &Bsd::Timer::_handle_delay_timeout)
		{
		}

		/**
		 * Update time counter
		 */
		void update_time()
		{
			microseconds = _timer_conn.curr_time().trunc_to_plain_us().value;
		}

		void delay(Genode::Microseconds us)
		{
			_delay_timeout.schedule(us);
		}

		void delay(Genode::uint64_t us)
		{
			_timer_conn_old.usleep(us);
		}
};


static Bsd::Timer *_bsd_timer;


void Bsd::timer_init(Genode::Env &env)
{
	/* XXX safer way preventing possible nullptr access? */
	static Bsd::Timer bsd_timer(env);
	_bsd_timer = &bsd_timer;

	/* initialize value explicitly */
	microseconds = 0;
}


void Bsd::update_time()
{
	_bsd_timer->update_time();
}


/*****************
 ** sys/systm.h **
 *****************/

extern "C" int msleep(const volatile void *ident, struct mutex *mtx,
                      int priority, const char *wmesg, int timo)
{
	Genode::error(__func__, ":", __LINE__);
	if (_sleep_task) {
		Genode::error("_sleep_task is not null, current task: ",
		              Bsd::scheduler().current()->name());
		Genode::sleep_forever();
	}

	_sleep_task = Bsd::scheduler().current();
	_sleep_task->block_and_schedule();

	return 0;
}

extern "C" void wakeup(const volatile void *ident)
{
	Genode::error(__func__, ":", __LINE__);

	if (!_sleep_task) {
		Genode::error("sleep task is NULL");
		Genode::sleep_forever();
	}
	_sleep_task->unblock();
	_sleep_task = nullptr;
}


/*********************
 ** machine/param.h **
 *********************/

extern "C" void delay(int delay)
{
#if 1
	_bsd_timer->delay((Genode::uint64_t)delay);
	Genode::error(__func__, ":", __LINE__, ": OLD delay: ", delay, " from: ", __builtin_return_address(0));
#else
	Genode::error(__func__, ":", __LINE__, ": NEW delay: ", delay, " from: ", __builtin_return_address(0));
	if (_sleep_task) {
		Genode::error("_sleep_task is not null, current task: ",
		              Bsd::scheduler().current()->name());
		Genode::sleep_forever();
	}

	_sleep_task = Bsd::scheduler().current();
	_bsd_timer->delay(Genode::Microseconds { (Genode::uint64_t)delay * 10 });
	_sleep_task->block_and_schedule();
#endif
}


/****************
 ** sys/time.h **
 ****************/

void microuptime(struct timeval *tv)
{
	_bsd_timer->update_time();

	if (!tv) { return; }

	/*
	 * So far only needed by auich_calibrate, which
	 * reuqires microseconds - switching the Bsd::Timer
	 * implementation over to the new Genode::Timer API
	 * is probably necessary for that to work properly.
	 */
	tv->tv_sec  = microseconds / (1000*1000);
	tv->tv_usec = microseconds % (1000*1000);
}

/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
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
#include <scheduler.h>


namespace Bsd {
	class Timer;
}

/**
 * Bsd::Timer
 */
class Bsd::Timer
{
	private:

		/*
		 * Use timer session for delay handling because we must prevent
		 * the calling task and thereby the EP from handling signals.
		 * Otherwise the interrupt task could be executed behind the
		 * suspended task, which leads to problems in the contrib source.
		 */
		::Timer::Connection _delay_timer;

		::Timer::Connection _timer;
		Genode::uint64_t    _microseconds;

		Bsd::Task *_sleep_task;

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Env &env)
		:
			_delay_timer(env),
			_timer(env),
			_microseconds(_timer.curr_time().trunc_to_plain_us().value),
			_sleep_task(nullptr)
		{ }

		/**
		 * Update time counter
		 */
		void update_time()
		{
			_microseconds = _timer.curr_time().trunc_to_plain_us().value;
		}

		/**
		 * Return current microseconds
		 */
		Genode::uint64_t microseconds() const
		{
			return _microseconds;
		}

		/**
		 * Block until delay is reached
		 */
		void delay(Genode::uint64_t us)
		{
			_delay_timer.usleep(us);
		}

		/**
		 * Return pointer for currently sleeping task
		 */
		Bsd::Task *sleep_task() const
		{
			return _sleep_task;
		}

		/**
		 * Set sleep task
		 *
		 * If the argment is 'nullptr' the task is reset.
		 */
		void sleep_task(Bsd::Task *task)
		{
			_sleep_task = task;
		}
};


static Bsd::Timer *_bsd_timer;


void Bsd::timer_init(Genode::Env &env)
{
	static Bsd::Timer bsd_timer(env);
	_bsd_timer = &bsd_timer;
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
	Bsd::Task *sleep_task = _bsd_timer->sleep_task();

	if (sleep_task) {
		Genode::error("sleep_task is not null, current task: ",
		              Bsd::scheduler().current()->name());
		Genode::sleep_forever();
	}

	sleep_task = Bsd::scheduler().current();
	_bsd_timer->sleep_task(sleep_task);
	sleep_task->block_and_schedule();

	return 0;
}


extern "C" void wakeup(const volatile void *ident)
{
	Bsd::Task *sleep_task = _bsd_timer->sleep_task();

	if (!sleep_task) {
		Genode::error("sleep task is NULL");
		Genode::sleep_forever();
	}

	sleep_task->unblock();
	_bsd_timer->sleep_task(nullptr);
}


/*********************
 ** machine/param.h **
 *********************/

extern "C" void delay(int delay)
{
	_bsd_timer->delay((Genode::uint64_t)delay);
}


/****************
 ** sys/time.h **
 ****************/

void microuptime(struct timeval *tv)
{
	/* always update the time */
	_bsd_timer->update_time();

	if (!tv) { return; }

	Genode::uint64_t const ms = _bsd_timer->microseconds();

	tv->tv_sec  = ms / (1000*1000);
	tv->tv_usec = ms % (1000*1000);
}

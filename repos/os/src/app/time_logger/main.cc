/*
 * \brief  Print various time stamps to log
 * \author Christian Prochaska
 * \date   2022-10-20
 *
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <trace/timestamp.h>
#include <timer_session/connection.h>

class Time_logger : public Genode::Thread
{
	private:

		int _cpu_index;
		Genode::Trace::Timestamp _ts { 0 };
		Genode::Semaphore _activation_sem { 0 };
		Genode::Semaphore _completion_sem { 0 };
		Timer::Connection &_timer;

		Genode::uint64_t _elapsed_us { 0 };
		Genode::uint64_t _curr_time_us { 0 };
		Genode::Trace::Timestamp _kernel_time { 0 };

	public:

		Time_logger(Genode::Env &env, int cpu_index, Timer::Connection &timer)
		: Genode::Thread(env,
		                 "time_logger",
		                 8192,
		                 env.cpu().affinity_space().location_of_index(cpu_index),
		                 Weight(),
		                 env.cpu()),
		  _cpu_index(cpu_index),
		  _timer(timer) { }

		void entry() override
		{
			for (;;) {
				_activation_sem.down();

				_ts = Genode::Trace::timestamp();
				_curr_time_us = _timer.curr_time().trunc_to_plain_us().value;
				_elapsed_us = _timer.elapsed_us();
				_kernel_time = _timer.kernel_time();

#if 0
				/* synchronize cycle counter with timer */
				Genode::Trace::Timestamp new_ts =
					_timer.elapsed_us() * 816;
				asm("msr pmccntr_el0, %0" :: "r" (new_ts));
#endif
				_completion_sem.up();
			}
		}

		Genode::uint64_t ts()
		{
			return _ts;
		}

		Genode::uint64_t ts_us()
		{
			/* PINE64: 816 MHz */
			return _ts / 816;
		}

		Genode::uint64_t elapsed_us()
		{
			return _elapsed_us;
		}

		Genode::uint64_t curr_time_us()
		{
			return _curr_time_us;
		}

		Genode::uint64_t kernel_time()
		{
			return _kernel_time;
		}

		void activate()
		{
			_activation_sem.up();
		}

		void wait_for_completion()
		{
			_completion_sem.down();
		}
};


void Component::construct(Genode::Env &env)
{
	Genode::Heap alloc(env.ram(), env.rm());
	Timer::Connection timer(env);
	Timer::Connection sleep_timer(env);

#if 1
	/* optional startup delay */
	sleep_timer.msleep(10000);
#endif

	enum { NUM_CPUS = 4 };

	Time_logger *t[NUM_CPUS];

	for (int cpu = 0; cpu < NUM_CPUS; cpu++) {
		t[cpu] = new (alloc) Time_logger(env, cpu, timer);
		t[cpu]->start();
	}

	for (;;) {

		for (int cpu = 0; cpu < NUM_CPUS; cpu++)
			t[cpu]->activate();

		for (int cpu = 0; cpu < NUM_CPUS; cpu++)
			t[cpu]->wait_for_completion();

		/* compare with idle cpu */
		int compare_cpu = 3;

		for (int cpu = 0; cpu < NUM_CPUS; cpu++) {
			Genode::log("cpu ", cpu, ": timestamp():       ",
			            t[cpu]->ts(), " (diff to idle cpu ", compare_cpu, ": ",
			            (int)(t[cpu]->ts() - t[compare_cpu]->ts()), ")");
			Genode::log("cpu ", cpu, ": timestamp() in us: ",
			            t[cpu]->ts_us(), " us (diff to idle cpu ", compare_cpu, ": ",
			            (int)(t[cpu]->ts_us() - t[compare_cpu]->ts_us()), " us)");
			Genode::log("cpu ", cpu, ": curr_time():       ",
			            t[cpu]->curr_time_us(), " us (diff to idle cpu ", compare_cpu, ": ",
			            (int)(t[cpu]->curr_time_us() - t[compare_cpu]->curr_time_us()), " us)");
			Genode::log("cpu ", cpu, ": elapsed_us():      ",
			            t[cpu]->elapsed_us(), " us (diff to idle cpu ", compare_cpu, ": ",
			            (int)(t[cpu]->elapsed_us() - t[compare_cpu]->elapsed_us()), " us)");
			Genode::log("cpu ", cpu, ": Kernel::time():    ",
			            t[cpu]->kernel_time(), " us (diff to idle cpu ", compare_cpu, ": ",
			            (int)(t[cpu]->kernel_time() - t[compare_cpu]->kernel_time()), " us)");
			Genode::log("");
		}

		Genode::Trace::Timestamp ts1 = Genode::Trace::timestamp();
		sleep_timer.msleep(10000);
		Genode::Trace::Timestamp ts2 = Genode::Trace::timestamp();
		Genode::log("cpu 0: timestamp() frequency: ", (ts2 - ts1) / 10, " Hz");
		Genode::log("");
	}
}

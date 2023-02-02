/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/time.h>

/*
 * private kernel/time header needed for internal functions
 * used in force_jiffies_update
 */
#include <../kernel/time/tick-internal.h>

extern void tick_do_update_jiffies64(ktime_t now);

static void force_jiffies_update(void);


signed long __real_schedule_timeout(signed long timeout);


signed long __wrap_schedule_timeout(signed long timeout)
{
	force_jiffies_update();
	return __real_schedule_timeout(timeout);
}


/**
 * Normally time proceeds due to
 * -> lx_emul/shadow/kernel/sched/core.c calls lx_emul_time_handle()
 *   -> repos/dde_linux/src/lib/lx_emul/clocksource.c:lx_emul_time_handle() calls
 *     -> dde_clock_event_device->event_handler() == tick_nohz_handler()
 *
 * -> kernel/time/tick-sched.c -> tick_nohz_handler()
 *   ->tick_sched_do_timer(ts, now);
 *     -> tick_do_update_jiffies64(now);
 *       -> which fast forwards jiffies in a loop until it matches wall clock
 *
 * force_jiffies_update can be used to update jiffies to current,
 * before invoking schedule_timeout(), which expects current jiffies values.
 * Without current jiffies, the programmed timeouts may be too short, which
 * leads to timeouts firing too early.
 */
void force_jiffies_update(void)
{
#if 0
	/* tick_nohz_handler() */

	struct tick_sched *ts = this_cpu_ptr(&tick_cpu_sched);
	struct pt_regs *regs = get_irq_regs();

	ktime_t now = ktime_get();

	dev->next_event = KTIME_MAX;

	tick_sched_do_timer(ts, now);
	tick_sched_handle(ts, regs);

	/* No need to reprogram if we are running tickless  */
	if (unlikely(ts->tick_stopped))
		return;

	hrtimer_forward(&ts->sched_timer, now, TICK_NSEC);
	tick_program_event(hrtimer_get_expires(&ts->sched_timer), 1);
#endif

	ktime_t  later, now              = ktime_get();
	uint64_t jiff_after, jiff_before = jiffies_64;

	tick_do_update_jiffies64(now);

	later      = ktime_get();
	jiff_after = jiffies_64;

	if ((now != later || jiff_before != jiff_after) &&
	    ((later - now > 1000) || (jiff_after - jiff_before > 1)))
		printk("%s: update diff ktime=%llu, jiff=%llu\n", __func__,
		       later - now, jiff_after - jiff_before);
}


#if 0
static void tick_sched_do_timer(struct tick_sched *ts, ktime_t now)
{
	int cpu = smp_processor_id();

#ifdef CONFIG_NO_HZ_COMMON
	/*
	 * Check if the do_timer duty was dropped. We don't care about
	 * concurrency: This happens only when the CPU in charge went
	 * into a long sleep. If two CPUs happen to assign themselves to
	 * this duty, then the jiffies update is still serialized by
	 * jiffies_lock.
	 *
	 * If nohz_full is enabled, this should not happen because the
	 * tick_do_timer_cpu never relinquishes.
	 */
	if (unlikely(tick_do_timer_cpu == TICK_DO_TIMER_NONE)) {
#ifdef CONFIG_NO_HZ_FULL
		WARN_ON(tick_nohz_full_running);
#endif
		tick_do_timer_cpu = cpu;
	}
#endif

	/* Check, if the jiffies need an update */
	if (tick_do_timer_cpu == cpu)
		tick_do_update_jiffies64(now);

	if (ts->inidle)
		ts->got_idle_tick = 1;
}

static void tick_nohz_handler(struct clock_event_device *dev)
{
	struct tick_sched *ts = this_cpu_ptr(&tick_cpu_sched);
	struct pt_regs *regs = get_irq_regs();
	ktime_t now = ktime_get();

	dev->next_event = KTIME_MAX;

	tick_sched_do_timer(ts, now);
	tick_sched_handle(ts, regs);

	/* No need to reprogram if we are running tickless  */
	if (unlikely(ts->tick_stopped))
		return;

	hrtimer_forward(&ts->sched_timer, now, TICK_NSEC);
	tick_program_event(hrtimer_get_expires(&ts->sched_timer), 1);
}

u64 hrtimer_forward(struct hrtimer *timer, ktime_t now, ktime_t interval)
{
	u64 orun = 1;
	ktime_t delta;

	delta = ktime_sub(now, hrtimer_get_expires(timer));

	if (delta < 0)
		return 0;

	if (WARN_ON(timer->state & HRTIMER_STATE_ENQUEUED))
		return 0;

	if (interval < hrtimer_resolution)
		interval = hrtimer_resolution;

	if (unlikely(delta >= interval)) {
		s64 incr = ktime_to_ns(interval);

		orun = ktime_divns(delta, incr);
		hrtimer_add_expires_ns(timer, incr * orun);
		if (hrtimer_get_expires_tv64(timer) > now)
			return orun;
		/*
		 * This (and the ktime_add() below) is the
		 * correction for exact:
		 */
		orun++;
	}
	hrtimer_add_expires(timer, interval);

	return orun;
}
EXPORT_SYMBOL_GPL(hrtimer_forward);

#endif

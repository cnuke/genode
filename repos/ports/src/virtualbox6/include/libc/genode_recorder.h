#pragma once


static inline uint64_t genode_rdtsc()
{
	uint32_t lo, hi;
	__asm__ __volatile__ (
		"lfence\n\t"
		"rdtsc" : "=a" (lo), "=d" (hi)
	);

	return (uint64_t)hi << 32 | lo;
}


struct Execution_guard
{
	uint64_t const start;
	unsigned const cpu_id;

	uint64_t &duration;

	Execution_guard(unsigned cpu_id, uint64_t &duration)
	: start { genode_rdtsc() }, cpu_id { cpu_id }, duration { duration }
	{ }

	~Execution_guard()
	{
		uint64_t const diff = genode_rdtsc() - start;
		duration = diff;
	}
};


struct Scoped_duration
{
	uint64_t total;
	uint64_t last;

	char const *name;

	Scoped_duration() : total{ 0 }, last{ 0 }, name { nullptr } { }

	Scoped_duration(char const *name) : total{ 0 }, last{ 0 }, name { name } { }

	void reset()
	{
		total = 0;
		last = 0;
	}
};


extern void genode_trace_checkpoint_start(char const *name, unsigned long data);
extern void genode_trace_checkpoint_end(char const *name, unsigned long data);

struct Scoped_duration_guard
{
	Scoped_duration &duration;
	unsigned         cpu_id;

	uint64_t const _start;

	Scoped_duration_guard(unsigned cpu_id, Scoped_duration &duration)
	: cpu_id { cpu_id }, duration { duration }, _start { genode_rdtsc() }
	{
		genode_trace_checkpoint_start(duration.name ? : "unknown", 0ul);
	}

	~Scoped_duration_guard()
	{
		uint64_t const diff = genode_rdtsc() - _start;
		duration.total += diff;
		duration.last = diff;

		genode_trace_checkpoint_end(duration.name ? : "unknown", 0ul);
	}
};


template<typename FN>
uint64_t genode_execution_duration(FN const &fn, char const *name = nullptr)
{
	uint64_t const start = genode_rdtsc();
	genode_trace_checkpoint_start(name ? : "unknown-duration", 0ul);
	fn();
	genode_trace_checkpoint_end(name ? : "unknown-duration", 0ul);
	uint64_t const diff = genode_rdtsc() - start;
	return diff;
}

extern void genode_record_timer(void *timer, void *func, ::uint64_t duration);
extern void genode_record_timer_dump();
extern void genode_record_timer_reset(void *timer);
extern void genode_executed_from_recorder(unsigned cpu_id, char const *name, void const *addr);
extern void genode_executed_from_recorder_dump(void);
extern void genode_executed_from_recorder_reset(unsigned cpu_id, void const *addr);
extern void genode_nemhandle_recorder(unsigned cpu_id, int rc);
extern void genode_nemhandle_recorder_dump(void);
extern void genode_nemhandle_recorder_reset(unsigned cpu_id, int rc);
extern void genode_record_ff_timer(void const *func, char const *name);
extern void genode_record_ff_timer_dump();
extern void genode_record_ff_timer_reset(void const *func);
extern void genode_newstate_recorder(unsigned cpu_id, unsigned state);
extern void genode_newstate_recorder_dump(void);
extern void genode_newstate_recorder_reset(unsigned cpu_id, unsigned state);
extern void genode_old_newstate_recorder(unsigned cpu_id, unsigned state);
extern void genode_old_newstate_recorder_dump(void);
extern void genode_old_newstate_recorder_reset(unsigned cpu_id, unsigned state);
extern void genode_nemrc_recorder(unsigned cpu_id, int rc);
extern void genode_nemrc_recorder_dump(void);
extern void genode_nemrc_recoder_reset(unsigned cpu_id, int rc);
extern void genode_rc_recorder(unsigned cpu_id, int rc);
extern void genode_rc_recorder_dump(void);
extern void genode_rc_recoder_reset(unsigned cpu_id, int rc);

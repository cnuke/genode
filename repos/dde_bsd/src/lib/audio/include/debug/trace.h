#ifndef _DEBUG__TRACE_H_
#define _DEBUG__TRACE_H_

namespace Debug {

	void init_tracing(Genode::Env &env);

	void enable_tracing();
	void disable_tracing();

	void dump_trace_buffer();
}

#endif /* _DEBUG__TRACE_H_ */

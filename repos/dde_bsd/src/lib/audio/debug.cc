/*
 * \brief  Component-locale TRACE monitor for debugging purposes
 * \author Josef Soentgen
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <trace_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>

/* local includes */
#include <debug/trace.h>
#include <debug/trace_buffer.h>


using namespace Genode;

struct Tracer
{
	Env               &env;
	Trace::Connection  trace { env, 64*1024*1024, 64*1024, 0 };

	Constructible<Trace_buffer> trace_buffer { };

	typedef Genode::String<64> String;
	String policy_module { "null" };

	String label;
	String thread_name;

	Rom_dataspace_capability policy_module_rom_ds { };
	Trace::Policy_id         policy_id { };

	char const *state_name(Trace::Subject_info::State state)
	{
		switch (state) {
		case Trace::Subject_info::INVALID:  return "INVALID";
		case Trace::Subject_info::UNTRACED: return "UNTRACED";
		case Trace::Subject_info::TRACED:   return "TRACED";
		case Trace::Subject_info::FOREIGN:  return "FOREIGN";
		case Trace::Subject_info::ERROR:    return "ERROR";
		case Trace::Subject_info::DEAD:     return "DEAD";
		}
		return "undefined";
	}

	template <typename FUNC>
	void for_each_subject(Trace::Subject_id subjects[],
	                      size_t max_subjects, FUNC const &func)
	{
		for (size_t i = 0; i < max_subjects; i++) {
			Trace::Subject_info info = trace.subject_info(subjects[i]);
			func(subjects[i].id, info);
		}
	}

	struct Failed : Genode::Exception { };

	Trace::Subject_id sub_id;

	Tracer(Env &env, char const *label, char const *name)
	: env(env), label(label), thread_name(name)
	{
		try {
			Rom_connection policy_rom(env, policy_module.string());
			policy_module_rom_ds = policy_rom.dataspace();

			size_t rom_size = Dataspace_client(policy_module_rom_ds).size();

			policy_id = trace.alloc_policy(rom_size);
			Dataspace_capability ds_cap = trace.policy(policy_id);

			if (ds_cap.valid()) {
				void *ram = env.rm().attach(ds_cap);
				void *rom = env.rm().attach(policy_module_rom_ds);
				memcpy(ram, rom, rom_size);

				env.rm().detach(ram);
				env.rm().detach(rom);
			}
		} catch (...) {
			error("could not load module '", policy_module, "'");
			throw Failed();
		}

		Trace::Subject_id subjects[128];
		size_t num_subjects = trace.subjects(subjects, 128);

#if 0
		log(num_subjects, " tracing subjects present");
		auto print_info = [this] (Trace::Subject_id id, Trace::Subject_info info) {

			log("ID:",      id.id,                    " "
			    "label:\"", info.session_label(),   "\" "
			    "name:\"",  info.thread_name(),     "\" "
			    "state:",   state_name(info.state()), " "
			    "policy:",  info.policy_id().id,      " "
			    "thread context time:", info.execution_time().thread_context, " "
			    "scheduling context time:", info.execution_time().scheduling_context, " ",
			    "priority:", info.execution_time().priority, " ",
			    "quantum:", info.execution_time().quantum);
		};
		for_each_subject(subjects, num_subjects, print_info);
#endif

		auto enable_tracing = [&] (Trace::Subject_id id,
		                                    Trace::Subject_info info) {

			if (   info.session_label() != label
			    || info.thread_name()   != thread_name) {
				return;
			}

			sub_id = id;
			log("Found '", info.session_label(), "' '", info.thread_name(), "' id: ", sub_id.id);
		};
		for_each_subject(subjects, num_subjects, enable_tracing);
	}

	bool _enabled = false;

	void enable_tracing()
	{
		try {
			if (!_enabled) {
				trace.trace(sub_id.id, policy_id, 62u << 20);
				Dataspace_capability ds_cap = trace.buffer(sub_id.id);
				trace_buffer.construct(env.rm(), ds_cap);
				_enabled = true;
			} else {
				trace.resume(sub_id.id);
			}
		} catch (Trace::Source_is_dead) {
			error("source is dead");
			throw Failed();
		}
	}

	void disable_tracing()
	{
		try {
			trace.pause(sub_id.id);
		} catch (Trace::Source_is_dead) {
			error("source is dead");
			throw Failed();
		}
	}

	void dump_trace_buffer()
	{
		trace_buffer->for_each_new_entry([&](Trace::Buffer::Entry entry) {

			if (entry.length() == 0) {
				return false;
			}

			char const *data = entry.data();
			bool const ending_lf = (data[entry.length()-1] == '\n');

			Genode::log(Genode::Cstring(entry.data(), entry.length() - ending_lf));
			return true;
		});
	}
};


static Genode::Constructible<Tracer> _tracer { };


void Debug::init_tracing(Genode::Env &env)
{
	if (_tracer.constructed()) {
		return;
	}

	_tracer.construct(env, "init -> usb_audio_drv", "ep");
}


void Debug::enable_tracing()
{
	if (!_tracer.constructed()) {
		Genode::warning("tracer not initialized");
		return;
	}

	_tracer->enable_tracing();
}


void Debug::disable_tracing()
{
	if (!_tracer.constructed()) {
		Genode::warning("tracer not initialized");
		return;
	}

	_tracer->disable_tracing();
}


void Debug::dump_trace_buffer()
{
	if (!_tracer.constructed()) {
		Genode::warning("tracer not initialized");
		return;
	}

	_tracer->dump_trace_buffer();
}

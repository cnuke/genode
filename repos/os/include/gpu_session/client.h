/*
 * \brief  Client-side Gpu session interface
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_SESSION__CLIENT_H_
#define _INCLUDE__GPU_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <gpu_session/capability.h>

namespace Gpu { class Session_client; }


class Gpu::Session_client : public Genode::Rpc_client<Session>
{
	public:

		Genode::uint64_t exec_buffer_count { 0 };
		Genode::uint64_t set_tiling_count { 0 };
		Genode::Signal_context_capability completion { };

		/**
		 * Constructor
		 *
		 * \param session  session capability
		 */
		Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

		/***********************
		 ** Session interface **
		 ***********************/

		Genode::Dataspace_capability info_dataspace() const override {
			return call<Rpc_info_dataspace>(); }

		Gpu::Sequence_number exec_buffer(Buffer_id id,
		                                 Genode::size_t size) override {
//exec_buffer_count++;
		    if (exec_buffer_count < 1000) {
			    Sequence_number res = call<Rpc_exec_buffer>(id, size);
//Genode::log(this, ": exec_buffer(): ", res.value);
			    return res;
			} else if (exec_buffer_count == 1000) {
Genode::log(this, ": *** exec_buffer() stopped");
			}
			return Sequence_number { exec_buffer_count };
		}

		bool complete(Sequence_number seqno) override {
			if (exec_buffer_count < 1000) {
				bool res = call<Rpc_complete>(seqno);
//Genode::log(this, ": complete(): ", seqno.value, " -> ", res);
				return res;
			}
			Genode::Signal_transmitter(completion).submit();
			return true;
		}

		void completion_sigh(Genode::Signal_context_capability sigh) override {
//Genode::log(this, ": completion_sigh()");
			completion = sigh;
			call<Rpc_completion_sigh>(sigh);
		}

		Genode::Dataspace_capability alloc_buffer(Buffer_id id, Genode::size_t size) override {
			return call<Rpc_alloc_buffer>(id, size); }

		void free_buffer(Gpu::Buffer_id id) override {
			call<Rpc_free_buffer>(id); }

		Gpu::Buffer_capability export_buffer(Buffer_id id) override {
			return call<Rpc_export_buffer>(id); }

		void import_buffer(Buffer_capability cap, Buffer_id id) override {
			call<Rpc_import_buffer>(cap, id); }

		Genode::Dataspace_capability map_buffer(Buffer_id id,
		                                        bool aperture,
		                                        Mapping_attributes attrs) override {
			return call<Rpc_map_buffer>(id, aperture, attrs); }

		void unmap_buffer(Buffer_id id) override {
			call<Rpc_unmap_buffer>(id); }

		bool map_buffer_ppgtt(Buffer_id id, Gpu::addr_t va) override {
			return call<Rpc_map_buffer_ppgtt>(id, va); }

		void unmap_buffer_ppgtt(Buffer_id id, Gpu::addr_t va) override {
			call<Rpc_unmap_buffer_ppgtt>(id, va); }

		Gpu::addr_t query_buffer_ppgtt(Gpu::Buffer_id id) override {
			return call<Rpc_query_buffer_ppgtt>(id); }

		bool set_tiling(Buffer_id id, unsigned mode) override {
//set_tiling_count++;
			if (set_tiling_count < 20000) {
				bool res = call<Rpc_set_tiling>(id, mode);
//Genode::log(this, ": set_tiling(): ", set_tiling_count, ", res: ", res);
				return res;
			} else if (set_tiling_count == 20000) {
Genode::log(this, ": *** set_tiling() stopped");
			}
			return true;
		}
};

#endif /* _INCLUDE__GPU_SESSION__CLIENT_H_ */

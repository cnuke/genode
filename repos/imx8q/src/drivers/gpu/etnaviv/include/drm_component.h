/*
 * \brief  Server-side DRM session component
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2021-04-XX
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __DRM_COMPONENT_H__
#define __DRM_COMPONENT_H__

#include <root/component.h>
#include <gpu/rpc_object.h>
#include <lx_kit/scheduler.h>

namespace Drm {
	using namespace Genode;

	class Root;
	class Session_component;
}


extern "C" void *lx_drm_open(void);
extern "C" void  lx_drm_close(void *);

extern "C" int lx_drm_ioctl(void *, unsigned int, unsigned long);

extern "C" int          lx_drm_check_gem_new(unsigned int);
extern "C" unsigned int lx_drm_get_gem_new_handle(unsigned long);

extern "C" int          lx_drm_check_gem_close(unsigned int);
extern "C" unsigned int lx_drm_get_gem_close_handle(unsigned long);

extern "C" int lx_drm_close_handle(void *,unsigned int);

Genode::Ram_dataspace_capability lx_drm_object_dataspace(unsigned long, unsigned long);


class Drm::Session_component : public Session_rpc_object
{
	public:

		enum { CAP_QUOTA = 8, /* XXX */ };

	private:

		Env &_env;

		Genode::Heap _alloc { _env.ram(), _env.rm() };

		Genode::Mutex _object_mutex { };
		struct Object_request
		{
			Genode::Ram_dataspace_capability cap;
			unsigned long offset;
			unsigned long size;
			bool pending;

			bool request_valid() const { return offset && size; }

			bool request_resolved() const { return !pending; }
		};

		struct Handle : Genode::Registry<Handle>::Element
		{
			uint32_t const handle;

			Handle(Genode::Registry<Handle> &registry, uint32_t handle)
			:
				Genode::Registry<Handle>::Element { registry, *this },
				handle { handle }
			{ }
		};

		struct Handle_registry : Genode::Registry<Handle>
		{
			Genode::Allocator &_alloc;

			Handle_registry(Genode::Allocator &alloc)
			: _alloc { alloc }
			{ }

			~Handle_registry()
			{
				/* assert registry is empty */
				bool empty = true;

				for_each([&] (Handle &h) {
					empty = false;
				});

				if (!empty) {
					Genode::error("handle registry not empty, leaking GEM objects");
				}
			}

			void add(uint32_t handle)
			{
				bool found = false;
				for_each([&] (Handle const &h) {
					if (h.handle == handle) {
						found = true;
					}
				});

				if (found) {
					Genode::error("handle ", handle, " already present in registry");
					return;
				}

				new (&_alloc) Handle(*this, handle);
			}

			void remove(uint32_t handle)
			{
				bool removed = false;
				for_each([&] (Handle &h) {
					if (h.handle == handle) {
						Genode::destroy(_alloc, &h);
						removed = true;
					}
				});

				if (!removed) {
					Genode::error("could not remove handle ", handle,
					              " - not present in registry");
				}
			}

			Genode::Allocator* alloc()
			{
				return &_alloc;
			}
		};

		Handle_registry _handle_reg { _alloc };

		struct Task_args
		{
			void *drm_session;

			Object_request obj;
			Tx::Sink *sink;

			Handle_registry *handle_reg;
			bool cleanup;
		};

		Task_args _task_args {
			.drm_session = nullptr,
			.obj = { Genode::Ram_dataspace_capability(), 0, 0, false},
			.sink = tx_sink(),
			.handle_reg = &_handle_reg,
			.cleanup = false,
		};

		char const *_name;

		Signal_handler<Session_component> _packet_avail { _env.ep(), *this,
			&Session_component::_handle_signal };
		Signal_handler<Session_component> _ready_to_ack { _env.ep(), *this,
			&Session_component::_handle_signal };
		Lx::Task                          _worker;

		static void _drm_request(void *drm_session, Handle_registry &handle_reg, Tx::Sink &sink)
		{
			while (sink.packet_avail() && sink.ready_to_ack()) {
				Packet_descriptor pkt = sink.get_packet();

				void *arg = sink.packet_content(pkt);
				int err = lx_drm_ioctl(drm_session, (unsigned int)pkt.request(), (unsigned long)arg);
				if (!err) {
					if (lx_drm_check_gem_new((unsigned int)pkt.request())) {
						uint32_t const handle = lx_drm_get_gem_new_handle((unsigned long)arg);
						handle_reg.add(handle);
					} else

					if (lx_drm_check_gem_close((unsigned int)pkt.request())) {
						uint32_t const handle = lx_drm_get_gem_close_handle((unsigned long)arg);
						handle_reg.remove(handle);
					}
				}
				pkt.error(err);
				sink.acknowledge_packet(pkt);
			}
		}

		static void _run(void *task_args)
		{
			Task_args *args = static_cast<Task_args*>(task_args);

			Tx::Sink       &sink = *args->sink;
			Object_request &obj  = args->obj;

			Handle_registry &handle_reg = *args->handle_reg;

			while (true) {

				if (!args->drm_session) {
					args->drm_session = lx_drm_open();
					if (!args->drm_session) {

						Genode::error("lx_drm_open failed");
						while (true) {
							Lx::scheduler().current()->block_and_schedule();
						}
					}
				}

				if (args->cleanup) {

					lx_drm_close(args->drm_session);

					handle_reg.for_each([&] (Handle &h) {

						/*
						 * Close here instead of in the handle object because
						 * handle might have been closed already and during cleanup
						 * only leftovers are handled.
						 */
						int const err =
							lx_drm_close_handle(args->drm_session, h.handle);
						/*
						 * EINVAL is returned in case the handle does not
						 * point to a object any longer. This may happen when
						 * the object was already taken care of, so ignore
						 * this case. Our cleanup, after all, is merely a
						 * rudimentary leakage * prevention.
						 */
						if (err != -22) {
							Genode::error("could not close handle ", h.handle,
							              " - leaking resources: ", err);
						}
						Genode::destroy(handle_reg.alloc(), &h);
					});
				}

				_drm_request(args->drm_session, handle_reg, sink);
				if (obj.request_valid() && !obj.request_resolved()) {
					obj.cap = lx_drm_object_dataspace(obj.offset, obj.size);
					obj.pending = false;
					obj.offset  = 0;
					obj.size =   0;
				}
				Lx::scheduler().current()->block_and_schedule();
			}
		}

		void _handle_signal()
		{
			/* wake up worker */
			_worker.unblock();
			Lx::scheduler().schedule();
		}

	public:

		Session_component(Env &env, Dataspace_capability tx_ds_cap,
		                  char const *name)
		:
		  Session_rpc_object(env.rm(), tx_ds_cap, env.ep().rpc_ep()), _env(env),
		  _name { name },
		  _worker { _run, &_task_args, _name, Lx::Task::PRIORITY_2, Lx::scheduler() }
		{
			_tx.sigh_packet_avail(_packet_avail);
			_tx.sigh_ready_to_ack(_ready_to_ack);
		}

		~Session_component()
		{
			/* wake up worker for cleanup */
			_task_args.cleanup = true;
			_worker.unblock();
			Lx::scheduler().schedule();
		}

		char const *name() const { return _name; }

		Ram_dataspace_capability object_dataspace(unsigned long offset,
		                                          unsigned long size) override
		{
			Genode::Mutex::Guard mutex_guard(_object_mutex);

			Object_request &obj = _task_args.obj;
			obj.pending = true;
			obj.offset  = offset;
			obj.size    = size;

			_worker.unblock();
			Lx::scheduler().schedule();

			Genode::Ram_dataspace_capability cap = obj.cap;
			obj.cap = Genode::Ram_dataspace_capability();

			return cap;
		}
};


class Drm::Root : public Root_component<Drm::Session_component, Multiple_clients>
{
	private:

		Env       &_env;
		Allocator &_alloc;

		uint32_t  _session_id;

	protected:

		Session_component *_create_session(char const *args) override
		{
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			char *name = (char*)_alloc.alloc(64);
			Genode::String<64> tmp("drm_worker-", ++_session_id);
			Genode::memcpy(name, tmp.string(), tmp.length());

			return new (_alloc) Session_component(_env, _env.ram().alloc(tx_buf_size), name);
		}

		void _destroy_session(Session_component *s) override
		{
			char const *name = s->name();

			Genode::destroy(_alloc, const_cast<char*>(name));
			Genode::destroy(md_alloc(), s);
		}

	public:

		Root(Env &env, Allocator &alloc)
		: Root_component<Session_component, Genode::Multiple_clients>(env.ep(), alloc),
			_env(env), _alloc(alloc), _session_id { 0 }
		{ }
};

#endif /* __DRM_COMPONENT_H__ */

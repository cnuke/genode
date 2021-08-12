/*
 * \brief  Server-side Gpu session component
 * \author Josef Soentgen
 * \date   2021-08-06
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/session_object.h>
#include <root/component.h>
#include <session/session.h>
#include <gpu_session/gpu_session.h>

/* Linux emulation */
#include <lx_kit/env.h>
#include <lx_kit/scheduler.h>
#include <lx_emul_cc.h>
#include <lx_drm.h>

extern Genode::Dataspace_capability genode_lookup_cap(void *, unsigned int);
extern unsigned int                 genode_lookup_handle(void *, Genode::Dataspace_capability);

namespace Gpu {

	using namespace Genode;

	struct Session_component;
	struct Root;

	using Root_component = Genode::Root_component<Session_component, Genode::Multiple_clients>;
}

struct Gpu::Session_component : public Genode::Session_object<Gpu::Session>,
                                public Genode::Registry<Gpu::Session_component>::Element
{
	private:

		Gpu::Info _info { 0, 0, 0, 0, Gpu::Info::Execution_buffer_sequence { 0 } };

		static int _populate_info(void *drm, Gpu::Info &info)
		{
			for (Gpu::Info::Etnaviv_param &p : info.etnaviv_param) {
				p = 0;
			}

			uint8_t params[] = {
				0xff, /* 0x00 inv */
				0x01, /* ETNAVIV_PARAM_GPU_MODEL      */
				0x02, /* ETNAVIV_PARAM_GPU_REVISION   */
				0x03, /* ETNAVIV_PARAM_GPU_FEATURES_0 */
				0x04, /* ETNAVIV_PARAM_GPU_FEATURES_1 */
				0x05, /* ETNAVIV_PARAM_GPU_FEATURES_2 */
				0x06, /* ETNAVIV_PARAM_GPU_FEATURES_3 */
				0x07, /* ETNAVIV_PARAM_GPU_FEATURES_4 */
				0x08, /* ETNAVIV_PARAM_GPU_FEATURES_5 */
				0x09, /* ETNAVIV_PARAM_GPU_FEATURES_6 */
				0x0a, /* ETNAVIV_PARAM_GPU_FEATURES_7 */
				0xff, /* 0x0b inv */
				0xff, /* 0x0c inv */
				0xff, /* 0x0d inv */
				0xff, /* 0x0e inv */
				0xff, /* 0x0f inv */
				0x10, /* ETNAVIV_PARAM_GPU_STREAM_COUNT              */
				0x11, /* ETNAVIV_PARAM_GPU_REGISTER_MAX              */
				0x12, /* ETNAVIV_PARAM_GPU_THREAD_COUNT              */
				0x13, /* ETNAVIV_PARAM_GPU_VERTEX_CACHE_SIZE         */
				0x14, /* ETNAVIV_PARAM_GPU_SHADER_CORE_COUNT         */
				0x15, /* ETNAVIV_PARAM_GPU_PIXEL_PIPES               */
				0x16, /* ETNAVIV_PARAM_GPU_VERTEX_OUTPUT_BUFFER_SIZE */
				0x17, /* ETNAVIV_PARAM_GPU_BUFFER_SIZE               */
				0x18, /* ETNAVIV_PARAM_GPU_INSTRUCTION_COUNT         */
				0x19, /* ETNAVIV_PARAM_GPU_NUM_CONSTANTS             */
				0x1a, /* ETNAVIV_PARAM_GPU_NUM_VARYINGS              */
				0x1b, /* ETNAVIV_PARAM_SOFTPIN_START_ADDR            */
				0xff, /* 0x1c inv */
				0xff, /* 0x1d inv */
				0xff, /* 0x1e inv */
				0xff, /* 0x1f inv */
			};

			static_assert(sizeof(params)/sizeof(params[0]) == 32);

			for (int p = 0; p < 32; p++) {
				uint64_t value;
				int const err = lx_drm_ioctl_etnaviv_gem_param(drm, p, &value);
				if (err) {
					return -1;
				}

				info.etnaviv_param[p] = value;
			}
			return 0;
		}

		Genode::Signal_context_capability _completion_sigh { };
		uint64_t _pending_seqno { };

		char const *_name;

		struct Gpu_request
		{
			enum class Op { INVALID, OPEN, CLOSE, NEW, DELETE, EXEC, MAP, UNMAP };
			enum class Result { SUCCESS, ERR };

			struct Op_data {
				// new
				size_t size;
				// new. close. exec, map, unmap
				Genode::Dataspace_capability buffer_cap;
				// exec
				uint64_t seqno;
			};

			Op      op;
			Op_data op_data;

			Result result;

			bool valid() const { return op != Op::INVALID; }
		};


		struct Drm_worker_args
		{
			Gpu_request request;
			void *drm_session;

			Gpu::Info &info;
		};

		Drm_worker_args _drm_worker_args {
			.request = Gpu_request {
				.op      = Gpu_request::Op::INVALID,
				.op_data = { 0, Genode::Dataspace_capability() },
				.result  = Gpu_request::Result::ERR,
			},
			.drm_session = nullptr,
			.info        = _info,
		};

		Lx::Task _drm_worker;

		static void _drm_worker_run(void *p)
		{
			Drm_worker_args &args = *static_cast<Drm_worker_args*>(p);

			while (true) {

				/* clear request result */
				args.request.result = Gpu_request::Result::ERR;

				switch (args.request.op) {
				case Gpu_request::Op::OPEN:
					if (!args.drm_session) {
						args.drm_session = lx_drm_open();
						if (!args.drm_session) {
							Genode::error("lx_drm_open failed");
							while (true) {
								Lx::scheduler().current()->block_and_schedule();
							}
						}

						_populate_info(args.drm_session, args.info);
					}
					break;
				case Gpu_request::Op::CLOSE:
					lx_drm_close(args.drm_session);
					args.drm_session = nullptr;
					break;
				case Gpu_request::Op::NEW:
				{
					uint32_t handle;

					/* make sure cap is invalid */
					args.request.op_data.buffer_cap = Genode::Dataspace_capability();

					int const err =
						lx_drm_ioctl_etnaviv_gem_new(args.drm_session,
						                             args.request.op_data.size, &handle);
					// XXX check value of err to propagate type of error
					if (err) {
						break;
					}

					Genode::Dataspace_capability cap = genode_lookup_cap(args.drm_session, handle);
					if (!cap.valid()) {
						break;
					}

					args.request.op_data.buffer_cap = cap;
					args.request.result             = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::DELETE:
				{
					unsigned int const handle =
						genode_lookup_handle(args.drm_session,
						                     args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					(void)lx_drm_ioctl_gem_close(args.drm_session, handle);

					args.request.result = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::EXEC:
				{
					unsigned int const handle =
						genode_lookup_handle(args.drm_session,
						                     args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					uint64_t seqno;
					int const err =
						lx_drm_ioctl_etnaviv_gem_submit(args.drm_session, handle, &seqno);
					if (err) {
						// XXX check value of err
						break;
					}

					args.request.op_data.seqno = seqno;
					args.request.result        = Gpu_request::Result::SUCCESS;
				}
				case Gpu_request::Op::MAP:
				{
					unsigned int const handle =
						genode_lookup_handle(args.drm_session,
						                     args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					int const err =
						lx_drm_ioctl_etnaviv_prep_cpu(args.drm_session, handle);
					if (err) {
						break;
					}
					args.request.result = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::UNMAP:
				{
					unsigned int const handle =
						genode_lookup_handle(args.drm_session,
					 	                     args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					(void)lx_drm_ioctl_etnaviv_fini_cpu(args.drm_session, handle);

					args.request.result = Gpu_request::Result::SUCCESS;
					break;
				}
				default:
				break;
				}


				Lx::scheduler().current()->block_and_schedule();
			}
		}

	public:

		struct Could_not_open_drm : Genode::Exception { };

		/**
		 * Constructor
		 */
		Session_component(Genode::Registry<Gpu::Session_component> &registry,
		                  Genode::Entrypoint &ep,
		                  Resources    const &resources,
		                  Label        const &label,
		                  Diag                diag,
		                  char         const *name)
		:
			Session_object { ep, resources, label, diag },
			Genode::Registry<Gpu::Session_component>::Element { registry, *this },
		  	_name { name },
			_drm_worker { _drm_worker_run, &_drm_worker_args, _name,
			              Lx::Task::PRIORITY_2, Lx::scheduler() }
		{ }

		~Session_component()
		{
			if (_drm_worker_args.request.valid()) {
				Genode::warning("destructor override currently pending request");
			}

			_drm_worker_args.request = Gpu_request {
				.op = Gpu_request::Op::CLOSE,
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				Genode::warning("could not close DRM session - leaking objects");
			}
		}

		char const *name() { return _name; }

		uint64_t pending_seqno() const { return _pending_seqno; }

		void submit_completion_signal(uint64_t seqno)
		{
			_info.last_completed.id = seqno;
			Genode::Signal_transmitter(_completion_sigh).submit();
		}

		/******************************
		 ** Session object interface **
		 ******************************/

		void session_quota_upgraded() override
		{
			Session_object::session_quota_upgraded();
		}

		/***************************
		 ** Gpu session interface **
		 ***************************/

		struct Retry_request : Genode::Exception { };

		Info info() const
		{
			return _info;
		}

		Gpu::Info::Execution_buffer_sequence exec_buffer(Genode::Dataspace_capability cap,
		                                                 Genode::size_t size) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::EXEC,
				.op_data = { .size = size, .buffer_cap = cap },
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				throw Gpu::Session::Invalid_state();
			}

			_pending_seqno = _drm_worker_args.request.op_data.seqno;

			return Gpu::Info::Execution_buffer_sequence {
				.id = _pending_seqno };
		}

		void completion_sigh(Genode::Signal_context_capability sigh) override
		{
			_completion_sigh = sigh;
		}

		Genode::Dataspace_capability alloc_buffer(Genode::size_t size) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::NEW,
				.op_data = { size, Genode::Dataspace_capability() },
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				throw Gpu::Session::Out_of_ram();
			}
			return _drm_worker_args.request.op_data.buffer_cap;
		}

		void free_buffer(Genode::Dataspace_capability cap) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::DELETE,
				.op_data = { .buffer_cap = cap },
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				Genode::warning(__func__, ": could not free buffer");
			}
		}

		Genode::Dataspace_capability map_buffer(Genode::Dataspace_capability cap,
		                                        bool /* aperture */,
		                                        Mapping_type /* mt */) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::MAP,
				.op_data = { .buffer_cap = cap, },
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				return Genode::Dataspace_capability();
			}

			return cap;
		}

		void unmap_buffer(Genode::Dataspace_capability cap) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::UNMAP,
				.op_data = { .buffer_cap = cap, },
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				Genode::warning(__func__, ": could not unmap buffer");
			}
		}

		bool map_buffer_ppgtt(Genode::Dataspace_capability, Gpu::addr_t) override
		{
			Genode::warning(__func__, ": not implemented");
			return false;
		}

		void unmap_buffer_ppgtt(Genode::Dataspace_capability, Gpu::addr_t)
		{
			Genode::warning(__func__, ": not implemented");
		}

		bool set_tiling(Genode::Dataspace_capability, unsigned)
		{
			Genode::warning(__func__, ": not implemented");
			return false;
		}
};


struct Gpu::Root : Gpu::Root_component
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		uint32_t _session_id;

		Genode::Registry<Gpu::Session_component> _sessions { };

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

	protected:

		Session_component *_create_session(char const *args) override
		{
			char *name = (char*)_alloc.alloc(64);
			Genode::String<64> tmp("drm_worker-", ++_session_id);
			Genode::memcpy(name, tmp.string(), tmp.length());

			Session::Label const label  { session_label_from_args(args) };
			// Session_policy const policy { label, _env.config.xml()      };

			return new (_alloc) Session_component(_sessions, _env.ep(),
			                                      session_resources_from_args(args),
			                                      label,
			                                      session_diag_from_args(args),
			                                      name);
		}

		void _upgrade_session(Session_component *sc, char const *args) override
		{
			sc->upgrade(ram_quota_from_args(args));
			sc->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Session_component *sc) override
		{
			char const *name = sc->name();

			Genode::destroy(_alloc, const_cast<char*>(name));
			Genode::destroy(md_alloc(), sc);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &alloc)
		:
			Root_component { env.ep(), alloc },
			_env           { env },
			_alloc         { alloc },
			_session_id    { 0 }
		{ }

		void completion_signal(uint64_t seqno)
		{
			_sessions.for_each([&] (Gpu::Session_component &sc) {
				if (sc.pending_seqno() == seqno) {
					sc.submit_completion_signal(seqno);
				}
			});
		}
};


static Genode::Constructible<Gpu::Root> _gpu_root { };


extern "C" void lx_emul_announce_gpu_session(void)
{
	if (!_gpu_root.constructed()) {
		_gpu_root.construct(Lx_kit::env().env, Lx_kit::env().heap);

		Genode::Entrypoint &ep = Lx_kit::env().env.ep();
		Lx_kit::env().env.parent().announce(ep.manage(*_gpu_root));
	}
}


void genode_completion_signal(unsigned long long seqno)
{
	if (!_gpu_root.constructed()) {
		return;
	}

	_gpu_root->completion_signal(seqno);
}

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
#include <base/heap.h>
#include <base/session_object.h>
#include <root/component.h>
#include <session/session.h>
#include <gpu_session/gpu_session.h>

/* Linux emulation */
#include <lx_kit/env.h>
#include <lx_kit/scheduler.h>
#include <lx_emul_cc.h>
#include <lx_drm.h>

extern Genode::Dataspace_capability genode_lookup_cap(void *, unsigned long long, unsigned long);

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

		Genode::Env  &_env;
		Genode::Heap  _alloc;

		struct Buffer_handle : Genode::Registry<Buffer_handle>::Element
		{
			uint32_t                     const handle;
			Genode::Dataspace_capability const cap;

			Buffer_handle(Genode::Registry<Buffer_handle> &registry,
			       uint32_t handle,
			       Genode::Dataspace_capability cap)
			:
				Genode::Registry<Buffer_handle>::Element { registry, *this },
				handle { handle },
				cap    { cap }
			{ }
		};

		struct Buffer_handle_registry : Genode::Registry<Buffer_handle>
		{
			Genode::Allocator &_alloc;

			Buffer_handle_registry(Genode::Allocator &alloc)
			: _alloc { alloc } { }

			~Buffer_handle_registry()
			{
				/* assert registry is empty */
				bool empty = true;

				for_each([&] (Buffer_handle &h) {
					empty = false;
				});

				if (!empty) {
					Genode::error("handle registry not empty, leaking GEM objects");
				}
			}

			void insert(uint32_t handle, Genode::Dataspace_capability cap)
			{
				bool found = false;
				for_each([&] (Buffer_handle const &h) {
					if (h.handle == handle) {
						found = true;
					}
				});

				if (found) {
					Genode::error("handle ", handle, " already present in registry");
					return;
				}

				new (&_alloc) Buffer_handle(*this, handle, cap);
			}

			void remove(uint32_t handle)
			{
				bool removed = false;
				for_each([&] (Buffer_handle &h) {
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

			uint32_t lookup_buffer(Genode::Dataspace_capability cap)
			{
				// XXX remove hardcoding
				uint32_t handle = ~0u;

				for_each([&] (Buffer_handle &h) {
					if (h.cap == cap) {
						handle = h.handle;
					}
				});

				return handle;
			}
		};

		Buffer_handle_registry _buffer_handle_registry { _alloc };


		Gpu::Info _info { };

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

				if (params[p] == 0xff) {
					continue;
				}

				uint64_t value;
				int const err = lx_drm_ioctl_etnaviv_gem_param(drm, params[p], &value);
				if (err) {
					return -1;
				}

				info.etnaviv_param[p] = value;
			}
			return 0;
		}

		static int _convert_mt(Gpu::Session::Mapping_type mt)
		{
			using MT = Gpu::Session::Mapping_type;

			switch (mt) {
			case MT::READ:
				return 1;
			case MT::WRITE:
				return 2;
			case MT::NOSYNC:
				return 4;
			case MT::UNKNOWN: [[fallthrough]];
			default:
					return 0;
			}

			return 0;
		}

		Genode::Signal_context_capability _completion_sigh { };
		uint32_t _pending_fence_id { ~0u };

		char const *_name;

		struct Gpu_request
		{
			enum class Op { INVALID, OPEN, CLOSE, NEW, DELETE, EXEC, WAIT_FENCE, MAP, UNMAP };
			enum class Result { SUCCESS, ERR };

			struct Op_data {
				// new
				size_t size;
				// new. close. map, unmap
				Genode::Dataspace_capability buffer_cap;
				// exec, wait_fence
				uint32_t fence_id;
				// map
				int mt;
				// exec
				void *gem_submit;
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

			Buffer_handle_registry &buffer_handle_registry;

			void *gem_submit;
		};

		Drm_worker_args _drm_worker_args {
			.request = Gpu_request {
				.op      = Gpu_request::Op::INVALID,
				.op_data = { 0, Genode::Dataspace_capability() },
				.result  = Gpu_request::Result::ERR,
			},
			.drm_session            = nullptr,
			.info                   = _info,
			.buffer_handle_registry = _buffer_handle_registry,
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

						args.request.result = Gpu_request::Result::SUCCESS;
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

					int err =
						lx_drm_ioctl_etnaviv_gem_new(args.drm_session,
						                             args.request.op_data.size, &handle);
					// XXX check value of err to propagate type of error
					if (err) {
						Genode::error("lx_drm_ioctl_etnaviv_gem_new failed: ", err);
						break;
					}

					unsigned long long offset;
					err = lx_drm_ioctl_etnaviv_gem_info(args.drm_session, handle, &offset);
					if (err) {
						Genode::error("lx_drm_ioctl_etnaviv_gem_info failed: ", err);
						lx_drm_ioctl_gem_close(args.drm_session, handle);
						break;
					}

					Genode::Dataspace_capability cap =
						genode_lookup_cap(args.drm_session, offset, args.request.op_data.size);
					if (!cap.valid()) {
						/* this should never happen */
						Genode::error("genode_lookup_cap for offset: ", Genode::Hex(offset),
						              " failed");
						lx_drm_ioctl_gem_close(args.drm_session, handle);
						break;
					}

					args.buffer_handle_registry.insert(handle, cap);

					args.request.op_data.buffer_cap = cap;
					args.request.result             = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::DELETE:
				{

					unsigned int const handle =
						args.buffer_handle_registry.lookup_buffer(args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					(void)lx_drm_ioctl_gem_close(args.drm_session, handle);
					args.buffer_handle_registry.remove(handle);

					args.request.result = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::EXEC:
				{
					void *gem_submit = args.request.op_data.gem_submit;
					uint32_t fence_id;

					int const err =
						lx_drm_ioctl_etnaviv_gem_submit(args.drm_session,
						                                (unsigned long)gem_submit,
						                                &fence_id);
					if (err) {
						// XXX check value of err
						break;
					}

					args.request.op_data.fence_id = fence_id;
					args.request.result           = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::WAIT_FENCE:
				{
					uint32_t fence_id;

					int const err =
						lx_drm_ioctl_etnaviv_wait_fence(args.drm_session, fence_id);
					if (err) {
						// XXX check value of err
						break;
					}

					args.request.result = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::MAP:
				{
					unsigned int const handle =
						args.buffer_handle_registry.lookup_buffer(args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					int const err =
						lx_drm_ioctl_etnaviv_cpu_prep(args.drm_session, handle,
						                              args.request.op_data.mt);
					if (err) {
						break;
					}
					args.request.result = Gpu_request::Result::SUCCESS;
					break;
				}
				case Gpu_request::Op::UNMAP:
				{
					unsigned int const handle =
						args.buffer_handle_registry.lookup_buffer(args.request.op_data.buffer_cap);
					if (handle == ~0u) {
						break;
					}

					(void)lx_drm_ioctl_etnaviv_cpu_fini(args.drm_session, handle);

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
		                  Genode::Env        &env,
		                  Genode::Entrypoint &ep,
		                  Resources    const &resources,
		                  Label        const &label,
		                  Diag                diag,
		                  char         const *name)
		:
			Session_object { ep, resources, label, diag },
			Genode::Registry<Gpu::Session_component>::Element { registry, *this },
			_env   { env },
			_alloc { _env.ram(), _env.rm() },
		  	_name { name },
			_drm_worker { _drm_worker_run, &_drm_worker_args, _name,
			              Lx::Task::PRIORITY_2, Lx::scheduler() }
		{
			_drm_worker_args.request = Gpu_request {
				.op = Gpu_request::Op::OPEN,
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				Genode::warning("could not open DRM session");
			}
		}

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

		uint32_t pending_fence_id() const { return _pending_fence_id; }

		void submit_completion_signal(uint32_t fence_id)
		{
			_info.last_completed.id = fence_id;
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
			if (!cap.valid()) {
				Genode::error(__func__, ": invalid exec buffer capability");
				throw Gpu::Session::Invalid_state();
			}

			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			void *arg = (void*)_env.rm().attach(cap);

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::EXEC,
				.op_data = { .size = size, .buffer_cap = cap, .gem_submit = arg },
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			_env.rm().detach(arg);

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				throw Gpu::Session::Invalid_state();
			}

			_pending_fence_id = _drm_worker_args.request.op_data.fence_id;

			return Gpu::Info::Execution_buffer_sequence {
				.id = _pending_fence_id };
		}

		bool wait_fence(Genode::uint32_t fence_id) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}
			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::WAIT_FENCE,
				.op_data = { .fence_id = fence_id }
			};

			_drm_worker.unblock();
			Lx::scheduler().schedule();
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;

			bool const finished = _drm_worker_args.request.result == Gpu_request::Result::SUCCESS;
			return finished;
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

			if (_drm_worker_args.request.result != Gpu_request::Result::SUCCESS) {
				throw Gpu::Session::Out_of_ram();
			}
			Genode::Dataspace_capability cap = _drm_worker_args.request.op_data.buffer_cap;

			if (!cap.valid()) {
				Genode::error(__func__, ": buffer_cap invalid");
			}
			_drm_worker_args.request.op = Gpu_request::Op::INVALID;
			return cap;
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

		Handle buffer_handle(Genode::Dataspace_capability cap) override
		{
			unsigned int const handle =
				_buffer_handle_registry.lookup_buffer(cap);
			if (handle == ~0u) {
					return Handle { ._valid = false };
			}

			return Handle { ._valid = true, .value = handle };
		}

		Genode::Dataspace_capability map_buffer(Genode::Dataspace_capability cap,
		                                        bool /* aperture */,
		                                        Mapping_type mt) override
		{
			if (_drm_worker_args.request.valid()) {
				throw Retry_request();
			}

			_drm_worker_args.request = Gpu_request {
				.op      = Gpu_request::Op::MAP,
				.op_data = { .buffer_cap = cap, .mt = _convert_mt(mt), },
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
			Genode::String<64> tmp("gpu_worker-", ++_session_id);
			Genode::memcpy(name, tmp.string(), tmp.length());

			Session::Label const label  { session_label_from_args(args) };
			// Session_policy const policy { label, _env.config.xml()      };

			return new (_alloc) Session_component(_sessions, _env, _env.ep(),
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
				sc.submit_completion_signal(seqno);
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

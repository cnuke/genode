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
#include <base/attached_ram_dataspace.h>
#include <base/heap.h>
#include <base/session_object.h>
#include <base/signal.h>
#include <root/component.h>
#include <session/session.h>
#include <gpu_session/gpu_session.h>
#include <gpu/info_etnaviv.h>

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

	using Root_component = Genode::Root_component<Session_component, Genode::Single_client>;

	struct Seqno { Genode::uint64_t value; };
	struct Virtual_address { unsigned long value; };

	struct Operation;
	struct Request;
}


struct Gpu::Operation
{
	enum class Type {
		INVALID = 0,
		ALLOC   = 1,
		FREE    = 2,
		MAP     = 3,
		UNMAP   = 4,
		EXEC    = 5,
		WAIT    = 6,
	};

	Type type;

	Virtual_address gpu_vaddr;
	unsigned        mode;

	unsigned long  size;
	Buffer_id      id;
	Seqno          seqno;
	Mapping_type   buffer_mapping;

	bool valid() const
	{
		return type != Type::INVALID;
	}

	static char const *type_name(Type type)
	{
		switch (type) {
		case Type::INVALID: return "INVALID";
		case Type::ALLOC:   return "ALLOC";
		case Type::FREE:    return "FREE";
		case Type::MAP:     return "MAP";
		case Type::UNMAP:   return "UNMAP";
		case Type::EXEC:    return "EXEC";
		case Type::WAIT:    return "WAIT";
		}
		return "INVALID";
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, type_name(type));
	}
};


struct Gpu::Request
{
	struct Tag { unsigned long value; };

	Operation operation;

	bool success;

	Tag tag;

	bool valid() const
	{
		return operation.valid();
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, "tag=", tag.value, " success=", success,
		                   " operation=", operation);
	}

	static Gpu::Request initialize(Operation::Type type)
	{
		return Gpu::Request {
			.operation = Operation {
				.type = type,
				.gpu_vaddr = Gpu::Virtual_address { 0 },
				.mode = 0,
				.size = 0,
				.id = Buffer_id { .value = 0 },
				.seqno = Seqno { .value = 0 },
				.buffer_mapping = Mapping_type::INVALID,
			},
			.success = false,
			.tag = Tag { 0 }
		};
	}
};


struct Gpu::Session_component : public Genode::Session_object<Gpu::Session>,
                                public Genode::Registry<Gpu::Session_component>::Element
{
	private:

		Genode::Env  &_env;
		Genode::Heap  _alloc;

		Genode::Attached_ram_dataspace _info_dataspace {
			_env.ram(), _env.rm(), 4096 };

		struct Buffer_handle : Genode::Registry<Buffer_handle>::Element
		{
			Gpu::Buffer_id               const id;
			uint32_t                     const handle;
			Genode::Dataspace_capability const cap;

			Buffer_handle(Genode::Registry<Buffer_handle> &registry,
			       uint32_t handle,
			       Gpu::Buffer_id id,
			       Genode::Dataspace_capability cap)
			:
				Genode::Registry<Buffer_handle>::Element { registry, *this },
				id     { id },
				handle { handle },
				cap    { cap }
			{ }
		};

		struct Buffer_handle_registry : Genode::Registry<Buffer_handle>
		{
			enum { MAX_ARRAY_ITEMS = 64, };

			Handle_id _items[MAX_ARRAY_ITEMS] { };
			Handle_id_array _array { .count = MAX_ARRAY_ITEMS, .items = _items };

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

			Handle_id_array const *populate_array()
			{
				uint32_t i = 0;
				for_each([&] (Buffer_handle const &bh) {
					if (i >= MAX_ARRAY_ITEMS) {
						return;
					}

					_items[i].id     = bh.id.value;
					_items[i].handle = bh.handle;
					i++;
				});

				_array.count = i;
				return &_array;
			}

			void insert(Gpu::Buffer_id id, uint32_t handle,
			            Genode::Dataspace_capability cap)
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

				new (&_alloc) Buffer_handle(*this, handle, id, cap);
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

			Genode::Dataspace_capability lookup_buffer(Gpu::Buffer_id id)
			{
				Genode::Dataspace_capability cap { };
				for_each([&] (Buffer_handle &h) {
					if (h.id.value == id.value) {
						cap = h.cap;
					}
				});
				return cap;
			}

			uint32_t lookup_handle(Gpu::Buffer_id id)
			{
				uint32_t handle = 0;
				for_each([&] (Buffer_handle &h) {
					if (h.id.value == id.value) {
						handle = h.handle;
					}
				});
				return handle;
			}

			template <typename FN> void with_handle(uint32_t handle, FN const &fn)
			{
				for_each([&] (Buffer_handle &h) {
					if (h.handle == handle) {
						fn(h.handle, h.cap);
					}
				});
			}

			bool managed(Gpu::Buffer_id id)
			{
				bool result = false;
				for_each([&] (Buffer_handle &h) {
					if (h.id.value == id.value) {
						result = true;
					}
				});
				return result;
			}
		};

		Buffer_handle_registry _buffer_handle_registry { _alloc };

		Gpu::Info_etnaviv _info { };

		static int _populate_info(void *drm, Gpu::Info_etnaviv &info)
		{
			for (Gpu::Info_etnaviv::Param &p : info.param) {
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

				info.param[p] = value;
			}
			return 0;
		}

		static int _convert_mt(Gpu::Mapping_type mt)
		{
			using MT = Gpu::Mapping_type;

			switch (mt) {
			case MT::READ:
				return 1;
			case MT::WRITE:
				return 2;
			case MT::NOSYNC:
				return 4;
			case MT::INVALID: [[fallthrough]];
			default:
					return 0;
			}

			return 0;
		}

		Genode::Signal_context_capability _completion_sigh { };

		char const *_name;

		Gpu::Request _pending_request   { };
		Gpu::Request _completed_request { };

		Gpu::Execution_buffer_sequence _pending_seqno { };

		struct Local_gpu_request
		{
			enum class Type { INVALID = 0, OPEN, CLOSE };
			Type type;
			bool success;
		};

		struct Drm_worker_args
		{
			Genode::Region_map &rm;

			Gpu::Request *pending_request;
			Gpu::Request *completed_request;

			Local_gpu_request local_request;
			void *drm_session;

			Gpu::Info_etnaviv &info;

			Buffer_handle_registry &buffer_handle_registry;

			void *gem_submit;

			template <typename FN> void for_each_pending_request(FN const &fn)
			{
				if (!pending_request || !pending_request->valid()) {
					return;
				}

				*completed_request = fn(*pending_request);
				*pending_request   = Gpu::Request();
			}
		};

		Drm_worker_args _drm_worker_args {
			.rm = _env.rm(),
			.pending_request = &_pending_request,
			.completed_request = &_completed_request,
			.local_request = Local_gpu_request {
				.type = Local_gpu_request::Type::INVALID,
				.success = false,
			},
			.drm_session            = nullptr,
			.info                   = _info,
			.buffer_handle_registry = _buffer_handle_registry,
		};

		Lx::Task _drm_worker;

		static void _drm_worker_run(void *p)
		{
			Drm_worker_args &args = *static_cast<Drm_worker_args*>(p);
			Buffer_handle_registry &registry = args.buffer_handle_registry;
			Genode::Region_map &rm = args.rm;

			using OP = Gpu::Operation::Type;

			while (true) {

				/* handle local requests first */
				args.local_request.success = false;
				switch (args.local_request.type) {
				case Local_gpu_request::Type::OPEN:
					if (!args.drm_session) {
						args.drm_session = lx_drm_open();
						if (!args.drm_session) {
							break;
						}

						_populate_info(args.drm_session, args.info);

						args.local_request.success = true;
					}
					break;
				case Local_gpu_request::Type::CLOSE:
					lx_drm_close(args.drm_session);
					args.drm_session = nullptr;
					args.local_request.success = true;
					break;
				case Local_gpu_request::Type::INVALID:
					break;
				}

				auto dispatch_pending = [&] (Gpu::Request r) {

					/* clear request result */
					r.success = false;

					switch (r.operation.type) {
					case OP::ALLOC:
					{
						uint32_t const size = r.operation.size;
						uint32_t handle;

						int err =
							lx_drm_ioctl_etnaviv_gem_new(args.drm_session, size, &handle);
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
							genode_lookup_cap(args.drm_session, offset, size);
						registry.insert(r.operation.id, handle, cap);

						r.success = true;
						break;
					}
					case OP::FREE:
					{
						uint32_t const handle = registry.lookup_handle(r.operation.id);

						(void)lx_drm_ioctl_gem_close(args.drm_session, handle);
						registry.remove(handle);

						r.success = true;
						break;
					}
					case OP::EXEC:
					{
						uint32_t const handle = registry.lookup_handle(r.operation.id);

						Handle_id_array const *id_array = registry.populate_array();

						auto exec_buffer = [&] (uint32_t handle, Genode::Dataspace_capability cap) {
							void const *gem_submit = (void*)rm.attach(cap);

							uint32_t fence_id;
							int const err =
								lx_drm_ioctl_etnaviv_gem_submit(args.drm_session,
								                                (unsigned long)gem_submit,
								                                &fence_id,
								                                id_array);
							rm.detach(gem_submit);
							if (err) {
								Genode::error("lx_drm_ioctl_etnaviv_gem_submit: ", err);
								return;
							}

							r.operation.seqno.value = fence_id;
							r.success = true;
						};

						registry.with_handle(handle, exec_buffer);
						break;
					}
					case OP::WAIT:
					{
						uint32_t const fence_id = r.operation.seqno.value;

						int const err =
							lx_drm_ioctl_etnaviv_wait_fence(args.drm_session, fence_id);
						if (err) {
							break;
						}

						r.success = true;
						break;
					}
					case OP::MAP:
					{
						uint32_t const handle = registry.lookup_handle(r.operation.id);
						int const mt = _convert_mt(r.operation.buffer_mapping);

						int const err =
							lx_drm_ioctl_etnaviv_cpu_prep(args.drm_session, handle, mt);
						if (err) {
							break;
						}

						r.success = true;
						break;
					}
					case OP::UNMAP:
					{
						uint32_t const handle = registry.lookup_handle(r.operation.id);
						(void)lx_drm_ioctl_etnaviv_cpu_fini(args.drm_session, handle);

						r.success = true;
						break;
					}
					default:
					break;
					}

					return r;

				};

				args.for_each_pending_request(dispatch_pending);

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
			_drm_worker_args.local_request = Local_gpu_request {
				.type = Local_gpu_request::Type::OPEN,
				.success = false,
			};

			// XXX must not return prematurely
			_drm_worker.unblock();
			Lx::scheduler().schedule();

			if (!_drm_worker_args.local_request.success) {
				Genode::warning("could not open DRM session");
				throw Could_not_open_drm();
			}

			void *info = _info_dataspace.local_addr<void>();
			Genode::memcpy(info, &_info, sizeof (_info));
		}

		~Session_component()
		{
			if (_drm_worker_args.pending_request->valid()) {
				Genode::warning("destructor override currently pending request");
			}

			_drm_worker_args.local_request = Local_gpu_request {
				.type = Local_gpu_request::Type::CLOSE,
				.success = false,
			};

			// XXX must not return prematurely
			_drm_worker.unblock();
			Lx::scheduler().schedule();

			if (!_drm_worker_args.local_request.success) {
				Genode::warning("could not close DRM session - leaking objects");
			}
		}

		char const *name() { return _name; }

		void submit_completion_signal()
		{
			if (_completion_sigh.valid()) {
				Genode::Signal_transmitter(_completion_sigh).submit();
			}
		}

		/******************************
		 ** Session object interface **
		 ******************************/

		void session_quota_upgraded() override
		{
			Session_object::session_quota_upgraded();
		}

		bool _managed_id(Gpu::Request request)
		{
			using OP = Gpu::Operation::Type;

			bool managed = true;
			switch (request.operation.type) {
			case OP::FREE:  [[fallthrough]];
			case OP::MAP:   [[fallthrough]];
			case OP::UNMAP: [[fallthrough]];
			case OP::EXEC:
				managed = _buffer_handle_registry.managed(request.operation.id);
				break;
			default:
				break;
			}

			if (!managed) {
				_completed_request = request;
				_completed_request.success = false;
			}

			return managed;
		}

		Gpu::Request _completed_requests()
		{
			if (_completed_request.valid()) {
				Gpu::Request r = _completed_request;
				_completed_request = Gpu::Request();
				return r;
			}

			return Gpu::Request();
		}

		bool _enqueue_request(Gpu::Request request)
		{
			if (_pending_request.valid()) {
				return false;
			}

			/*
			 * Requests referencing not managed handles will be
			 * marked as complete but unsuccessful and the client
			 * will is notified.
			 */
			if (!_managed_id(request)) {
				return true;
			}

			return true;
		}

		Genode::Dataspace_capability _dataspace(Gpu::Buffer_id id)
		{
			return _buffer_handle_registry.lookup_buffer(id);
		}

		Genode::Dataspace_capability _mapped_dataspace(Gpu::Buffer_id id)
		{
			return _buffer_handle_registry.lookup_buffer(id);
		}

		/***************************
		 ** Gpu session interface **
		 ***************************/

		Genode::Dataspace_capability info_dataspace() const override
		{
			return _info_dataspace.cap();
		}

		struct Invalid_exec_buffer : Genode::Exception { };

		Gpu::Execution_buffer_sequence exec_buffer(Gpu::Buffer_id id,
		                                           Genode::size_t size) override
		{
			Gpu::Request r = Gpu::Request::initialize(Gpu::Operation::Type::EXEC);
			r.operation.id   = id;
			r.operation.size = size;

			if (!_enqueue_request(r)) {
				throw Retry();
			}

			_pending_request = r;

			_drm_worker.unblock();
			Lx::scheduler().schedule();

			Gpu::Execution_buffer_sequence seqno { .id = 0 };

			do {
				if (_completed_request.valid()) {
					break;
				}

				_env.ep().wait_and_dispatch_one_io_signal();

			} while (true);

			if (_completed_request.success) {
				seqno = Gpu::Execution_buffer_sequence {
					.id = _completed_request.operation.seqno.value };
			} else {
				throw Invalid_exec_buffer();
			}

			_pending_seqno = seqno;

			return seqno;
		}

		bool complete(Gpu::Execution_buffer_sequence seqno)
		{
			Gpu::Request r = Gpu::Request::initialize(Gpu::Operation::Type::WAIT);
			r.operation.seqno = Gpu::Seqno { .value = _pending_seqno.id };

			if (!_enqueue_request(r)) {
				throw Retry();
			}

			_pending_request = r;

			_drm_worker.unblock();
			Lx::scheduler().schedule();

			do {
				if (_completed_request.valid()) {
					break;
				}

				_env.ep().wait_and_dispatch_one_io_signal();

			} while (true);

			return _completed_request.success;
		}

		void completion_sigh(Genode::Signal_context_capability sigh) override
		{
			_completion_sigh = sigh;
		}

		struct Retry : Genode::Exception { };

		Genode::Dataspace_capability alloc_buffer(Gpu::Buffer_id id,
		                                          Genode::size_t size) override
		{
			Gpu::Request r = Gpu::Request::initialize(Gpu::Operation::Type::ALLOC);
			r.operation.id   = id;
			r.operation.size = size;

			if (!_enqueue_request(r)) {
				throw Retry();
			}

			_pending_request = r;

			Genode::Dataspace_capability cap { };

			_drm_worker.unblock();
			Lx::scheduler().schedule();

			do {
				if (_completed_request.valid()) {
					if (_completed_request.success) {
						cap = _dataspace(_completed_request.operation.id);
					}
					break;
				}

				_env.ep().wait_and_dispatch_one_io_signal();

			} while (true);

			return cap;
		}

		void free_buffer(Gpu::Buffer_id id) override
		{
			Gpu::Request r = Gpu::Request::initialize(Gpu::Operation::Type::FREE);
			r.operation.id = id;

			if (!_enqueue_request(r)) {
				throw Retry();
			}

			_pending_request = r;

			_drm_worker.unblock();
			Lx::scheduler().schedule();

			do {
				if (_completed_request.valid()) {
					if (!_completed_request.success) {
						Genode::warning("free buffer ", id.value, " failed");
					}
					break;
				}

				_env.ep().wait_and_dispatch_one_io_signal();

			} while (true);
		}

		Genode::Dataspace_capability map_buffer(Gpu::Buffer_id id,
		                                        bool /* aperture */,
		                                        Gpu::Mapping_type mt) override
		{
			Gpu::Request r = Gpu::Request::initialize(Gpu::Operation::Type::MAP);
			r.operation.id             = id;
			r.operation.buffer_mapping = mt;

			if (!_enqueue_request(r)) {
				throw Retry();
			}

			_pending_request = r;
			Genode::Dataspace_capability cap { };

			_drm_worker.unblock();
			Lx::scheduler().schedule();

			do {
				if (_completed_request.valid()) {
					if (_completed_request.success) {
						cap = _buffer_handle_registry.lookup_buffer(id);
					}
					break;
				}

				_env.ep().wait_and_dispatch_one_io_signal();

			} while (true);

			return cap;
		}

		void unmap_buffer(Gpu::Buffer_id id) override
		{
			Gpu::Request r = Gpu::Request::initialize(Gpu::Operation::Type::UNMAP);
			r.operation.id = id;

			if (!_enqueue_request(r)) {
				throw Retry();
			}

			_pending_request = r;

			_drm_worker.unblock();
			Lx::scheduler().schedule();

			do {
				if (_completed_request.valid()) {
					if (!_completed_request.success) {
						Genode::warning("unmap buffer ", id.value, " failed");
					}
					break;
				}

				_env.ep().wait_and_dispatch_one_io_signal();

			} while (true);
		}

		bool map_buffer_ppgtt(Gpu::Buffer_id, Gpu::addr_t) override
		{
			Genode::warning(__func__, ": not implemented");
			return false;
		}

		void unmap_buffer_ppgtt(Gpu::Buffer_id, Gpu::addr_t) override
		{
			Genode::warning(__func__, ": not implemented");
		}

		bool set_tiling(Gpu::Buffer_id, unsigned) override
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
				sc.submit_completion_signal();
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

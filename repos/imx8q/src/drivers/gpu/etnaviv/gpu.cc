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
#include <lx_emul_cc.h>

// XXX move execution of lx_drm calls into drm-worker task
#include <lx_drm.h>

// Genode::Ram_dataspace_capability lx_drm_object_dataspace(unsigned long, unsigned long);

extern Genode::Dataspace_capability genode_lookup_cap(void *, unsigned int);
extern unsigned int                 genode_lookup_handle(void *, Genode::Dataspace_capability);

namespace Gpu {

	using namespace Genode;

	struct Session_component;
	struct Root;

	using Root_component = Genode::Root_component<Session_component, Genode::Multiple_clients>;
}

struct Gpu::Session_component : public Genode::Session_object<Gpu::Session>
{
	private:

		void *_drm_session { nullptr };

		Gpu::Info _info { 0, 0, 0, 0, Gpu::Info::Execution_buffer_sequence { 0 } };

		int _populate_info(void *drm, Gpu::Info &info)
		{
			for (Gpu::Info::Etnaviv_param &p : _info.etnaviv_param) {
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

	public:

		struct Could_not_open_drm : Genode::Exception { };

		/**
		 * Constructor
		 */
		Session_component(Genode::Entrypoint &ep,
		                  Resources    const &resources,
		                  Label        const &label,
		                  Diag                diag)
		:
			Session_object { ep, resources, label, diag }
		{
			_drm_session = lx_drm_open();
			if (!_drm_session) {
				throw Could_not_open_drm();
			}

			_populate_info(_drm_session, _info);
		}

		~Session_component()
		{
			lx_drm_close(_drm_session);
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

		Info info() const
		{
			return _info;
		}

		Gpu::Info::Execution_buffer_sequence exec_buffer(Genode::Dataspace_capability cap,
		                                                 Genode::size_t /* size */) override
		{
			Gpu::Info::Execution_buffer_sequence seq { .id = 0 };

			unsigned int const handle = genode_lookup_handle(_drm_session, cap);
			if (handle == ~0u) {
				// XXX check value of err
				throw Gpu::Session::Invalid_state();
			}

			int const err = lx_drm_ioctl_etnaviv_gem_submit(_drm_session, handle, &seq.id);
			if (err) {
				// XXX check value of err
				throw Gpu::Session::Invalid_state();
			}

			return seq;
		}

		void completion_sigh(Genode::Signal_context_capability sigh) override
		{
			(void)sigh;
		}

		Genode::Dataspace_capability alloc_buffer(Genode::size_t size) override
		{
			uint32_t handle;

			int const err = lx_drm_ioctl_etnaviv_gem_new(_drm_session, size, &handle);
			if (err) {
				// XXX check value of err
				throw Gpu::Session::Out_of_ram();
			}

			Genode::Dataspace_capability cap = genode_lookup_cap(_drm_session, handle);
			if (!cap.valid()) {
				// XXX check value of err
				throw Gpu::Session::Out_of_ram();
			}

			return cap;
		}

		void free_buffer(Genode::Dataspace_capability cap) override
		{
			unsigned int const handle = genode_lookup_handle(_drm_session, cap);
			if (handle == ~0u) {
				return;
			}

			(void)lx_drm_ioctl_gem_close(_drm_session, handle);
		}

		Genode::Dataspace_capability map_buffer(Genode::Dataspace_capability cap,
		                                        bool /* aperture */) override
		{
			(void)cap;

			unsigned int const handle = genode_lookup_handle(_drm_session, cap);
			if (handle == ~0u) {
				return Genode::Dataspace_capability();
			}

			int const err = lx_drm_ioctl_etnaviv_prep_cpu(_drm_session, handle);
			if (err) {
				return Genode::Dataspace_capability();
			}

			return cap;
		}

		void unmap_buffer(Genode::Dataspace_capability cap) override
		{
			unsigned int const handle = genode_lookup_handle(_drm_session, cap);
			if (handle == ~0u) {
				return;
			}

			(void)lx_drm_ioctl_etnaviv_fini_cpu(_drm_session, handle);
		}

		bool map_buffer_ppgtt(Genode::Dataspace_capability /* cap */,
		                      Gpu::addr_t /* va */) override
		{
			return false;
		}
};


struct Gpu::Root : Gpu::Root_component
{
	private:

		Genode::Env &_env;

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

	protected:

		Session_component *_create_session(char const *args) override
		{
			(void)args;

			throw Genode::Service_denied();
		}

		void _upgrade_session(Session_component *s, char const *args) override
		{
			(void)s;
			(void)args;
		}

		void _destroy_session(Session_component *s) override
		{
			(void)s;
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &alloc)
		:
			Root_component { env.ep(), alloc },
			_env           { env }
		{ }
};


static Genode::Constructible<Gpu::Root> _gpu_root { };


void lx_emul_announce_gpu_session(void)
{
	if (!_gpu_root.constructed()) {
		_gpu_root.construct(Lx_kit::env().env, Lx_kit::env().heap);

		Genode::Entrypoint &ep = Lx_kit::env().env.ep();
		Lx_kit::env().env.parent().announce(ep.manage(*_gpu_root));
	}
}

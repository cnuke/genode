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
#include <root/component.h>
#include <session/session.h>

/* Linux emulation */
#include <lx_emul_cc.h>


Genode::Ram_dataspace_capability lx_drm_object_dataspace(unsigned long, unsigned long);

namespace Gpu {

	struct Session_component;
	struct Root;

	using Root_component = Genode::Root_component<Session_component, Genode::Multiple_clients>;
}

struct Gpu::Session_component : Genode::Session_object<Gpu::Session>
{
	private:

		Gpu::Info _info { 0, 0, 0, 0, 0 };

		int _populate_info(void *drm, Gpu::Info &info)
		{
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

		/**
		 * Constructor
		 */
		Session_component(Genode::Entrypoint &ep,
		                  Resources    const &resources,
		                  Label        const &label,
		                  Diag                diag)
		:
			Session_object { ep, resources, label, diag }
		{ }

		~Session_component() { }

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
		                                                 Genode::size_t size) override
		{
			(void)cap;
			(void)size;

			// lx_drm_ioctl_etnaviv_gem_submit

			return 0;
		}

		void completion_sigh(Genode::Signal_context_capability sigh) override
		{
			(void)sigh;
		}

		Genode::Dataspace_capability alloc_buffer(Genode::size_t size) override
		{
			(void)size;

			// lx_drm_ioctl_etnaviv_gem_new

			return Genode::Dataspace_capability();
		}

		void free_buffer(Genode::Dataspace_capability cap) override
		{
			(void)cap;

			// lx_drm_ioctl_gem_close
		}

		Genode::Dataspace_capability map_buffer(Genode::Dataspace_capability cap
		                                        bool aperture) override
		{
			(void)cap;
			(void)aperture;

			// lx_drm_ioctl_etnaviv_prep_cpu

			return Genode::Dataspace_capability();
		}

		void unmap_buffer(Genode::Dataspace_capability cap) override
		{
			(void)cap;

			// lx_drm_ioctl_etnaviv_fini_cpu
		}

		bool map_buffer_ppgtt(Genode::Dataspace_capability cap,
		                      Gpu::addr_t va) override
		{
			(void)cap;
			(void)va;

			// lx_drm_ioctl_etnaviv_prep_cpu

			return false;
		}
};


struct Gpu::Root : Gpu::Root_component
{
	private:

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

	protected:

		Session_component *_create_session(char const *args) override
		{
			(void)args;

			throw Server_denied;
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

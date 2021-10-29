/*
 * \brief  Genode Gpu service provider C-API
 * \author Josef Soentgen
 * \date   2021-10-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <root/component.h>
#include <os/reporter.h>
#include <os/session_policy.h>

#include <genode_c_api/gpu.h>

using namespace Genode;


class Root;


class genode_gpu_session : public Session_object<Gpu::Session>,
                           public Registry<genode_gpu_session>::Element
{
	private:

		Env                 &_env;
		Session_label const  _label

		Genode::Signal_context_capability _completion_sigh { };

	public:

		genode_gpu_session(Genode::Registry<genode_gpu_session> &registry,
		                   Genode::Env        &env,
		                   Genode::Entrypoint &ep,
		                   Resources    const &resources,
		                   Label        const &label,
		                   Diag                diag,
		                   char         const *name);

		virtual ~genode_gpu_session();

		/***************************
		 ** Gpu session interface **
		 ***************************/

		Genode::Dataspace_capability info_dataspace() const override;

		Gpu::Sequence_number exec_buffer(Gpu::Buffer_id id,
		                                 size_t size) override;

		bool complete(Gpu::Sequence_number seqno) override;

		void completion_sigh(Genode::Signal_context_capability sigh) override
		{
			_completion_sigh = sigh;
		}

		Genode::Dataspace_capability alloc_buffer(Gpu::Buffer_id id,
		                                          size_t size) override;

		void free_buffer(Gpu::Buffer_id id) override;

		Genode::Dataspace_capability map_buffer(Gpu::Buffer_id id,
		                                        bool /* aperture */,
		                                        Gpu::Mapping_attributes attrs);

		void unmap_buffer(Gpu::Buffer_id id) override;

		bool map_buffer_ppgtt(Gpu::Buffer_id, Gpu::addr_t) override
		{
			warning(__func__, ": not implemented");
			return false;
		}

		void unmap_buffer_ppgtt(Gpu::Buffer_id, Gpu::addr_t) override
		{
			warning(__func__, ": not implemented");
		}

		bool set_tiling(Gpu::Buffer_id, unsigned) override
		{
			warning(__func__, ": not implemented");
			return false;
		}
};

class Root : public Root_component<genode_gpu_session, Single_client>
{
	private:

		Env       &_env;
		Allocator &_alloc;

		uint32_t _session_id;

		Genode::Registry<genode_gpu_session> _sessions;

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

	protected:

		genode_gpu_session *_create_session(char const *args,
		                                    Affinity const &) override;

		void _destroy_session(genode_gpu_session *session) override;

	public:

		Root(Env &env, Genode::Allocator &alloc);

}

static ::Root                   *_gpu_root  = nullptr;
static genode_gpu_rpc_callbacks *_callbacks = nullptr;


genode_gpu_session::genode_gpu_session(Genode::Registry<genode_gpu_session> &registry,
                                       Genode::Env                          &env,
                                       Genode::Entrypoint                   &ep,
                                       Resources                      const &resources,
                                       Label                          const &label,
                                       Diag                                  diag)
:
	Session_object                                { ep, resources, label, diag },
	Genode::Registry<genode_gpu_session>::Element { registry, *this },
	_env                                          { env }
{ }


Genode::Dataspace_capability genode_gpu_session::info_dataspace()
{
	return _callbacks->info_dataspace_fn();
}


Gpu::Sequence_number genode_gpu_session::exec_buffer(Gpu::Buffer_id id,
                                                     size_t         size)
{
	return Gpu::Sequence_number {
		.value = _callbacks->exec_buffer_fn(id.value, sizet) };
}


bool genode_gpu_session::complete(Gpu::Sequence_number seqno)
{
	return _callbacks->complete_fn(seqno.value);
}


Dataspace_capability genode_gpu_session::alloc_buffer(Gpu::Buffer_id id,
                                                      size_t         size)
{
	return _callbacks->alloc_buffer_fn(id.value, size);
}


void genode_gpu_session::free_buffer(Gpu::Buffer_id id)
{
	_callbacks->free_buffer_fn(id.value);
}


Dataspace_capability genode_gpu_session::map_buffer(Gpu::Buffer_id id,
                                                    bool /* aperture */,
                                                    Gpu::Mapping_attributes mattrs)
{
	int attrs = 0;

	if (mattrs.readable)
		attrs |= GENODE_GPU_ATTR_READ;

	if (mattrs.writeable)
		attrs |= GENODE_GPU_ATTR_WRITE;

	return _callbacks->map_buffer_fn(id.value, 0, attrs);
}


void genode_gpu_session::unmap_buffer(Gpu::Buffer_id id)
{
	_callbacks->unmap_buffer(id.value);
}


genode_gpu_session *::Root::_create_session(char const *args,
                                            Affinity const &)
{
	Session::Label const label  { session_label_from_args(args) };

	return new (_alloc) Session_component(_sessions, _env, _env.ep(),
	                                      session_resources_from_args(args),
	                                      label,
	                                      session_diag_from_args(args),
	                                      name);
	return nullptr;
}


void genode_gpu_session::_destroy_session(genode_gpu_session *session)
{
}


::Root::Root(Env &env, Allocator &alloc)
:
	Root_component { env.ep(), alloc },
	_env           { env },
	_alloc         { alloc },
	_session_id    { 0 },
	_sessions      { }
{ }


void genode_gpu_init(genode_env               *env_ptr,
                     genode_allocator         *alloc_ptr,
                     genode_signal_handler    *,
                     genode_usb_rpc_callbacks *callbacks)
{
	static ::Root root(*static_cast<Env*>(env_ptr),
	                   *static_cast<Allocator*>(alloc_ptr));

	_gpu_root  = &root;
	_callbacks = callbacks;
}


void genode_gpu_annouce_service()
{
	if (_gpu_root)
		_gpu_root->annouce_service();
}


struct genode_gpu_session *
genode_gpu_session_by_name(const char * name)
{
	_gpu_root ? _gpu_root->session(name) : nullptr;
}

/*
 * \brief  DRM ioctl backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Alexander Boettcher
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/sleep.h>

#include <gpu_session/connection.h>
#include <gpu/info_iris.h>
#include <util/retry.h>

extern "C" {
#include <errno.h>
#include <drm.h>
#include <i915_drm.h>

#define DRM_NUMBER(req) ((req) & 0xff)
}

using Genode::addr_t;
using Genode::Attached_dataspace;
using Genode::Constructible;

enum { verbose_ioctl = false };

namespace Utils
{
	uint64_t limit_to_48bit(uint64_t addr) {
		return addr & ((1ULL << 48) - 1); }
}


/**
 * Get DRM command number
 */
static unsigned long command_number(unsigned long request)
{
	return request & 0xffu;
}


/**
 * Get device specific command number
 */
static unsigned long device_number(unsigned long request)
{
	return command_number(request) - DRM_COMMAND_BASE;
}


/**
 * Check if request is device command
 */
static bool device_ioctl(unsigned long request)
{
	unsigned long const cmd = command_number(request);
	return cmd >= DRM_COMMAND_BASE && cmd < DRM_COMMAND_END;
}


static const char *command_name(unsigned long request)
{
	if (IOCGROUP(request) != DRM_IOCTL_BASE)
		return "<non-DRM>";


	if (!device_ioctl(request)) {
		long const cmd = command_number(request);
		switch (cmd) {
		case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE): return "DRM_IOCTL_GEM_CLOSE";
		case DRM_NUMBER(DRM_IOCTL_GEM_FLINK): return "DRM_IOCTL_GEM_FLINK";
		case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_CREATE): return "DRM_IOCTL_SYNCOBJ_CREATE";
		case DRM_NUMBER(DRM_IOCTL_PRIME_HANDLE_TO_FD): return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
		default:                  return "<unknown command>";
		}
	}

	switch (device_number(request)) {
	case DRM_I915_INIT:                  return "DRM_I915_INIT";
	case DRM_I915_FLUSH:                 return "DRM_I915_FLUSH";
	case DRM_I915_FLIP:                  return "DRM_I915_FLIP";
	case DRM_I915_BATCHBUFFER:           return "DRM_I915_BATCHBUFFER";
	case DRM_I915_IRQ_EMIT:              return "DRM_I915_IRQ_EMIT";
	case DRM_I915_IRQ_WAIT:              return "DRM_I915_IRQ_WAIT";
	case DRM_I915_GETPARAM:              return "DRM_I915_GETPARAM";
	case DRM_I915_SETPARAM:              return "DRM_I915_SETPARAM";
	case DRM_I915_ALLOC:                 return "DRM_I915_ALLOC";
	case DRM_I915_FREE:                  return "DRM_I915_FREE";
	case DRM_I915_INIT_HEAP:             return "DRM_I915_INIT_HEAP";
	case DRM_I915_CMDBUFFER:             return "DRM_I915_CMDBUFFER";
	case DRM_I915_DESTROY_HEAP:          return "DRM_I915_DESTROY_HEAP";
	case DRM_I915_SET_VBLANK_PIPE:       return "DRM_I915_SET_VBLANK_PIPE";
	case DRM_I915_GET_VBLANK_PIPE:       return "DRM_I915_GET_VBLANK_PIPE";
	case DRM_I915_VBLANK_SWAP:           return "DRM_I915_VBLANK_SWAP";
	case DRM_I915_HWS_ADDR:              return "DRM_I915_HWS_ADDR";
	case DRM_I915_GEM_INIT:              return "DRM_I915_GEM_INIT";
	case DRM_I915_GEM_EXECBUFFER:        return "DRM_I915_GEM_EXECBUFFER";
	case DRM_I915_GEM_PIN:               return "DRM_I915_GEM_PIN";
	case DRM_I915_GEM_UNPIN:             return "DRM_I915_GEM_UNPIN";
	case DRM_I915_GEM_BUSY:              return "DRM_I915_GEM_BUSY";
	case DRM_I915_GEM_THROTTLE:          return "DRM_I915_GEM_THROTTLE";
	case DRM_I915_GEM_ENTERVT:           return "DRM_I915_GEM_ENTERVT";
	case DRM_I915_GEM_LEAVEVT:           return "DRM_I915_GEM_LEAVEVT";
	case DRM_I915_GEM_CREATE:            return "DRM_I915_GEM_CREATE";
	case DRM_I915_GEM_PREAD:             return "DRM_I915_GEM_PREAD";
	case DRM_I915_GEM_PWRITE:            return "DRM_I915_GEM_PWRITE";
	case DRM_I915_GEM_MMAP:              return "DRM_I915_GEM_MMAP";
	case DRM_I915_GEM_SET_DOMAIN:        return "DRM_I915_GEM_SET_DOMAIN";
	case DRM_I915_GEM_SW_FINISH:         return "DRM_I915_GEM_SW_FINISH";
	case DRM_I915_GEM_SET_TILING:        return "DRM_I915_GEM_SET_TILING";
	case DRM_I915_GEM_GET_TILING:        return "DRM_I915_GEM_GET_TILING";
	case DRM_I915_GEM_GET_APERTURE:      return "DRM_I915_GEM_GET_APERTURE";
	case DRM_I915_GEM_MMAP_GTT:          return "DRM_I915_GEM_MMAP_GTT";
	case DRM_I915_GET_PIPE_FROM_CRTC_ID: return "DRM_I915_GET_PIPE_FROM_CRTC_ID";
	case DRM_I915_GEM_MADVISE:           return "DRM_I915_GEM_MADVISE";
	case DRM_I915_OVERLAY_PUT_IMAGE:     return "DRM_I915_OVERLAY_PUT_IMAGE";
	case DRM_I915_OVERLAY_ATTRS:         return "DRM_I915_OVERLAY_ATTRS";
	case DRM_I915_GEM_EXECBUFFER2:       return "DRM_I915_GEM_EXECBUFFER2";
	case DRM_I915_REG_READ:              return "DRM_I915_REG_READ";
	case DRM_I915_GET_RESET_STATS:       return "DRM_I915_GET_RESET_STATS";
	case DRM_I915_GEM_CONTEXT_CREATE:    return "DRM_I915_GEM_CONTEXT_CREATE";
	default:
		return "<unknown driver>";
	}
}

static void dump_ioctl(unsigned long request)
{
	using namespace Genode;

	log("ioctl(request=", Hex(request),
	    (request & 0xe0000000) == IOC_OUT   ? " out"   :
	    (request & 0xe0000000) == IOC_IN    ? " in"    :
	    (request & 0xe0000000) == IOC_INOUT ? " inout" : " void",
	    " len=", IOCPARM_LEN(request),
	    " cmd=",command_name(request), " (", Hex(command_number(request)), ")");
}


class Drm_call
{
	private:

		Genode::Env      &_env;
		Genode::Heap      _heap               { _env.ram(), _env.rm() };
		Gpu::Connection   _gpu_session        { _env };

		Gpu::Info_iris  const &_gpu_info      { *_gpu_session.attached_info<Gpu::Info_iris>() };
		size_t            _available_gtt_size { _gpu_info.aperture_size };

		using Offset = unsigned long;

		struct Gpu_virtual_address : Gpu::Virtual_address
		{
			Gpu_virtual_address(unsigned long value)
			{
				// XXX throw rather than cap silently?
				Gpu::Virtual_address::value = Utils::limit_to_48bit(value);
			}
		};

		struct Buffer_handle;
		typedef Genode::Id_space<Buffer_handle>::Element Handle;
		typedef Genode::Id_space<Buffer_handle>::Id      Handle_id;

		struct Buffer_handle
		{
			Genode::Dataspace_capability const cap;
			Genode::size_t               const size;
			Handle                       const handle;

			Constructible<Attached_dataspace> buffer_attached { };

			Genode::Dataspace_capability map_cap    { };
			Offset                       map_offset { 0 };

			Gpu_virtual_address          gpu_vaddr { 0 };
			Gpu::Seqno                   seqno { };

			bool                         gpu_vaddr_valid { false };
			bool                         busy            { false };

			Buffer_handle(Genode::Id_space<Buffer_handle> &space,
			              Genode::Dataspace_capability cap,
			              Gpu::Buffer_id id,
			              Genode::size_t size)
			:
				cap(cap), size(size),
				handle { *this, space, Handle_id { .value = id.value } }
			{
				if (!cap.valid() || !size)
					Genode::warning("invalid Buffer_handle ?");
			}

			virtual ~Buffer_handle() { }

			bool valid() const { return cap.valid() && size != 0; }

			bool mmap(Genode::Env &env)
			{
				if (!valid())
					return false;

				if (!buffer_attached.constructed())
					buffer_attached.construct(env.rm(), cap);

				return buffer_attached.constructed();
			}

			addr_t mmap_addr() {
				return reinterpret_cast<addr_t>(buffer_attached->local_addr<addr_t>());
			}
		};

		Genode::Id_space<Buffer_handle> _buffer_handles { };

		using Buffer = Genode::Registered<Buffer_handle>;

		Genode::Registry<Buffer> _buffer_registry { };

		struct Sync_obj
		{
			using Sync = Genode::Id_space<Sync_obj>;
			using Id = Sync::Id;

			Sync::Element id;

			Sync_obj(Sync &space)
			: id(*this, space)
			{ }
		};

		Genode::Id_space<Sync_obj> _sync_objects { };

		/** Gpu::Request **/

		Genode::Blockade _request_completion { };

		void _handle_request_completion()
		{
			_request_completion.wakeup();
		}

		Genode::Signal_handler<Drm_call> _request_completion_sigh;

		bool _wait_for_request(Gpu::Request &r)
		{
			_request_completion.block();

			bool success = false;
			_gpu_session.for_each_completed_request([&] (Gpu::Request completed) {
				if (completed.tag.value != r.tag.value) {
					Genode::error(__func__, ": wrong: request: ", completed.tag.value,
					                        " expected: ", r.tag.value);
					return;
				}

				if (!completed.success) {
					return;
				}

				success = completed.success;
				r = completed;
			});

			return success;
		}

		static Gpu::Request _initialize_request()
		{
			static uint32_t tag = 0;

			return Gpu::Request {
				.operation = Gpu::Operation {
					.type = Gpu::Operation::Type::INVALID,
					.gpu_vaddr = Gpu::Virtual_address { 0 },
					.aperture = false,
					.mode = 0,
					.size = 0,
					.id = Gpu::Buffer_id { .value = 0 },
					.seqno = Gpu::Seqno { .value = 0 },
					.buffer_mapping = Gpu::Buffer_mapping::UNKNOWN,
				},
				.success = false,
				.tag = Gpu::Request::Tag { .value = tag++ },
			};
		}

		struct Cap_handle
		{
			Genode::Dataspace_capability cap { };
			Gpu::Buffer_id id { };

			bool valid() const
			{
				return cap.valid() && id.valid();
			}
		};

		Cap_handle _alloc_buffer(Genode::size_t const size)
		{
			Cap_handle result { };

			Gpu::Request r = _initialize_request();
			r.operation.type = Gpu::Operation::Type::ALLOC;
			r.operation.size = size;

			for (int i = 0; i < 2; i++) {

				if (!_gpu_session.enqueue_request(r)) {
					Genode::error(__func__, ": could not enqueue request");
					return Cap_handle { };
				}

				bool success = _wait_for_request(r);
				if (success) {
					Genode::Dataspace_capability cap =
						_gpu_session.dataspace(r.operation.id);
					result = Cap_handle {
						.cap = cap,
						.id = r.operation.id,
					};
					break;
				}

				/* retry once */
				if (!success && i == 0) {
					_gpu_session.upgrade_ram(size);
					_gpu_session.upgrade_caps(4);
					continue;
				}

				break;
			}

			return result;
		}

		void __free_buffer(Gpu::Buffer_id id)
		{
			Gpu::Request r = _initialize_request();
			r.operation.type = Gpu::Operation::Type::FREE;
			r.operation.id = id;

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue request");
				return;
			}

			bool const success = _wait_for_request(r);
			if (!success) {
				Genode::warning("could not free buffer: ",
				                r.operation.id.value);
			}
		}


		bool _map_buffer_ppgtt(Buffer_handle &buffer, Gpu_virtual_address const vaddr)
		{
			if (buffer.gpu_vaddr_valid)
				Genode::warning(__func__, " already have a gpu virtual address ",
				                Genode::Hex(buffer.gpu_vaddr.value), " vs ",
				                Genode::Hex(vaddr.value));

			// /* XXX out of cap XXX */
			// /* XXX out of memory XXX */
			Gpu::Buffer_id const id { .value = (uint32_t)buffer.handle.id().value };

			Gpu::Request r = _initialize_request();
			r.operation.type     = Gpu::Operation::Type::MAP;
			r.operation.id       = id;
			r.operation.gpu_vaddr = vaddr;

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue request");
				return false;
			}

			bool success = _wait_for_request(r);
			if (!success) {
				Genode::error("could not insert buffer into PPGTT");
				return false;
			}

			buffer.gpu_vaddr       = vaddr;
			buffer.gpu_vaddr_valid = true;
			return true;
		}

		void _unmap_buffer_ppgtt(Buffer_handle &buffer)
		{
			if (!buffer.gpu_vaddr_valid) return;

			Gpu::Request r = _initialize_request();
			r.operation.type      = Gpu::Operation::Type::UNMAP;
			r.operation.id        = Gpu::Buffer_id { .value = (uint32_t)buffer.handle.id().value };
			r.operation.gpu_vaddr = buffer.gpu_vaddr;

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue request");
				return;
			}

			bool const success = _wait_for_request(r);
			if (!success) {
				Genode::warning("unmapping PPGTT ", Genode::Hex(buffer.gpu_vaddr.value),
				                " failed");
				return;
			}

			buffer.gpu_vaddr_valid = false;
		}

		void _unmap_buffer_ggtt(Buffer_handle &h)
		{
			Offset const offset = h.map_offset;
			// XXX check if it is necessary to detach before the mapped cap
			//     gets invalid
			_env.rm().detach(h.map_offset);
			h.map_offset = 0;
			h.map_cap = Genode::Dataspace_capability();

			Gpu::Request r = _initialize_request();
			r.operation.type = Gpu::Operation::Type::UNMAP;
			r.operation.id   = Gpu::Buffer_id { .value = (uint32_t)h.handle.id().value };
			r.operation.gpu_vaddr = Gpu::Virtual_address { 0 };

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue request");
				return;
			}

			bool const success = _wait_for_request(r);
			if (!success) {
				Genode::warning("unmapping GTT ", Genode::Hex(offset),
				                " failed");
				return;
			}

			_available_gtt_size += h.size;
		}

		int _free_buffer(Handle_id const &id)
		{
			bool const handled = _apply_buffer(id, [&] (Buffer_handle &bh) {
				if (bh.map_cap.valid())
					_unmap_buffer_ggtt(bh);

				if (bh.gpu_vaddr_valid) {
					_unmap_buffer_ppgtt(bh);
				}
				__free_buffer(Gpu::Buffer_id { .value = (uint32_t)bh.handle.id().value});

				Genode::destroy(&_heap, (&bh));
			});

			if (!handled) {
				Genode::error(__func__, ": invalid handle ", id.value);
				Genode::sleep_forever();
			}

			return handled ? 0 : -1;
		}

		Offset _map_buffer_ggtt(Buffer_handle &bh)
		{
			Offset offset = 0;

			if (bh.map_cap.valid()) {
				offset = bh.map_offset;
				return offset;
			}

			Gpu::Buffer_id const id { .value = (uint32_t)bh.handle.id().value };

			Gpu::Request r = _initialize_request();
			r.operation.type     = Gpu::Operation::Type::MAP;
			r.operation.id       = id;
			r.operation.aperture = true;

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue request");
				return -1;
			}

			bool success = _wait_for_request(r);
			if (success) {
				Genode::Dataspace_capability cap =
					_gpu_session.mapped_dataspace(id);
				if (!cap.valid()) {
					success = false;
				} else {
					try {
						bh.map_cap = cap;
						bh.map_offset = static_cast<Offset>(_env.rm().attach(bh.map_cap));
						offset       = bh.map_offset;
						_available_gtt_size -= bh.size;
					} catch (...) {
						// if (bh.map_cap.valid()) {
						// 	_gpu_session.unmap_buffer(bh.map_cap);
						// }
						// XXX leak for now
						Genode::error("could not attach GEM buffer handle: ", bh.handle);
						Genode::sleep_forever();
					}
				}
			}
			return offset;
		}

		Offset _map_buffer(Handle_id const &id)
		{
			Offset offset = 0;

			bool handled = _apply_buffer(id, [&] (Buffer_handle &bh) {
				offset = _map_buffer_ggtt(bh);
			});

			if (!handled) {
				Genode::error(__func__, ": invalid handle ", id.value);
				Genode::sleep_forever();
			}

			return offset;
		}

		bool _set_tiling(Gpu::Buffer_id id, uint32_t mode)
		{
			Gpu::Request r = _initialize_request();
			r.operation.type = Gpu::Operation::Type::VIEW;
			r.operation.id   = id;
			r.operation.mode = mode;

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue request");
				return -1;
			}

			return _wait_for_request(r);
		}

		/*******************
		 ** lookup buffer **
		 *******************/

		template <typename FUNC>
		bool _apply_buffer(Handle_id const &id, FUNC const &fn)
		{
			bool found = false;

			_buffer_handles.apply<Buffer_handle>(id, [&](Buffer_handle &bh) {
				fn(bh);
				found = true;
			});

			return found;
		}

		template <typename FUNC>
		void _for_each_buffer(FUNC const &fn)
		{
			_buffer_handles.for_each<Buffer_handle>(fn);
		}

		/************
		 ** ioctls **
		 ************/


		int _device_gem_get_aperture_size(void *arg)
		{
			drm_i915_gem_get_aperture * const p = reinterpret_cast<drm_i915_gem_get_aperture*>(arg);
			p->aper_size           = _gpu_info.aperture_size;
			p->aper_available_size = _available_gtt_size;
			Genode::warning(__func__, ": available_gtt_size is not properly accounted");
			return 0;
		}

		int _device_gem_create(void *arg)
		{
			auto const p = reinterpret_cast<drm_i915_gem_create*>(arg);

			uint64_t const size = (p->size + 0xfff) & ~0xfff;

			Cap_handle cap_handle = _alloc_buffer(size);
			if (!cap_handle.valid()) {
				return -1;
			}

			try {
				Buffer_handle *buffer =
					new (&_heap) Buffer_handle(_buffer_handles, cap_handle.cap,
					                           cap_handle.id, size);
				p->handle = buffer->handle.id().value;
				p->size = size;
				return 0;
			} catch (...) {
				__free_buffer(cap_handle.id);
			}
			return -1;
		}

		int _device_gem_mmap(void *arg)
		{
			auto      const p      = reinterpret_cast<drm_i915_gem_mmap *>(arg);
			Handle_id const handle { .value = p->handle };

			auto lookup_and_attach = [&] (Buffer_handle &bh) {

				if (!bh.mmap(_env)) {
					return;
				}
				p->addr_ptr = (uint64_t) bh.mmap_addr();
			};
			return _apply_buffer(handle, lookup_and_attach) && p->addr_ptr != 0 ? 0 : -1;
		}

		int _device_gem_mmap_gtt(void *arg)
		{
			auto      const p = reinterpret_cast<drm_i915_gem_mmap_gtt *>(arg);
			Handle_id const id { .value = p->handle };

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id.value,
				              " offset: ", Genode::Hex(p->offset));
			}

			/*
			 * We always map a buffer when the tiling is set. Since Mesa
			 * sets the filing first and maps the buffer afterwards we might
			 * already have a mapping at this point.
			 */
			p->offset = _map_buffer(id);

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id.value,
				              " offset: ", Genode::Hex(p->offset), " (mapped)");
			}
			return p->offset ? 0 : -1;
		}

		char const *_domain_name(uint32_t d)
		{
			if (d & I915_GEM_DOMAIN_CPU)         { return "CPU"; }
			if (d & I915_GEM_DOMAIN_GTT)         { return "CPU (GTT)"; }
			if (d & I915_GEM_DOMAIN_RENDER)      { return "GPU (RC)"; }
			if (d & I915_GEM_DOMAIN_VERTEX)      { return "GPU (VC)"; }
			if (d & I915_GEM_DOMAIN_INSTRUCTION) { return "GPU (IC)"; }
			if (d & I915_GEM_DOMAIN_SAMPLER)     { return "GPU (SC)"; }
			return "N/A";
		}

		int _device_gem_set_domain(void *arg)
		{
			/* XXX check read_domains/write_domain */
			auto      const p  = reinterpret_cast<drm_i915_gem_set_domain*>(arg);
			Handle_id const id { .value = p->handle };
			uint32_t  const rd = p->read_domains;
			uint32_t  const wd = p->write_domain;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ", "handle: ", id.value,
				              " rd: ", _domain_name(rd),
				              " wd: ", _domain_name(wd));
			}

			return 0;
		}

		int _device_getparam(void *arg)
		{
			drm_i915_getparam_t *p = reinterpret_cast<drm_i915_getparam_t*>(arg);
			int const param = p->param;
			int *value      = p->value;

			switch (param) {
			case I915_PARAM_CHIPSET_ID:
				*value = _gpu_info.chip_id;
				break;
			case I915_PARAM_HAS_CONTEXT_ISOLATION:
			case I915_PARAM_HAS_RELAXED_FENCING:
			case I915_PARAM_HAS_RELAXED_DELTA:
			case I915_PARAM_HAS_EXECBUF2:
			case I915_PARAM_HAS_LLC:
				*value = 1;
				break;
			case I915_PARAM_HAS_BSD:
			case I915_PARAM_HAS_BLT:
			case I915_PARAM_HAS_VEBOX:
			case I915_PARAM_HAS_WAIT_TIMEOUT:
			case I915_PARAM_HAS_RESOURCE_STREAMER:
			case 54 /* I915_PARAM_PERF_REVISION */:
				*value = 0;
				break;
			case I915_PARAM_REVISION:
				*value = _gpu_info.revision.value;
				return 0;
			case I915_PARAM_CS_TIMESTAMP_FREQUENCY:
				Genode::error("I915_PARAM_CS_TIMESTAMP_FREQUENCY not supported");
				return -1;
			case I915_PARAM_SLICE_MASK:
				*value = _gpu_info.slice_mask.value;
				return 0;
			case I915_PARAM_EU_TOTAL:
				*value = _gpu_info.eus.value;
				return 0;
			case I915_PARAM_SUBSLICE_TOTAL:
				*value = _gpu_info.subslices.value;
				return 0;
			case I915_PARAM_SUBSLICE_MASK:
				*value = _gpu_info.subslice_mask.value;
				return 0;
			case I915_PARAM_MMAP_GTT_VERSION:
				*value = 0; /* XXX */
				Genode::warning("I915_PARAM_MMAP_GTT_VERSION ", *value);
				return 0;
			default:
				Genode::error("Unhandled device param:", Genode::Hex(param));
				return -1;
				break;
			}
			return 0;
		}

		int _device_gem_context_create(void *arg)
		{
			static unsigned cnt = 0;

			drm_i915_gem_context_create * const p = reinterpret_cast<drm_i915_gem_context_create*>(arg);
			p->ctx_id = _gpu_info.ctx_id + cnt;
			Genode::error("create gem context ", p->ctx_id);
			cnt ++;
			return 0;
		}

		int _device_gem_context_set_param(void *arg)
		{
			auto * const p = reinterpret_cast<drm_i915_gem_context_param*>(arg);

			switch (p->param) {
			case I915_CONTEXT_PARAM_PRIORITY:
				Genode::error(__func__, " context=", p->ctx_id, " priority:=", p->value);
				return 0;
			case I915_CONTEXT_PARAM_RECOVERABLE:
				Genode::error(__func__, " context=", p->ctx_id, " recoverable:=", p->value);
				return 0;
			default:
				Genode::error(__func__, " ctx=", p->ctx_id, " param=", p->param, " size=", p->size, " value=", Genode::Hex(p->value));
				Genode::error(__func__, " unknown param=", p->param);
				return -1;
			};
		}

		int _device_gem_context_get_param(void *arg)
		{
			auto * const p = reinterpret_cast<drm_i915_gem_context_param*>(arg);

			switch (p->param) {
			case I915_CONTEXT_PARAM_SSEU:
			Genode::error("get ", p->ctx_id, " ", p->param, " ", p->size, " ", Genode::Hex(p->value));
				Genode::error(__func__, " sseu ");
				return 0;
			default:
				Genode::error(__func__, " ctx=", p->ctx_id, " param=", p->param, " size=", p->size, " value=", Genode::Hex(p->value));
				return -1;
			}
		}

		int _device_gem_set_tiling(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_i915_gem_set_tiling*>(arg);
			Handle_id const id { .value = p->handle };
			uint32_t  const mode    = p->tiling_mode;
			uint32_t  const stride  = p->stride;
			uint32_t  const swizzle = p->swizzle_mode;

			if (verbose_ioctl) {
				Genode::error(__func__, ": ",
				                "handle: ", id.value, " "
				                "mode: ", mode, " "
				                "stride: ", stride , " "
				                "swizzle: ", swizzle);
			}

			bool ok      = false;
			bool handled = _apply_buffer(id, [&] (Buffer_handle &bh) {
				if (!bh.cap.valid())
					return;

				/* we need a valid GGTT mapping for fencing */
				if (!bh.map_cap.valid() && !_map_buffer_ggtt(bh))
					return;

				uint32_t const m = (stride << 16) | (mode == 1 ? 1 : 0);

				Gpu::Buffer_id const id =
					Gpu::Buffer_id { .value = (uint32_t)bh.handle.id().value };
				ok = _set_tiling(id, m);
			});

			if (!handled)
				Genode::error(__func__, ": invalid handle: ", id.value);

			return ok ? 0 : -1;
		}

		int _device_gem_sw_finish(void *)
		{
			Genode::error(__func__, " called - unsupported");
			// drm_i915_gem_sw_finish * const p = reinterpret_cast<drm_i915_gem_sw_finish*>(arg);
			// Handle const handle              = p->handle;
			return 0;
		}

		int _device_gem_execbuffer2(void *arg)
		{
			auto const * const p = reinterpret_cast<drm_i915_gem_execbuffer2*>(arg);

			/* batch-buffer index and cap */
			unsigned const bb_id = (p->flags & I915_EXEC_BATCH_FIRST) ? 0 : p->buffer_count - 1;

			Buffer_handle *command_buffer = nullptr;

			if (verbose_ioctl) {
				uint64_t const ctx_id = p->rsvd1;
				Genode::log(__func__,
				            " buffers_ptr: ",        Genode::Hex(p->buffers_ptr),
				            " buffer_count: ",       p->buffer_count,
				            " batch_start_offset: ", Genode::Hex(p->batch_start_offset),
				            " batch_len: ",          p->batch_len,
				            " dr1: ",                Genode::Hex(p->DR1),
				            " dr4: ",                Genode::Hex(p->DR4),
				            " num_cliprects: ",      p->num_cliprects,
				            " cliprects_ptr: ",      Genode::Hex(p->cliprects_ptr),
				            " flags: ",              Genode::Hex(p->flags),
				            " ctx_id: ",             Genode::Hex(ctx_id));
			}

			if (!(p->flags & I915_EXEC_NO_RELOC)) {
				Genode::error("no relocation supported");
				return -1;
			}

			if (p->flags & I915_EXEC_FENCE_ARRAY) {
				bool unsupported = false;

				for (unsigned i = 0; i < p->num_cliprects; i++) {
					auto &fence = reinterpret_cast<drm_i915_gem_exec_fence *>(p->cliprects_ptr)[i];

					Sync_obj::Id const id { .value = fence.handle };
					_sync_objects.apply<Sync_obj>(id, [&](Sync_obj &) {
						/**
						 * skipping signal fences should be save as long as
						 * no one tries to wait for ...
						 * - fence.flags & I915_EXEC_FENCE_SIGNAL
						 */
						if (fence.flags & I915_EXEC_FENCE_WAIT)
							unsupported = true;
					});
				}

				if (unsupported) {
					Genode::error("fence wait not supported");
					return -1;
				}
			}

			auto const obj =
				reinterpret_cast<drm_i915_gem_exec_object2*>(p->buffers_ptr);

			for (uint64_t i = 0; i < p->buffer_count; i++) {
				if (verbose_ioctl) {
					Genode::log("  obj[", i, "] ",
					            "handle: ", obj[i].handle, " "
					            "relocation_count: ", obj[i].relocation_count, " "
					            "relocs_ptr: ", Genode::Hex(obj[i].relocs_ptr), " "
					            "alignment: ", Genode::Hex(obj[i].alignment), " "
					            "offset: ", Genode::Hex(obj[i].offset), " "
					            "flags: ", Genode::Hex(obj[i].flags));
				}

				if (obj[i].relocation_count > 0) {
					Genode::error("no relocation supported");
					return -1;
				}

				int             ret = -1;
				Handle_id const id  { .value = obj[i].handle };

				bool handled = _apply_buffer(id, [&](Buffer_handle &bh) {
					if (!bh.valid())
						return;

					if (bh.busy)
						Genode::warning("handle: ", obj[i].handle, " reused but is busy");

					if (bh.gpu_vaddr_valid && bh.gpu_vaddr.value != obj[i].offset) {
						Genode::error("unmap already mapped ", bh.handle, " ",
						              Genode::Hex(bh.gpu_vaddr.value), "->", Genode::Hex(obj[i].offset));
						_unmap_buffer_ppgtt(bh);
					}

					if (!bh.gpu_vaddr_valid)
						_map_buffer_ppgtt(bh, Gpu_virtual_address { .addr = obj[i].offset });

					if (!bh.gpu_vaddr_valid) {
						Genode::error("handle: ", obj[i].handle, " gpu_vaddr invalid");
						return;
					}

					bh.busy = true;

					if (i == bb_id)
						command_buffer = &bh;

					ret = 0;
				});

				if (!handled || ret) {
					Genode::error("handle: ", obj[i].handle, " invalid, ret=", ret);
					return ret;
				}
			}

			if (!command_buffer)
				return -1;

			Gpu::Request r = _initialize_request();
			r.operation.type = Gpu::Operation::Type::EXEC;
			r.operation.id   = Gpu::Buffer_id { .value = (uint32_t)command_buffer->handle.id().value };

			if (!_gpu_session.enqueue_request(r)) {
				Genode::error(__func__, ": could not enqueue_request");
				return -1;
			}

			bool success = _wait_for_request(r);
			if (success) {
				command_buffer->seqno = r.operation.seqno;
			}

			for (uint64_t i = 0; i < p->buffer_count; i++) {
				Handle_id const id { .value = obj[i].handle };
				_apply_buffer(id, [&](Buffer_handle &bh) {
					bh.seqno = command_buffer->seqno;
				});
			}

			/*
			 * Always wait for buffer to complete to avoid race between map and unmap
			 * of signal ep, the original drm_i915_gem_wait simply 0 now
			 */
			struct drm_i915_gem_wait wait = {
				.bo_handle = (__u32)command_buffer->handle.id().value,
				.flags = 0,
				.timeout_ns = -1LL
			};

			_device_gem_wait(&wait);
			return 0;
		}

		int _device_gem_busy(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_i915_gem_busy*>(arg);
			Handle_id const id { .value = p->handle };

			bool handled = _apply_buffer(id, [&](Buffer_handle const &bh) {
				p->busy = bh.busy;
			});

			return handled ? 0 : -1;
		}

		int _device_gem_madvise(void *arg)
		{
			drm_i915_gem_madvise * const p = reinterpret_cast<drm_i915_gem_madvise*>(arg);
			// Handle const handle = p->handle;
			// uint32_t const madv = p->madv;
			/* all buffer are always available */
			p->retained = 1;
			return 0;
		}

		int _device_gem_wait(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_i915_gem_wait*>(arg);
			Handle_id const id { .value = p->bo_handle };

			if (p->timeout_ns != -1LL) {
				Genode::error(__func__, " not supported ",
				              " handle:= ", p->bo_handle,
				              " timeout_ns:= ", Genode::Hex(p->timeout_ns));
				return -1;
			}

			bool success = false;
			bool handled = _apply_buffer(id, [&] (Buffer_handle &bh) {

				Gpu::Request r = _initialize_request();
				r.operation.type = Gpu::Operation::Type::WAIT;
				r.operation.seqno = bh.seqno;

				if (!_gpu_session.enqueue_request(r)) {
					Genode::error(__func__, ": could not enqueue_request");
					return;
				}

				success = _wait_for_request(r);
				if (success) {
					_for_each_buffer([&] (Buffer_handle &h) {
						if (!h.busy) return;
						if (h.seqno.value > bh.seqno.value) return;
						h.busy = false;

						/*
						 * Because bo object map/unmap is not supported correctly right now
						 * (reference counting), we unmap and map the buffers on for each frame
						 */
						_unmap_buffer_ppgtt(h);
					});
				}
			});

			return handled && success ? 0 : -1;
		}

		int _device_query(void *arg)
		{
			auto const query = reinterpret_cast<drm_i915_query*>(arg);

			if (query->num_items == 1) {
				auto const items = reinterpret_cast<drm_i915_query_item*>(query->items_ptr);
				Genode::error(__func__, " query_id=", items->query_id,
				              items->query_id == DRM_I915_QUERY_TOPOLOGY_INFO ? " query_topology_info" : "",
				              items->query_id == 3 /*DRM_I915_QUERY_PERF_CONFIG*/ ? " query_perf_config" : "");
			}

			Genode::error("device specific iocall DRM_I915_QUERY not supported"
			              " - num_items=", query->num_items);

			return -1;
		}

		int _device_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_I915_GEM_GET_APERTURE:   return _device_gem_get_aperture_size(arg);
			case DRM_I915_GETPARAM:           return _device_getparam(arg);
			case DRM_I915_GEM_CREATE:         return _device_gem_create(arg);
			case DRM_I915_GEM_MMAP:           return _device_gem_mmap(arg);
			case DRM_I915_GEM_MMAP_GTT:       return _device_gem_mmap_gtt(arg);
			case DRM_I915_GEM_SET_DOMAIN:     return _device_gem_set_domain(arg);
			case DRM_I915_GEM_CONTEXT_CREATE: return _device_gem_context_create(arg);
			case DRM_I915_GEM_SET_TILING:     return _device_gem_set_tiling(arg);
			case DRM_I915_GEM_SW_FINISH:      return _device_gem_sw_finish(arg);
			case DRM_I915_GEM_EXECBUFFER2:    return _device_gem_execbuffer2(arg);
			case DRM_I915_GEM_BUSY:           return _device_gem_busy(arg);
			case DRM_I915_GEM_MADVISE:        return _device_gem_madvise(arg);
			case DRM_I915_GEM_WAIT:           return 0; //_device_gem_wait(arg);
			case DRM_I915_QUERY:              return _device_query(arg);
			case DRM_I915_GEM_CONTEXT_SETPARAM: return _device_gem_context_set_param(arg);
			case DRM_I915_GEM_CONTEXT_GETPARAM: return _device_gem_context_get_param(arg);
			default:
				Genode::error("Unhandled device specific ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

		int _generic_gem_close(void *arg)
		{
			auto      const p  = reinterpret_cast<drm_gem_close*>(arg);
			Handle_id const id { .value = p->handle };
			return _free_buffer(id);
		}

		int _generic_gem_flink(void *arg)
		{
			auto const p = reinterpret_cast<drm_gem_flink*>(arg);
			p->name = prime_fd;
			return 0;
		}

		int _generic_syncobj_create(void *arg)
		{
			drm_syncobj_create * const p = reinterpret_cast<drm_syncobj_create *>(arg);
			if (p->flags) {
				Genode::error(__func__, " unsupported flags");
				errno = EINVAL;
				return -1;
			}
			auto * const obj = new (&_heap) Sync_obj(_sync_objects);
			p->handle = obj->id.id().value;
			return 0;
		}

		int _generic_syncobj_wait(void *arg)
		{
			auto &p = *reinterpret_cast<drm_syncobj_wait *>(arg);

			Genode::error(__func__, " ", p.count_handles, " ",
			              Genode::Hex(p.handles),
			              " tiemout_nsec=", p.timeout_nsec,
			              " flags=", p.flags);

			if (p.count_handles > 1) {
				Genode::error(__func__, " count handles > 1 - not supported");
				return -1;
			}

			uint32_t * handles = reinterpret_cast<uint32_t *>(p.handles);
			bool ok = false;

			try {
				Sync_obj::Id const id { .value = handles[0] };
				_sync_objects.apply<Sync_obj>(id, [&](Sync_obj &) {
					ok = true;
				});
			} catch (Sync_obj::Sync::Unknown_id) {
				errno = EINVAL;
				return -1;
			}

			if (ok) {
				errno = 62 /* ETIME */;
				return -1;
			} else
				Genode::error("unknown sync object handle ", handles[0]);

			return -1;
		}
		int _generic_syncobj_destroy(void *arg)
		{
			auto * const p = reinterpret_cast<drm_syncobj_destroy *>(arg);
			try {
				Sync_obj::Id const id { .value = p->handle };
				_sync_objects.apply<Sync_obj>(id, [&](Sync_obj &obj) {
					Genode::destroy(_heap, &obj);
				});
				return 0;
			} catch (Sync_obj::Sync::Unknown_id) {
				errno = EINVAL;
				return -1;
			}
		}

		int _generic_gem_open(void *arg)
		{
			auto const p = reinterpret_cast<drm_gem_open *>(arg);

			Genode::error("generic ioctl DRM_IOCTL_GEM_OPEN not supported ",
			              p->handle, " name=", Genode::Hex(p->name));

			return -1;
		}

		int _generic_get_cap(void *arg)
		{
			auto const p = reinterpret_cast<drm_get_cap *>(arg);

			if (p->capability == DRM_CAP_PRIME) {
				Genode::error("cap ", p->capability, " ", DRM_CAP_PRIME, " XXXXX");
				/* XXX fd == 43 check */
				p->value = DRM_PRIME_CAP_IMPORT;
				return 0;
			}

			Genode::error("generic ioctl DRM_IOCTL_GET_CAP not supported ",
			              p->capability);
			return -1;
		}

		int       const prime_fd     { 44 };
		Handle_id       prime_handle { };

		int _generic_prime_fd_to_handle(void *arg)
		{
			auto const p = reinterpret_cast<drm_prime_handle *>(arg);
			if (p->fd != prime_fd) {
				Genode::error("generic ioctl DRM_IOCTL_PRIME_FD_TO_HANDLE not supported ", __builtin_return_address(0), " ", p->fd);
				return -1;
			}
			p->handle = prime_handle.value;
			return 0;
		}

		int _generic_prime_handle_to_fd(void *arg)
		{
			auto const p = reinterpret_cast<drm_prime_handle *>(arg);

			Handle_id const handle { .value = p->handle };
			bool handled = _apply_buffer(handle, [&](Buffer_handle const &bh) {
				if (!prime_handle.value)
					prime_handle = handle;

				if (prime_handle.value != handle.value)
					Genode::error("prime handle changed - ignored ", bh.handle);
			});
			if (!handled)
				return -1;

			p->fd = prime_fd;
			return 0;
		}

		int _generic_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_NUMBER(DRM_IOCTL_GEM_CLOSE): return _generic_gem_close(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_FLINK): return _generic_gem_flink(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_CREATE): return _generic_syncobj_create(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_WAIT): return _generic_syncobj_wait(arg);
			case DRM_NUMBER(DRM_IOCTL_SYNCOBJ_DESTROY): return _generic_syncobj_destroy(arg);
			case DRM_NUMBER(DRM_IOCTL_GEM_OPEN): return _generic_gem_open(arg);
			case DRM_NUMBER(DRM_IOCTL_GET_CAP): return _generic_get_cap(arg);
			case DRM_NUMBER(DRM_IOCTL_PRIME_FD_TO_HANDLE):
				return _generic_prime_fd_to_handle(arg);
			case DRM_NUMBER(DRM_IOCTL_PRIME_HANDLE_TO_FD):
				return _generic_prime_handle_to_fd(arg);
			default:
				Genode::error("Unhandled generic DRM ioctl:", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

	public:

		Drm_call(Genode::Env &env, Genode::Entrypoint &signal_ep)
		:
			_env(env),
			_request_completion_sigh { signal_ep, *this, &Drm_call::_handle_request_completion }
		{
			/* make handle id 0 unavailable, handled as invalid by iris */
			drm_syncobj_create reserve_id_0 { };
			if (_generic_syncobj_create(&reserve_id_0))
				Genode::warning("syncobject 0 not reserved");

			_gpu_session.request_complete_sigh(_request_completion_sigh);
		}

		bool map_buffer_ggtt(Offset offset, size_t length)
		{
			bool result = false;

			_for_each_buffer([&] (Buffer_handle &h) {
				Genode::log("handle: ", h.handle.id().value, " map_offset: ", Genode::Hex(h.map_offset));
				if (h.map_offset != offset) { return; }
				if (length > h.size) { Genode::error("map_buffer_ggtt: size mismatch"); return; }
				result = true;
			});

			if (!result)
				Genode::error("could not lookup buffer for offset: ", offset);

			return result;
		}

		void unmap_buffer(void *addr, size_t length)
		{
			bool found = false;

			_for_each_buffer([&] (Buffer_handle &bh) {
				if (found || !bh.buffer_attached.constructed())
					return;

				if (reinterpret_cast<void *>(bh.mmap_addr()) != addr)
					return;

				if (bh.buffer_attached->size() != length) {
					Genode::warning(__func__, " size mismatch");
					Genode::sleep_forever();
					return;
				}

				bh.buffer_attached.destruct();
				found = true;
			});

			if (!found) {
				Genode::warning(__func__, " unknown region ",
				                addr, "+", Genode::Hex(length));
				Genode::sleep_forever();
			}
		}

		void unmap_buffer_ggtt(void *addr, size_t length)
		{
			Offset const offset = Offset(addr);

			bool handled = false;

			_for_each_buffer([&] (Buffer_handle &h) {
				if (handled) return;
				if (h.map_offset != offset) return;
				if (length > h.size) { Genode::error("unmap_buffer_ggtt: size mismatch"); return; }

				if (!h.map_cap.valid()) {
					Genode::error("no valid capability found for offset: ", Genode::Hex(offset));
					return;
				}

				_unmap_buffer_ggtt(h);
				handled = true;
			});

			if (!handled) {
				Genode::error(__func__, ": unknown addr ", addr, "+", Genode::Hex(length));
				Genode::sleep_forever();
			}
		}

		int ioctl(unsigned long request, void *arg)
		{
			bool const device = device_ioctl(request);
			return device ? _device_ioctl(device_number(request), arg)
			              : _generic_ioctl(command_number(request), arg);
		}
};


static Genode::Constructible<Drm_call> _call;


void drm_init(Genode::Env &env, Genode::Entrypoint &signal_ep)
{
	_call.construct(env, signal_ep);
}


/**
 * Mmap buffer object
 *
 * On Genode the virtual address of MMAP_GTT is stored in the offset.
 */
extern "C" void *drm_mmap(void * /* vaddr */, size_t length,
                          int /* prot */, int /* flags */,
                          int /* fd */, off_t offset)
{
	/* sanity check if we got a GTT mapped offset */
	bool const ok = _call->map_buffer_ggtt(offset, length);
	return ok ? (void *)offset : nullptr;
}

/**
 * Unmap buffer object
 */
extern "C" int drm_munmap(void *addr, size_t length)
{
	_call->unmap_buffer(addr, length);
	return 0;
}


extern "C" int genode_ioctl(int /* fd */, unsigned long request, void *arg)
{
	if (verbose_ioctl) { dump_ioctl(request); }
	int const ret = _call->ioctl(request, arg);
	if (verbose_ioctl) { Genode::log("returned ", ret); }
	return ret;
}

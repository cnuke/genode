/*
 * \brief  DRM ioctl backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
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
#include <base/debug.h>
#include <base/heap.h>
#include <base/log.h>
#include <gpu/connection.h>
#include <gpu_session/connection.h>
#include <util/string.h>

extern "C" {
#include <errno.h>
#include <fcntl.h>

#include <drm.h>
#include <etnaviv_drm.h>
#include <libdrm_macros.h>
}


enum { verbose_ioctl = false };


/**
 * Get DRM command number
 */
static unsigned long constexpr command_number(unsigned long request)
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
	long const cmd = command_number(request);
	return cmd >= DRM_COMMAND_BASE && cmd < DRM_COMMAND_END;
}


/**
 * Return name of DRM command
 */
const char *command_name(unsigned long request)
{
	if (IOCGROUP(request) != DRM_IOCTL_BASE)
		return "<non-DRM>";


	if (!device_ioctl(request)) {
		switch (command_number(request)) {
		case command_number(DRM_IOCTL_VERSION):            return "DRM_IOCTL_VERSION";
		case command_number(DRM_IOCTL_GEM_CLOSE):          return "DRM_IOCTL_GEM_CLOSE";
		case command_number(DRM_IOCTL_GEM_FLINK):          return "DRM_IOCTL_GEM_FLINK";
		case command_number(DRM_IOCTL_GEM_OPEN):           return "DRM_IOCTL_GEM_OPEN";
		case command_number(DRM_IOCTL_GET_CAP):            return "DRM_IOCTL_GET_CAP";
		case command_number(DRM_IOCTL_PRIME_HANDLE_TO_FD): return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
		case command_number(DRM_IOCTL_PRIME_FD_TO_HANDLE): return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
		default:                                  return "<unknown drm>";
		}
	}

	switch (device_number(request)) {
	case DRM_ETNAVIV_GET_PARAM:    return "DRM_ETNAVIV_GET_PARAM";
	case DRM_ETNAVIV_GEM_NEW:      return "DRM_ETNAVIV_GEM_NEW";
	case DRM_ETNAVIV_GEM_INFO:     return "DRM_ETNAVIV_GEM_INFO";
	case DRM_ETNAVIV_GEM_CPU_PREP: return "DRM_ETNAVIV_GEM_CPU_PREP";
	case DRM_ETNAVIV_GEM_CPU_FINI: return "DRM_ETNAVIV_GEM_CPU_FINI";
	case DRM_ETNAVIV_GEM_SUBMIT:   return "DRM_ETNAVIV_GEM_SUBMIT";
	case DRM_ETNAVIV_WAIT_FENCE:   return "DRM_ETNAVIV_WAIT_FENCE";
	case DRM_ETNAVIV_GEM_USERPTR:  return "DRM_ETNAVIV_GEM_USERPTR";
	case DRM_ETNAVIV_GEM_WAIT:     return "DRM_ETNAVIV_GEM_WAIT";
	case DRM_ETNAVIV_PM_QUERY_DOM: return "DRM_ETNAVIV_PM_QUERY_DOM";
	case DRM_ETNAVIV_PM_QUERY_SIG: return "DRM_ETNAVIV_PM_QUERY_SIG";
	case DRM_ETNAVIV_NUM_IOCTLS:   return "DRM_ETNAVIV_NUM_IOCTLS";
	default:
		return "<unknown driver>";
	}
}


/**
 * Check if request is OUT
 */
static bool constexpr req_out(unsigned long request)
{
	return (request & IOC_OUT);
}


/**
 * Check if request is IN
 */
static bool constexpr req_in(unsigned long request)
{
	return (request & IOC_IN);
}


/**
 * Convert FreeBSD (libc) I/O control to Linux (DRM driver)
 */
static unsigned long to_linux(unsigned long request)
{
	/*
	 * FreeBSD and Linux have swapped IN/OUT values.
	 */
	unsigned long lx = request & 0x0fffffffu;
	if (req_out(request)) { lx |= IOC_IN; }
	if (req_in (request)) { lx |= IOC_OUT; }

	return lx;
}


namespace Drm {

	size_t get_payload_size(drm_etnaviv_gem_submit const &submit);

	// XXX better implement as 'size_t for_each_object(T const *t, unsigned len, FN const &fn, char *dst)'
	template <typename FN, typename T> void for_each_object(T const *t, unsigned len, FN const &fn)
	{
		for (unsigned i = 0; i < len; i++) {
			T const *obj = &t[i];
			fn(obj);
		}
	}

	void serialize(drm_etnaviv_gem_submit *submit, char *content);

	size_t get_payload_size(drm_version const &version);
	void serialize(drm_version *version, char *content);
	void deserialize(drm_version *version, char *content);

} /* anonymous namespace */


size_t Drm::get_payload_size(drm_etnaviv_gem_submit const &submit)
{
	size_t size = 0;

	size += sizeof (drm_etnaviv_gem_submit_reloc) * submit.nr_relocs;
	size += sizeof (drm_etnaviv_gem_submit_bo) * submit.nr_bos;
	size += sizeof (drm_etnaviv_gem_submit_pmr) * submit.nr_pmrs;
	size += submit.stream_size;

	return size;
}


void Drm::serialize(drm_etnaviv_gem_submit *submit, char *content)
{
	size_t offset = 0;

	/* leave place for object itself first */
	offset += sizeof (*submit);

	/* next are the buffer-objects */
	if (submit->nr_bos) {
		size_t const new_start = offset;

		auto copy_bos = [&] (drm_etnaviv_gem_submit_bo const *bo) {
			char * const dst = content + offset;
			Genode::memcpy(dst, bo, sizeof (*bo));
			offset += sizeof (*bo);
		};
		for_each_object((drm_etnaviv_gem_submit_bo*)submit->bos,
		                submit->nr_bos, copy_bos);
		submit->bos = reinterpret_cast<__u64>(new_start);
	}

	/* next are the relocs */
	if (submit->nr_relocs) {
		size_t const new_start = offset;

		auto copy_relocs = [&] (drm_etnaviv_gem_submit_reloc const *reloc) {
			char * const dst = content + offset;
			Genode::memcpy(dst, reloc, sizeof (*reloc));
			offset += sizeof (*reloc);
		};
		for_each_object((drm_etnaviv_gem_submit_reloc*)submit->relocs,
		                submit->nr_relocs, copy_relocs);
		submit->relocs = reinterpret_cast<__u64>(new_start);
	}

	/* next are the pmrs */
	if (submit->nr_pmrs) {
		size_t const new_start = offset;
		auto copy_pmrs = [&] (drm_etnaviv_gem_submit_pmr const *pmr) {
			char * const dst = content + offset;
			Genode::memcpy(dst, pmr, sizeof (*pmr));
			offset += sizeof (*pmr);
		};
		for_each_object((drm_etnaviv_gem_submit_pmr*)submit->pmrs,
		                submit->nr_pmrs, copy_pmrs);
		submit->pmrs = reinterpret_cast<__u64>(new_start);
	}

	/* next is the cmd stream */
	{
		size_t const new_start = offset;

		char * const dst = content + offset;
		Genode::memcpy(dst, reinterpret_cast<void const*>(submit->stream), submit->stream_size);
		offset += submit->stream_size;
		submit->stream = reinterpret_cast<__u64>(new_start);
	}

	/* copy submit object last but into the front */
	Genode::memcpy(content, submit, sizeof (*submit));
}


size_t Drm::get_payload_size(drm_version const &version)
{
	size_t size = 0;
	size += version.name_len;
	size += version.date_len;
	size += version.desc_len;
	return size;
}


void Drm::serialize(drm_version *version, char *content)
{
	size_t offset = 0;
	char *start = 0;
	offset += sizeof (*version);

	start = (char*)offset;
	version->name = start;
	offset += version->name_len;

	start = (char*)offset;
	version->date = start;
	offset += version->date_len;

	start = (char*)offset;
	version->desc = start;
	offset += version->desc_len;

	Genode::memcpy(content, version, sizeof (*version));
}


void Drm::deserialize(drm_version *version, char *content)
{
	drm_version *cversion = reinterpret_cast<drm_version*>(content);

	version->version_major      = cversion->version_major;
	version->version_minor      = cversion->version_minor;
	version->version_patchlevel = cversion->version_patchlevel;

	version->name += (unsigned long)version;
	version->date += (unsigned long)version;
	version->desc += (unsigned long)version;

	cversion->name += (unsigned long)cversion;
	cversion->date += (unsigned long)cversion;
	cversion->desc += (unsigned long)cversion;

	Genode::copy_cstring(version->name, cversion->name, cversion->name_len);
	Genode::copy_cstring(version->date, cversion->date, cversion->date_len);
	Genode::copy_cstring(version->desc, cversion->desc, cversion->desc_len);
}


class Drm_call
{
	private:

		/*
		 * Noncopyable
		 */
		Drm_call(Drm_call const &) = delete;
		Drm_call &operator=(Drm_call const &) = delete;

		Genode::Env          &_env;
		Genode::Heap          _heap { _env.ram(), _env.rm() };

		/*****************
		 ** Gpu session **
		 *****************/

		Genode::Constructible<Gpu::Connection> _gpu_session { };
		Gpu::Info                              _gpu_info {
			0, 0, 0, 0, Gpu::Info::Execution_buffer_sequence { 0 } };

		/* apparently glmark2 submits araound 110 KiB at some point */
		enum { EXEC_BUFFER_SIZE = 256u << 10 };
		Genode::Dataspace_capability  _exec_buffer_cap   { };
		char                         *_local_exec_buffer { nullptr };

		Genode::Blockade                 _completion_blockade { };
		Genode::Signal_handler<Drm_call> _completion_sigh;

		void _handle_completion()
		{
			_completion_blockade.wakeup();
		}

		void _wait_for_completion(uint32_t fence)
		{
			do {
				Gpu::Info const info { _gpu_session->info() };
				uint32_t  const last = info.last_completed.id & 0xffffffffu;

				if (last >= fence) {
					break;
				}

				_completion_blockade.block();
			} while (true);
		}

		struct Buffer_handle;
		using Handle    = Genode::Id_space<Buffer_handle>::Element;
		using Handle_id = Genode::Id_space<Buffer_handle>::Id;

		struct Buffer_handle
		{
			struct Invalid_capability : Genode::Exception { };

			Genode::Dataspace_capability const cap;
			Genode::size_t               const size;
			Handle                       const handle;

			Gpu::Info::Execution_buffer_sequence seqno;

			Genode::Constructible<Genode::Attached_dataspace> _attached_buffer { };

			Buffer_handle(Genode::Id_space<Buffer_handle> &space,
			              Genode::Dataspace_capability cap,
			              Genode::uint32_t handle,
			              Genode::size_t size)
			:
				cap { cap }, size { size },
				handle { *this, space, Handle_id { .value = handle } },
				seqno { .id = 0 }
			{
				if (!cap.valid()) {
					throw Invalid_capability();
				}
			}

			~Buffer_handle() { }

			bool mmap(Genode::Env &env)
			{
				if (!_attached_buffer.constructed()) {
					_attached_buffer.construct(env.rm(), cap);
				}

				return _attached_buffer.constructed();
			}

			Genode::addr_t mmap_addr()
			{
				using addr_t = Genode::addr_t;
				return reinterpret_cast<addr_t>(_attached_buffer->local_addr<addr_t>());
			}
		};

		Genode::Id_space<Buffer_handle> _buffer_handles { };

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

		Genode::Dataspace_capability _lookup_cap_from_handle(uint32_t handle)
		{
			Handle_id const id { .value = handle };

			Genode::Dataspace_capability cap { };
			auto lookup_cap = [&] (Buffer_handle const &bh) {
				cap = bh.cap;
			};

			return _apply_buffer(id, lookup_cap) ? cap : Genode::Dataspace_capability();
		}

		/******************************
		 ** Device DRM I/O controls **
		 ******************************/

		int _drm_etnaviv_gem_cpu_fini(drm_etnaviv_gem_cpu_fini &arg)
		{
			Genode::Dataspace_capability const cap = _lookup_cap_from_handle(arg.handle);
			if (!cap.valid()) {
				return -1;
			}

			_gpu_session->unmap_buffer(cap);
			return 0;
		}

		int _drm_etnaviv_gem_cpu_prep(drm_etnaviv_gem_cpu_prep &arg)
		{
			Genode::Dataspace_capability const cap = _lookup_cap_from_handle(arg.handle);
			if (!cap.valid()) {
				return -1;
			}

			Gpu::Session::Mapping_type mt { Gpu::Session::Mapping_type::UNKNOWN };
			switch (arg.op) {
			case ETNA_PREP_READ:   mt = Gpu::Session::Mapping_type::READ; break;
			case ETNA_PREP_WRITE:  mt = Gpu::Session::Mapping_type::WRITE; break;
			case ETNA_PREP_NOSYNC: mt = Gpu::Session::Mapping_type::NOSYNC; break;
			default: break;
			}

			Genode::Dataspace_capability const map_cap =
				_gpu_session->map_buffer(cap, false, mt);
			return map_cap.valid() ? 0 : -1;
		}

		int _drm_etnaviv_gem_info(drm_etnaviv_gem_info &arg)
		{
			Handle_id const id { .value = arg.handle };

			auto lookup_and_attach = [&] (Buffer_handle &bh) {
				if (!bh.mmap(_env)) {
					return;
				}
				arg.offset = (uint64_t) bh.mmap_addr();
			};
			return _apply_buffer(id, lookup_and_attach) ? 0 : -1;
		}

		Genode::Dataspace_capability _alloc_buffer(Genode::size_t const size)
		{
			Genode::size_t donate = size;

			try {
				return Genode::retry<Gpu::Session::Out_of_ram>(
				[&] () { return _gpu_session->alloc_buffer(size); },
				[&] () {
					_gpu_session->upgrade_ram(donate);
					donate >>= 2;
				}, 8);
			} catch (Gpu::Session::Out_of_ram) { }

			return Genode::Dataspace_capability();
		}

		int _drm_etnaviv_gem_new(drm_etnaviv_gem_new &arg)
		{
			Genode::size_t const size = arg.size;

			Genode::Dataspace_capability cap = _alloc_buffer(size);
			if (!cap.valid()) {
				return -1;
			}

			Gpu::Handle const handle { _gpu_session->buffer_handle(cap) };
			if (!handle.valid()) {
				_gpu_session->free_buffer(cap);
				return -1;
			}

			try {
				Buffer_handle *buffer =
					new (&_heap) Buffer_handle(_buffer_handles, cap, handle.value, size);
				arg.handle = buffer->handle.id().value;
				return 0;
			} catch (...) {
				_gpu_session->free_buffer(cap);
			}
			return -1;
		}

		int _drm_etnaviv_gem_submit(drm_etnaviv_gem_submit &arg)
		{
			size_t const payload_size = Drm::get_payload_size(arg);
			if (payload_size > EXEC_BUFFER_SIZE) {
				Genode::error(__func__, ": exec buffer too small (",
				              (unsigned)EXEC_BUFFER_SIZE, ") needed ", payload_size);
				return -1;
			}

			/*
			 * Copy each array flat to the exec buffer and adjust the
			 * addresses in the submit object.
			 */
			Genode::memset(_local_exec_buffer, 0, EXEC_BUFFER_SIZE);
			Drm::serialize(&arg, _local_exec_buffer);

			try {
				uint64_t const pending_exec_buffer =
					_gpu_session->exec_buffer(_exec_buffer_cap, EXEC_BUFFER_SIZE).id;
				arg.fence = pending_exec_buffer & 0xffffffffu;
				return 0;
			} catch (Gpu::Session::Invalid_state) { }

			return -1;
		}

		int _drm_etnaviv_gem_wait(drm_etnaviv_gem_wait &)
		{
			Genode::warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_gem_userptr(drm_etnaviv_gem_userptr &)
		{
			Genode::warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_get_param(drm_etnaviv_param &arg)
		{
			if (arg.param > Gpu::Info::MAX_ETNAVIV_PARAMS) {
				errno = EINVAL;
				return -1;
			}

			arg.value = _gpu_info.etnaviv_param[arg.param];
			return 0;
		}

		int _drm_etnaviv_pm_query_dom(drm_etnaviv_pm_domain &)
		{
			Genode::warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_pm_query_sig(drm_etnaviv_pm_signal &)
		{
			Genode::warning(__func__, ": not implemented");
			return -1;
		}

		int _drm_etnaviv_wait_fence(drm_etnaviv_wait_fence &arg)
		{
			// XXX ignore timeout for now
			_wait_for_completion(arg.fence);
			return 0;
		}

		int _device_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case DRM_ETNAVIV_GEM_CPU_FINI:
				return _drm_etnaviv_gem_cpu_fini(*reinterpret_cast<drm_etnaviv_gem_cpu_fini*>(arg));
			case DRM_ETNAVIV_GEM_CPU_PREP:
				return _drm_etnaviv_gem_cpu_prep(*reinterpret_cast<drm_etnaviv_gem_cpu_prep*>(arg));
			case DRM_ETNAVIV_GEM_INFO:
				return _drm_etnaviv_gem_info(*reinterpret_cast<drm_etnaviv_gem_info*>(arg));
			case DRM_ETNAVIV_GEM_NEW:
				return _drm_etnaviv_gem_new(*reinterpret_cast<drm_etnaviv_gem_new*>(arg));
			case DRM_ETNAVIV_GEM_SUBMIT:
				return _drm_etnaviv_gem_submit(*reinterpret_cast<drm_etnaviv_gem_submit*>(arg));
			case DRM_ETNAVIV_GEM_USERPTR:
				return _drm_etnaviv_gem_userptr(*reinterpret_cast<drm_etnaviv_gem_userptr*>(arg));
			case DRM_ETNAVIV_GEM_WAIT:
				return _drm_etnaviv_gem_wait(*reinterpret_cast<drm_etnaviv_gem_wait*>(arg));
			case DRM_ETNAVIV_GET_PARAM:
				return _drm_etnaviv_get_param(*reinterpret_cast<drm_etnaviv_param*>(arg));
			case DRM_ETNAVIV_PM_QUERY_DOM:
				return _drm_etnaviv_pm_query_dom(*reinterpret_cast<drm_etnaviv_pm_domain*>(arg));
			case DRM_ETNAVIV_PM_QUERY_SIG:
				return _drm_etnaviv_pm_query_sig(*reinterpret_cast<drm_etnaviv_pm_signal*>(arg));
			case DRM_ETNAVIV_WAIT_FENCE:
				return _drm_etnaviv_wait_fence(*reinterpret_cast<drm_etnaviv_wait_fence*>(arg));
			default: break;
			}

			return 0;
		}

		/*******************************
		  ** Generic DRM I/O controls **
		 *******************************/

		int _drm_gem_close(drm_gem_close const &gem_close)
		{
			Handle_id const id { .value = gem_close.handle };

			bool const handled = _apply_buffer(id, [&] (Buffer_handle &bh) {
				_gpu_session->free_buffer(bh.cap);

				Genode::destroy(_heap, &bh);
			});

			return handled ? 0 : -1;
		}

		int _drm_version(drm_version &version)
		{
			// TODO make sure user ptr are properly accounted for
			version.version_major = 1;
			version.version_minor = 3;
			version.version_patchlevel = 0;

			return 0;
		}

		int _generic_ioctl(unsigned cmd, void *arg)
		{
			if (!arg) {
				errno = EINVAL;
				return -1;
			}

			switch (cmd) {
			case command_number(DRM_IOCTL_GEM_CLOSE):
				return _drm_gem_close(*reinterpret_cast<drm_gem_close*>(arg));
			case command_number(DRM_IOCTL_VERSION):
				return _drm_version(*reinterpret_cast<drm_version*>(arg));
			default:
				Genode::error("unhandled generic DRM ioctl: ", Genode::Hex(cmd));
				break;
			}

			return -1;
		}

		int _ioctl_gpu(unsigned long request, void *arg)
		{
			bool const device_request = device_ioctl(request);
			return device_request ? _device_ioctl(device_number(request), arg)
			                      : _generic_ioctl(command_number(request), arg);
		}

		/*****************
		 ** Drm session **
		 *****************/

		Genode::Allocator_avl _drm_alloc { &_heap };
		Genode::Constructible<Drm::Connection> _drm_session { };

		int _ioctl_drm(unsigned long request, void *arg)
		{
			size_t size = IOCPARM_LEN(request);

			bool const in  = req_in(request);
			bool const out = req_out(request);

			unsigned long const lx_request = to_linux(request);

			/*
			 * Adjust packet size for flatten arrays.
			 */
			if (command_number(request) == DRM_ETNAVIV_GEM_SUBMIT) {
				/* account for the arrays */
				drm_etnaviv_gem_submit *submit =
					reinterpret_cast<drm_etnaviv_gem_submit*>(arg);
				size_t const payload_size = Drm::get_payload_size(*submit);
				size += payload_size;
			} else

			/*
			 * Adjust packet size for user pointer storage.
			 */
			if (command_number(request) == command_number(DRM_IOCTL_VERSION)) {
				drm_version *version =
					reinterpret_cast<drm_version*>(arg);
				size_t const payload_size = Drm::get_payload_size(*version);
				size += payload_size;
			}

			Drm::Session::Tx::Source &src = *_drm_session->tx();
			Drm::Packet_descriptor p { src.alloc_packet(size), lx_request };

			/*
			 * Copy each array flat to the packet buffer and adjust the
			 * addresses in the submit object.
			 */
			if (device_number(request) == DRM_ETNAVIV_GEM_SUBMIT) {
				drm_etnaviv_gem_submit *submit =
					reinterpret_cast<drm_etnaviv_gem_submit*>(arg);
				char *content = src.packet_content(p);
				Drm::serialize(submit, content);
			} else

			/*
			 * Copy and adjust user pointer in DRM version object.
			 */
			if (command_number(request) == command_number(DRM_IOCTL_VERSION)) {
				drm_version *version =
					reinterpret_cast<drm_version*>(arg);
				char *content = src.packet_content(p);
				Drm::serialize(version, content);
			} else

			/*
			 * The remaining ioctls get the memcpy treament. Hopefully there
			 * are no user pointers left...
			 */
			if (in) {
				Genode::memcpy(src.packet_content(p), arg, size);
			}

			/*
			 * For the moment we perform a "blocking" packetstream operation
			 * which could be time-consuming but is easier to debug. Eventually
			 * it should be replace by a asynchronous operation.
			 */
			src.submit_packet(p);
			p = src.get_acked_packet();

			if (out && arg) {
				/*
				 * Adjust user pointers back to make the client happy.
				 */
				if (command_number(request) == command_number(DRM_IOCTL_VERSION)) {
					drm_version *version =
						reinterpret_cast<drm_version*>(arg);
						char *content = src.packet_content(p);
					Drm::deserialize(version, content);

				} else {
					// XXX handle unserializaton in a better way
					Genode::memcpy(arg, src.packet_content(p), size);
				}
			}

			src.release_packet(p);
			return p.error();
		}

	public:

		Drm_call(Genode::Env &env, Genode::Entrypoint &signal_ep, bool use_gpu_session)
		:
			_env { env },
			_completion_sigh { signal_ep, *this, &Drm_call::_handle_completion }
		{
			if (use_gpu_session) {
				_gpu_session.construct(_env);
				_gpu_info = _gpu_session->info();
				_exec_buffer_cap = _alloc_buffer(EXEC_BUFFER_SIZE);
				if (!_exec_buffer_cap.valid()) {
					throw Gpu::Session::Invalid_state();
				}
				try {
					_local_exec_buffer =
						(char*)_env.rm().attach(_exec_buffer_cap);
				} catch (...) {
					throw Gpu::Session::Invalid_state();
				}

				_gpu_session->completion_sigh(_completion_sigh);

			} else {
				_drm_session.construct(_env, &_drm_alloc, 1u<<20);
			}
		}

		~Drm_call()
		{
			if (_local_exec_buffer) {
				_env.rm().detach(_local_exec_buffer);
			}

			if (_exec_buffer_cap.valid()) {
				_gpu_session->free_buffer(_exec_buffer_cap);
			}
		}

		int ioctl(unsigned long request, void *arg)
		{
			return _gpu_session.constructed() ? _ioctl_gpu(request, arg)
			                                  : _ioctl_drm(request, arg);
		}

		void *mmap(unsigned long offset, unsigned long size)
		{

			Genode::Ram_dataspace_capability cap { };
			
			if (_gpu_session.constructed()) {
				/*
				 * Buffer should have been mapped during GEM INFO call.
				 */
				return (void*)offset;
			} else {
				cap = _drm_session->object_dataspace(offset, size);
			}

			if (!cap.valid()) {
				return (void *)-1;
			}

			try {
				return _env.rm().attach(cap);
			} catch (...) { }
			
			return (void *)-1;
		}

		void munmap(void *addr)
		{
			if (_gpu_session.constructed()) {
				return;
			}

			_env.rm().detach(addr);
		}
};


static Genode::Constructible<Drm_call> _drm;


void drm_init(Genode::Env &env, Genode::Entrypoint &ep, bool use_gpu_session)
{
	_drm.construct(env, ep, use_gpu_session);
}


void drm_complete()
{
	Genode::error(__func__, ": called, not implemented yet");
}


/**
 * Dump I/O control request to LOG
 */
static void dump_ioctl(unsigned long request)
{
	using namespace Genode;

	log("ioctl(request=", Hex(request),
	    (request & 0xe0000000u) == IOC_OUT   ? " out"   :
	    (request & 0xe0000000u) == IOC_IN    ? " in"    :
	    (request & 0xe0000000u) == IOC_INOUT ? " inout" : " void",
	    " len=", IOCPARM_LEN(request),
	    " cmd=", command_name(request), " (", Hex(command_number(request)), ")");
}


/**
 * Perfom I/O control request
 */
extern "C" int genode_ioctl(int /* fd */, unsigned long request, void *arg)
{
	if (verbose_ioctl)
		dump_ioctl(request);

	try {
		int ret = _drm->ioctl(request, arg);

		if (verbose_ioctl)
			Genode::log("returned ", ret);

		return ret;
	} catch (...) { }

	return -1;
}


/**
 * Map DRM buffer-object
 */
void *drm_mmap(void *addr, size_t length, int prot, int flags,
               int fd, off_t offset)
{
	(void)addr;
	(void)prot;
	(void)flags;
	(void)fd;

	return _drm->mmap(offset, length);
}


/**
 * Unmap DRM buffer-object
 */
int drm_munmap(void *addr, size_t length)
{
	(void)length;

	_drm->munmap(addr);
	return 0;
}

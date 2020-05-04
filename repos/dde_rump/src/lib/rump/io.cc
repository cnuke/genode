/**
 * \brief  Connect rump kernel to Genode's block interface
 * \author Sebastian Sumpf
 * \date   2013-12-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "sched.h"
#include <base/allocator_avl.h>
#include <rump/env.h>
#include <rump_fs/fs.h>

#include <vfs/simple_env.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>


static const bool verbose = false;

enum  { GENODE_FD = 64 };

/**
 * Block session connection
 */
class Backend
{
	private:

		Genode::Allocator              &_alloc      { Rump::env().heap() };
		Genode::Attached_rom_dataspace &_config_rom { Rump::env().config_rom() };
		Genode::Entrypoint             &_ep         { Rump::env().env().ep() };

		Genode::Lock _session_lock;

		Vfs::File_system        &_vfs;
		Vfs::Vfs_handle mutable *_handle { nullptr };

		bool _sync()
		{
			while (!_handle->fs().queue_sync(_handle)) {
				_ep.wait_and_dispatch_one_io_signal();
			}

			Vfs::File_io_service::Sync_result result;

			for (;;) {
				result = _handle->fs().complete_sync(_handle);

				if (result != Vfs::File_io_service::SYNC_QUEUED)
					break;

				_ep.wait_and_dispatch_one_io_signal();
			}

			if (result != Vfs::File_io_service::SYNC_OK) {
				return false;
			}

			return true;
		}

		struct At { Vfs::file_offset value; };

		bool _read(At at, char *dst, size_t bytes)
		{
			Vfs::file_size out_count = 0;

			_handle->seek(at.value);

			while (!_handle->fs().queue_read(_handle, bytes)) {
				_ep.wait_and_dispatch_one_io_signal();
			}

			Vfs::File_io_service::Read_result result;

			for (;;) {
				result = _handle->fs().complete_read(_handle, dst, bytes,
				                                     out_count);

				if (result != Vfs::File_io_service::READ_QUEUED)
					break;

				_ep.wait_and_dispatch_one_io_signal();
			}

			/*
			 * XXX handle READ_ERR_AGAIN, READ_ERR_WOULD_BLOCK, READ_QUEUED
			 *                                                   */

			if (result != Vfs::File_io_service::READ_OK) {
				return false;
			}

			if (out_count != bytes) {
				/* partial read */
				return false;
			}

			return true;
		}

		bool _write(At at, char const *src, size_t bytes)
		{
			Vfs::file_size out_count = 0;

			_handle->seek(at.value);

			using Write_result = Vfs::File_io_service::Write_result;

			Write_result result;

			try {
				result = _handle->fs().write(_handle, src, bytes, out_count);
			} catch (Vfs::File_io_service::Insufficient_buffer) {
				return false;
			}

			if (result != Write_result::WRITE_OK || out_count != bytes) {
				/* partial write */
				return false;
			}

			return true;
		}

		using Block_device = Genode::String<256>;
		Block_device _block_device { };

	public:

		Backend(Vfs::File_system &fs, char const *device)
		:
			_vfs(fs),
			_block_device(device)
		{
			if (!_block_device.valid()) {
				throw Genode::Exception();
			}

			using Open_result = Vfs::Directory_service::Open_result;

			Open_result res = _vfs.open(_block_device.string(),
			                            Vfs::Directory_service::OPEN_MODE_RDWR,
			                            &_handle, _alloc);

			if (res != Open_result::OPEN_OK) {
				throw Genode::Exception();
			}

		}

		bool writable() const { return true; }

		void sync()
		{
			Genode::Lock::Guard guard(_session_lock);
			_sync();
		}

		uint64_t size()
		{
			using Stat_result = Vfs::Directory_service::Stat_result;

			Vfs::Directory_service::Stat stat { };
			Stat_result stat_res = _vfs.stat(_block_device.string(), stat);
			return stat_res == Stat_result::STAT_OK ? stat.size : 0;
		}

		bool submit(int op, int64_t offset, size_t bytes, char *data)
		{
			Genode::Lock::Guard guard(_session_lock);

			bool const read = op & RUMPUSER_BIO_WRITE ? false : true;
			At at { .value = offset };

			bool const succeeded = read ? _read(at, data, bytes)
			                            : _write(at, data, bytes);

			/* sync request */
			if (op & RUMPUSER_BIO_SYNC) {
				_sync();
			}

			return succeeded;
		}
};


static Backend *_global_backend_ptr;


static Backend &backend()
{
	/* rely on rump_io_backend_init be called first */
	return *_global_backend_ptr;
}


int rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	if (Genode::strcmp(GENODE_BLOCK_SESSION, name))
		return ENXIO;

	if (type)
		*type = RUMPUSER_FT_BLK;

	if (size)
		*size = backend().size();

	return 0;
}


int rumpuser_open(const char *name, int mode, int *fdp)
{
	if (!(mode & RUMPUSER_OPEN_BIO || Genode::strcmp(GENODE_BLOCK_SESSION, name)))
		return ENXIO;

	/* check for writable */
	if ((mode & RUMPUSER_OPEN_ACCMODE) && !backend().writable())
		return EROFS;

	*fdp = GENODE_FD;
	return 0;
}


void rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off, 
                  rump_biodone_fn biodone, void *donearg)
{
	int nlocks;
	rumpkern_unsched(&nlocks, 0);

	/* data request */
	if (verbose)
		Genode::log("fd: ",   fd,   " "
		            "op: ",   op,   " "
		            "len: ",  dlen, " "
		            "off: ",  Genode::Hex((unsigned long)off), " "
		            "bio ",   donearg, " "
		            "sync: ", !!(op & RUMPUSER_BIO_SYNC));

	bool succeeded = backend().submit(op, off, dlen, (char*)data);

	rumpkern_sched(nlocks, 0);

	if (biodone)
		biodone(donearg, dlen, succeeded ? 0 : EIO);
}


void rump_io_backend_sync()
{
	(void)backend().sync();
}


/* constructors in rump_fs.lib.so */
extern "C" void rumpcompctor_RUMP_COMPONENT_KERN_SYSCALL(void);
extern "C" void rumpcompctor_RUMP_COMPONENT_SYSCALL(void);
extern "C" void rumpcompctor_RUMP__FACTION_VFS(void);
extern "C" void rumpcompctor_RUMP__FACTION_DEV(void);
extern "C" void rumpns_modctor_cd9660(void);
extern "C" void rumpns_modctor_dk_subr(void);
extern "C" void rumpns_modctor_ext2fs(void);
extern "C" void rumpns_modctor_ffs(void);
extern "C" void rumpns_modctor_msdos(void);
extern "C" void rumpns_modctor_wapbl(void);


void rump_io_backend_init(void *vfs, char const *block_device)
{
	/* call init/constructor functions of rump_fs.lib.so (order is important!) */
	rumpcompctor_RUMP_COMPONENT_KERN_SYSCALL();
	rumpns_modctor_wapbl();
	rumpcompctor_RUMP_COMPONENT_SYSCALL();
	rumpcompctor_RUMP__FACTION_VFS();
	rumpcompctor_RUMP__FACTION_DEV();
	rumpns_modctor_msdos();
	rumpns_modctor_ffs();
	rumpns_modctor_ext2fs();
	rumpns_modctor_dk_subr();
	rumpns_modctor_cd9660();

	/* create back end */
	static Backend b(*reinterpret_cast<Vfs::File_system*>(vfs), block_device);
	_global_backend_ptr = &b;
}


void rumpuser_dprintf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	char buf[128] { };
	Genode::String_console(buf, sizeof(buf)).vprintf(format, list);
	Genode::log(Genode::Cstring(buf));

	va_end(list);
}

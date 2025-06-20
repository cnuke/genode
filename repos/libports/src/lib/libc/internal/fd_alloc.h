/*
 * \brief  file descriptor allocator interface
 * \author Christian Prochaska 
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_PLUGIN__FD_ALLOC_H_
#define _LIBC_PLUGIN__FD_ALLOC_H_

/* Genode includes */
#include <base/mutex.h>
#include <base/log.h>
#include <os/path.h>
#include <base/allocator.h>
#include <base/id_space.h>
#include <util/bit_allocator.h>
#include <util/xml_generator.h>

/* libc includes */
#include <stdlib.h>
#include <string.h>

/* libc-internal includes */
#include <internal/plugin.h>

enum { MAX_NUM_FDS = 1024 };

namespace Vfs {
	struct Vfs_handle;
}

namespace Libc {

	/**
	 * Plugin-specific file-descriptor context
	 */
	struct Plugin_context { virtual ~Plugin_context() { } };

	enum { ANY_FD = -1 };

	struct File_descriptor;

	class File_descriptor_allocator;
}

struct Libc::File_descriptor
{
	Genode::Mutex mutex { };

	using Id_space = Genode::Id_space<File_descriptor>;
	Id_space::Element _elem;

	int const libc_fd = _elem.id().value;

	char const *fd_path = nullptr;  /* for 'fchdir', 'fstat' */

	Plugin         *plugin;
	Plugin_context *context;

	struct Slot_vfs_handle;
	struct Lio_slot
	{
		enum class State { FREE, PENDING, IN_PROGRESS, COMPLETE };

		const struct aiocb *iocb   = nullptr;

		Slot_vfs_handle *handle = nullptr;
		ssize_t          result = -1;
		int              error  = 0;
		State            state  = State::FREE;

		void free()
		{
			handle->slot = nullptr;
			handle       = nullptr;
			iocb         = nullptr;
			error        = 0;
			result       = -1;
			state        = State::FREE;
		}
	};

	static constexpr unsigned MAX_AIOCB_PER_FD = 64;
	Lio_slot _lio_slots[MAX_AIOCB_PER_FD] { };

	void for_each_lio_slot(Lio_slot::State state, auto const &fn)
	{
		for (unsigned i = 0; i < MAX_AIOCB_PER_FD; i++)
			if (_lio_slots[i].state == state)
				fn(_lio_slots[i]);
	}

	bool any_free_lio_slot(auto const &fn)
	{
		for (unsigned i = 0; i < MAX_AIOCB_PER_FD; i++)
			if (_lio_slots[i].state == Lio_slot::State::FREE) {
				fn(_lio_slots[i]);
				return true;
			}

		return false;
	}

	void apply_lio(struct aiocb const *iocb, auto const &fn)
	{
		for (unsigned i = 0; i < MAX_AIOCB_PER_FD; i++)
			if (iocb == _lio_slots[i].iocb)
				fn(_lio_slots[i]);
	}

	unsigned lio_list_completed = 0;
	unsigned lio_list_queued    = 0;

	struct Slot_vfs_handle
	{
		enum class State { INVALID, QUEUED, COMPLETE };
		Vfs::Vfs_handle *vfs_handle = nullptr;
		struct Lio_slot *slot       = nullptr;
		State            state      = State::INVALID;

		::size_t count  = 0;
		::off_t  offset = 0;

		void reset()
		{
			slot   = nullptr;
			count  = 0;
			offset = 0;
			state  = State::INVALID;
		}
	};
	static constexpr unsigned MAX_VFS_HANDLES_PER_FD = MAX_AIOCB_PER_FD;
	Slot_vfs_handle _slot_vfs_handles[MAX_VFS_HANDLES_PER_FD] { };

	void any_unused_slot_vfs_handle(auto const &fn)
	{
		for (unsigned i = 0; i < MAX_VFS_HANDLES_PER_FD; i++)
			if (_slot_vfs_handles[i].slot == nullptr) {
				fn(_slot_vfs_handles[i]);
				break;
			}
	}


	int  flags    = 0;  /* for 'fcntl' */
	bool cloexec  = 0;  /* for 'fcntl' */
	bool modified = false;

	File_descriptor(Id_space &id_space, Plugin &plugin, Plugin_context &context,
	                Id_space::Id id)
	: _elem(*this, id_space, id), plugin(&plugin), context(&context) { }

	void path(char const *newpath);
};


class Libc::File_descriptor_allocator
{
	private:

		Genode::Mutex _mutex;

		Genode::Allocator &_alloc;

		using Id_space = File_descriptor::Id_space;

		Id_space _id_space;

		using Id_bit_alloc = Genode::Bit_allocator<MAX_NUM_FDS>;

		Id_bit_alloc _id_allocator;

	public:

		/**
		 * Constructor
		 */
		File_descriptor_allocator(Genode::Allocator &_alloc);

		/**
		 * Allocate file descriptor
		 */
		File_descriptor *alloc(Plugin *plugin, Plugin_context *context, int libc_fd = -1);

		/**
		 * Release file descriptor
		 */
		void free(File_descriptor *fdo);

		/**
		 * Prevent the use of the specified file descriptor
		 */
		void preserve(int libc_fd);

		File_descriptor *find_by_libc_fd(int libc_fd);

		/**
		 * Return any file descriptor with close-on-execve flag set
		 *
		 * \return pointer to file descriptor, or
		 *         nullptr is no such file descriptor exists
		 */
		File_descriptor *any_cloexec_libc_fd();

		/**
		 * Update seek state of file descriptor with append flag set.
		 */
		void update_append_libc_fds();

		/**
		 * Return file-descriptor ID of any open file, or -1 if no file is
		 * open
		 */
		int any_open_fd();

		void generate_info(Genode::Xml_generator &);
};


struct Pretty_slot_printer
{
	using Lio_slot        = Libc::File_descriptor::Lio_slot;
	using Slot_vfs_handle = Libc::File_descriptor::Slot_vfs_handle;

	Lio_slot const &_slot;

	static char const *_slot_state(Lio_slot::State state)
	{
		switch (state) {
		case Lio_slot::State::FREE:        return "FREE";
		case Lio_slot::State::PENDING:     return "PENDING";
		case Lio_slot::State::IN_PROGRESS: return "IN_PROGRESS";
		case Lio_slot::State::COMPLETE:    return "COMPLETE";
		}
		/* never reached */
		return "UNKNOWN";
	}

	static char const *_handle_state(Slot_vfs_handle::State state)
	{
		switch (state) {
		case Slot_vfs_handle::State::INVALID:  return "INVALID";
		case Slot_vfs_handle::State::QUEUED:   return "QUEUED";
		case Slot_vfs_handle::State::COMPLETE: return "COMPLETE";
		}
		/* never reached */
		return "UNKNOWN";
	}

	static char const *_lio_opcode(int op)
	{
		switch (op) {
		case LIO_NOP:   return "NOP";
		case LIO_READ:  return "READ";
		case LIO_WRITE: return "WRITE";
		}
		/* never reached */
		return "UNKNOWN";
	}

	Pretty_slot_printer(Libc::File_descriptor::Lio_slot const &slot)
	: _slot { slot } { }

	void print(Genode::Output &out) const
	{
		Genode::print(out, _lio_opcode(_slot.iocb->aio_lio_opcode), ": ", " "
		              "offset: ", _slot.iocb->aio_offset, " nbytes: ", _slot.iocb->aio_nbytes, " "
		              "slot: ", _slot_state(_slot.state), " "
		              "handle: ", _handle_state(_slot.handle ? _slot.handle->state
		                                                     : Slot_vfs_handle::State::INVALID), " "
		              "error: ", _slot.error, " "
		              "result: ", _slot.result);
	}
};


#endif /* _LIBC_PLUGIN__FD_ALLOC_H_ */

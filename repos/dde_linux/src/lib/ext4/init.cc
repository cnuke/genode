/*
 * \brief  Linux Ext4
 * \author Josef Soentgen
 * \date   2016-04-28
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <os/server.h>
#include <file_system_session/file_system_session.h>

/* local includes */
#include <lx_emul.h>
#include <lx_private.h>

/* Lx_kit includes */
#include <lx_kit/scheduler.h>
#include <lx_kit/timer.h>
#include <lx_kit/internal/list.h>

#include <lx_emul/extern_c_begin.h>
# include <uapi/linux/stat.h>
#include <lx_emul/extern_c_end.h>


extern "C" void module_journal_init();
extern "C" void module_ext4_init_fs();


static Genode::Signal_transmitter *_sender;


struct workqueue_struct *system_wq;


struct file_system_type *Lx::fs_list[Lx::MAX_FS_LIST];


struct dentry *_root_dir;


static char const *type(File_system::Directory_entry::Type t)
{
	switch (t) {
	case File_system::Directory_entry::Type::TYPE_FILE:      return "f";
	case File_system::Directory_entry::Type::TYPE_DIRECTORY: return "d";
	case File_system::Directory_entry::Type::TYPE_SYMLINK:   return "s";
	}
	return "-";
}


static void run_linux(void *)
{
	system_wq = alloc_workqueue("system_wq", 0, 0);

	module_journal_init();
	module_ext4_init_fs();

	struct file_system_type *ext4_fs = Lx::fs_list[1];
	if (!ext4_fs) { PERR("BUG"); return ; }

	int const flags = MS_RDONLY; /* XXX check Block_backend */
	_root_dir = ext4_fs->mount(ext4_fs, flags, "blockdevice", "noatime");

	_sender->submit(1);


	struct inode *inode = _root_dir->d_inode;

	/*
	 * iterate root_dir
	 */
	{
		int err = inode->i_fop->open(inode, nullptr);
		if (!err) {
			struct file file;
			file.f_inode   = inode;
			file.f_version = inode->i_version;
			file.f_pos     = 0L;

			struct dir_context *ctx = kzalloc(sizeof(struct dir_context), 0);
			ctx->pos = file.f_pos;

			enum { MAX_BUFFER_LEN = 16384 };
			ctx->lx_buffer = kzalloc(MAX_BUFFER_LEN, 0);
			ctx->lx_max    = MAX_BUFFER_LEN;
			ctx->lx_count  = 0;
			ctx->lx_error  = 0;

			err = inode->i_fop->iterate(&file, ctx);
			if (err) {
				PERR("Could not iterate dir, err: %d", err);
				throw -1;
			}

			if (ctx->lx_error) {
				PERR("Iterating dir failed, err: %d", ctx->lx_error);
				throw -1;
			}

			using namespace File_system;

			Directory_entry *de = reinterpret_cast<Directory_entry*>(ctx->lx_buffer);
			size_t const entries = ctx->lx_count / sizeof(Directory_entry);
			for (size_t i = 0; i < entries; i++) {
				PLOG("%s '%s'", type(de[i].type), de[i].name);
			}

			file.f_pos = ctx->pos;

			kfree(ctx->lx_buffer);
			kfree(ctx);
		} else { PERR("Could not open root directory"); }
	}

	/*
	 * open file in root dir
	 */
	{
		char const *file_name = "UnixEditionZero.txt";
		struct dentry *dentry = kzalloc(sizeof(struct dentry), 0);
		dentry->d_name.name = file_name;
		dentry->d_name.len  = strlen(file_name); /* important */

		inode->i_op->lookup(inode, dentry, 0);
		if (dentry->d_inode) {
			struct inode *finode = dentry->d_inode;
			PINF("found inode: %p for file '%s'", finode, dentry->d_name.name);
			PINF("i_op: %p i_fop: %p", finode->i_op, finode->i_fop);

			/*
			 * Get file status
			 */
			File_system::Status stat;
			stat.inode = finode->i_ino;
			stat.size  = finode->i_size;

			if (S_ISDIR(finode->i_mode)) {
				stat.mode = File_system::Status::MODE_DIRECTORY;
			} else if (S_ISLNK(finode->i_mode)) {
				stat.mode = File_system::Status::MODE_SYMLINK;
			} else {
				stat.mode = File_system::Status::MODE_FILE;
			}

			PINF("stat inode: %lu size: %zu mode: %u", stat.inode, stat.size, stat.mode);

			/*
			 * Read file by directly using address_space ops
			 */
			loff_t file_offset = 0;
			while (file_offset < stat.size) {
				char buf[PAGE_SIZE+1];

				struct page *page = kzalloc(sizeof(struct page), 0);
				if (!page) {
					PERR("Could not allocate page struct");
					throw -1;
				}
				page->addr = kzalloc(PAGE_SIZE, 0);
				if (!page->addr) {
					PERR("Could not allocate page data");
					throw -1;
				}

				page->index = file_offset >> PAGE_CACHE_SHIFT;

				page->mapping = finode->i_mapping;
				int err = finode->i_mapping->a_ops->readpage(nullptr, page);
				if (err) {
					PERR("Could not read page, err: %d", err);
					throw -1;
				}

				if (PageUptodate(page)) {
					size_t const isize = i_size_read(inode);
					PINF("PageUptodate: %p addr: %p isize: %zu", page, page->addr, isize);
					memcpy(buf, page->addr, sizeof(buf)-1);
					buf[sizeof(buf)] = '\0';
					kfree(page->addr);

					file_offset += PAGE_SIZE;

					// PLOG("dump:\n %s", buf);
				} else {
					PERR("Reading page not successfull");
					throw -1;
				}

				kfree(page);
			}
			PINF("Reading finished");
		}

		kfree(dentry);
	}

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
	}
}


static Lx::Task linux_task(run_linux, nullptr, "linux",
                           Lx::Task::PRIORITY_0, Lx::scheduler());


/***************************
 ** Block session backend **
 ***************************/

static struct block_device _block_device;
struct block_device *Lx::block_device = &_block_device;


struct Block_backend
{
	enum {
		REQUEST_SIZE = 128 * 512,
		TX_BUFFER = Block::Session::TX_QUEUE_SIZE * REQUEST_SIZE,
	};

	typedef Block::Packet_descriptor Pd;

	Server::Entrypoint &ep { ep };
	Genode::Allocator  &alloc { alloc };

	Genode::Allocator_avl block_alloc { &alloc };

	Block::Connection block { &block_alloc, TX_BUFFER };

	Block::Session::Operations block_ops;
	Block::sector_t            block_count;
	Genode::size_t             block_size;

	Lx::Task &task;

	struct Request : Lx_kit::List<Request>::Element
	{
		Block::sector_t  number;
		Genode::size_t   count;
		char            *data;
		bool             read;

		Genode::off_t    offset; /* offset in packet stream */
		bool             pending;
		bool             success;
	};

	enum { MAX_REQUEST_NUM = Block::Session::TX_QUEUE_SIZE };
	Request requests[MAX_REQUEST_NUM];

	Request *alloc_request()
	{
		for (int i = 0; i < MAX_REQUEST_NUM; i++) {
			if (!requests[i].pending) {
				requests[i].pending = true;
				return &requests[i];
			}
		}
		return nullptr;
	}

	void reset_request(Request *r)
	{
		r->pending = false;
		r->offset  = -1;
	}

	Lx_kit::List<Request> pending_requests;

	Request *find_pending_request(Pd const &packet)
	{
		for (Request *r = pending_requests.first(); r; r = r->next()) {
			if (r->offset == packet.offset()) {
				return r;
			}
		}

		PERR("No pending request for packet %p found", &packet);
		return nullptr;
	}

	void handle_packets(unsigned)
	{
		while (block.tx()->ack_avail()) {
			Pd p       = block.tx()->get_acked_packet();
			Request *r = find_pending_request(p);

			if (r->read) {
				memcpy(r->data, block.tx()->packet_content(p),
				       p.block_count() * block_size);
			}

			r->success = p.succeeded();

			block.tx()->release_packet(p);
			pending_requests.remove(r);
			reset_request(r);
		}

		task.unblock();
		Lx::scheduler().schedule();
	}

	Genode::Signal_rpc_member<Block_backend> packet_dispatcher {
		ep, *this, &Block_backend::handle_packets };

	Block_backend(Server::Entrypoint &ep,
                 Genode::Allocator  &alloc,
                 Lx::Task           &task)
	: task(task)
	{
		block.tx_channel()->sigh_ack_avail(packet_dispatcher);
		block.tx_channel()->sigh_ready_to_submit(packet_dispatcher);

		block.info(&block_count, &block_size, &block_ops);

		PINF("block count: %llu size: %zu, read: %d write: %d",
		     block_count, block_size,
		     block_ops.supported(Block::Packet_descriptor::READ),
		     block_ops.supported(Block::Packet_descriptor::WRITE));
	}

	bool readable()  { return block_ops.supported(Pd::READ);  }
	bool writeable() { return block_ops.supported(Pd::WRITE); }

	bool read(Request *r)
	{
		try {
			Pd p(block.dma_alloc_packet(block_size*r->count),
			     Pd::READ, r->number, r->count);
			block.tx()->submit_packet(p);
			r->offset = p.offset();
			pending_requests.insert(r);
			return true;
		} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
			return false; }
	}
};


static Block_backend *_block_client;


bool block_init(Server::Entrypoint &ep, Genode::Allocator &alloc)
{
	static Block_backend inst(ep, alloc, linux_task);
	Lx::block_device->lx_block = &inst;

	Lx::block_device->bd_inode      = (struct inode*)kzalloc(sizeof(struct inode), 0);
	Lx::block_device->bd_block_size = (unsigned)inst.block_size;
	return true;
}


int Lx::read_block(struct super_block *s, sector_t nr, unsigned count, char *dst, unsigned len)
{
	Block_backend *bc = reinterpret_cast<Block_backend*>(s->s_bdev->lx_block);

	Block_backend::Request *r = bc->alloc_request();
	if (!r) { return -1;}

	/* convert between fs and bdev blocksize */
	r->number = nr * (s->s_blocksize / Lx::block_device->bd_block_size);
	r->count  = (count * s->s_blocksize) / Lx::block_device->bd_block_size;

	PDBG("nr: %llu count: %u r->nr: %llu r->count: %u", nr, count, r->number, r->count);

	r->data   = dst;
	r->read   = true;

	bool const queued = bc->read(r);
	if (!queued) {
		PERR("Could not queue read request for block %llu", nr);
		return -1;
	}

	/* wait until request was handled */
	Lx::scheduler().current()->block_and_schedule();
	return r->success ? 0 : -1;
}


/**********************************
 ** File system session frontend **
 **********************************/

unsigned long jiffies;
static struct task_struct _current;
struct task_struct *current = &_current;

bool fs_init(Server::Entrypoint &ep, Genode::Signal_transmitter &sender)
{
	_sender = &sender;

	Lx::scheduler();

	Lx::timer(&ep, &jiffies);

	/* kick-off the first round before returning */
	Lx::scheduler().schedule();

	return true;
}

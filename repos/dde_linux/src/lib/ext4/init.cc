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

/* Ext4 frontend includes */
#include <ext4/ext4.h>
#include <ext4/directory.h>
#include <ext4/file.h>


extern "C" void module_journal_init();
extern "C" void module_ext4_init_fs();


/*************
 ** Request **
 *************/

struct Request
{
	struct inode *inode;

	uint64_t      offset;
	size_t        data_len;
	void         *data;

	int (*func)(Request *);
	Ext4::Completion *completion;
};


static int request_read_directory(Request *request)
{
	struct inode *inode = request->inode;
	uint64_t     offset = request->offset;
	char           *dst = (char*)request->data;
	size_t          len = request->data_len;

	PDBG("inode: %p offset: %llu dst: %p len: %zu", inode, offset, dst, len);

	int err = inode->i_fop->open(inode, nullptr);
	if (err) {
		PERR("Could not open directory: %p", inode);
		return -1;
	}

	struct file file;
	::memset(&file, 0, sizeof(struct file));

	file.f_inode   = inode;
	file.f_version = inode->i_version;
	file.f_pos     = offset;

	struct dir_context *ctx = (struct dir_context*)kzalloc(sizeof(struct dir_context), 0);
	ctx->pos = file.f_pos;

	ctx->lx_buffer = dst;
	ctx->lx_max    = len;
	ctx->lx_count  = 0;
	ctx->lx_error  = 0;

	err = inode->i_fop->iterate(&file, ctx);
	if (err) {
		PERR("Could not iterate dir, err: %d", err);
		return -1;
	}

	if (ctx->lx_error) {
		PERR("Iterating dir failed, err: %d", ctx->lx_error);
		return -1;
	}

	int res = ctx->lx_count;
	kfree(ctx);
	return res;
}


static int request_open_file(Request *request)
{
	struct inode *inode = request->inode;
	char const    *name = (char const*)request->data;

	struct dentry *dentry = (struct dentry*)kzalloc(sizeof(struct dentry), 0);
	dentry->d_name.name = (unsigned char const*)name;
	dentry->d_name.len  = ::strlen(name); /* important */

	inode->i_op->lookup(inode, dentry, 0);
	if (!dentry->d_inode) {
		PERR("Could not look up '%s'", name);
		kfree(dentry);
	}

	return 666;
}


/* XXX move to linux_task ctx */
static struct dentry *_root_dir;
static Request current_request;
static Genode::Signal_transmitter *_fs_ready;


struct file_system_type *Lx::fs_list[Lx::MAX_FS_LIST];


static void run_linux(void *)
{
	/*
	 * Initialize lx emul env and create ext4fs instance
	 */
	system_wq = alloc_workqueue("system_wq", 0, 0);

	module_journal_init();
	module_ext4_init_fs();

	struct file_system_type *ext4_fs = Lx::fs_list[1];
	if (!ext4_fs) { PERR("BUG"); return ; }

	int const flags = MS_RDONLY; /* XXX check Block_backend */
	_root_dir = ext4_fs->mount(ext4_fs, flags, "blockdevice", (void*)"noatime");

	_fs_ready->submit(1);

	struct inode *inode = _root_dir->d_inode;

	/*
	 * open file in root dir
	 */
	if (0) {
		char const *file_name = "UnixEditionZero.txt";
		struct dentry *dentry = (struct dentry*)kzalloc(sizeof(struct dentry), 0);
		dentry->d_name.name = (unsigned char const*)file_name;
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

			PINF("stat inode: %lu size: %llu mode: %u", stat.inode, stat.size, stat.mode);

			/*
			 * Read file by directly using address_space ops
			 */
			loff_t file_offset = 0;
			while (file_offset < stat.size) {
				char buf[PAGE_SIZE+1];

				struct page *page = (struct page*)kzalloc(sizeof(struct page), 0);
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
		PDBG("Handle request");
		if (!current_request.inode) { continue; }
		PDBG("inode: %p", inode);
		int res = current_request.func(&current_request);
		if (res < 0) {
			PERR("error: %d", res);
			res = 0;
		}

		current_request.completion->complete(current_request.completion, res);
		current_request.inode = nullptr;
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

		PERR("handle_packets unblock");

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


static bool block_init(Server::Entrypoint &ep, Genode::Allocator &alloc)
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

	PDBG("nr: %llu count: %u r->nr: %llu r->count: %zu", nr, count, r->number, r->count);

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


/*******************************
 ** Ext4 file system frontend **
 *******************************/

File_system::Directory *Ext4::root_dir()
{
	static File_system::Directory inst(*Genode::env()->heap(), _root_dir);
	return &inst;
}


void Ext4::free_dentry(struct dentry *dentry)
{
	PERR("%s: not implented, leaking memory", __func__);
}


void Ext4::read_directory(Completion *completion, struct inode *inode,
                          uint64_t offset, char *dst, size_t len)
{
	current_request.inode      = inode;
	current_request.offset     = offset;
	current_request.data       = dst;
	current_request.data_len   = len;
	current_request.func       = request_read_directory;
	current_request.completion = completion;
}


void Ext4::open_file(Completion *completion, struct inode *inode,
                     char const *name, unsigned mode, bool create)
{
	current_request.inode      = inode;
	current_request.data       = (void*)name;
	current_request.func       = request_open_file;
	current_request.completion = completion;
}


void Ext4::schedule_task()
{
	linux_task.unblock();
	Lx::scheduler().schedule();
}


bool Ext4::init(Server::Entrypoint &ep,
                Allocator          &alloc,
                Signal_transmitter &sig_trans)
{
	if (!block_init(ep, alloc)) {
		PERR("Could not open block session");
		return false;
	}

	_fs_ready = &sig_trans;

	Lx::scheduler();
	Lx::timer(&ep, &jiffies);

	/* kick-off the first round before returning */
	Lx::scheduler().schedule();
	return true;
}

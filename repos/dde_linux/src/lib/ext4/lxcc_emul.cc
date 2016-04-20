/**
 * \brief  Linux emulation code
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
#include <base/env.h> /* XXX needed for impl/slab.h */
#include <base/object_pool.h>
#include <base/sleep.h>
#include <util/string.h>
#include <file_system_session/file_system_session.h>

/* Lx_kit includes */
#include <lx_kit/scheduler.h>

/* local includes */
#include <lx_emul.h>
#include <lx_private.h>

#if 1
#define NOTE \
	do { \
		PINF("%s not completely implemented from: %p", __func__, __builtin_return_address(0)); \
	} while (0)
#endif


/*********************************
 ** Lx::Backend_alloc interface **
 *********************************/

#include <lx_kit/backend_alloc.h>


struct Memory_object_base : Genode::Object_pool<Memory_object_base>::Entry
{
	Memory_object_base(Genode::Ram_dataspace_capability cap)
		: Genode::Object_pool<Memory_object_base>::Entry(cap) {}

	void free() { Genode::env()->ram_session()->free(ram_cap()); }

	Genode::Ram_dataspace_capability ram_cap()
	{
		using namespace Genode;
		return reinterpret_cap_cast<Ram_dataspace>(cap());
	}
};


static Genode::Object_pool<Memory_object_base> memory_pool;


Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached)
{
	using namespace Genode;

	Genode::Ram_dataspace_capability cap = env()->ram_session()->alloc(size);
	Memory_object_base *o = new (env()->heap()) Memory_object_base(cap);

	memory_pool.insert(o);
	return cap;
}


void Lx::backend_free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;

	Memory_object_base *object;
	memory_pool.apply(cap, [&] (Memory_object_base *o) {
		if (!o) { return; }
		o->free();
		memory_pool.remove(o);
		object = o; /* save for destroy */
	});
	destroy(env()->heap(), object);
}


/********************
 ** linux/string.h **
 ********************/

char *kstrdup(const char *s, gfp_t gfp)
{
	size_t len = strlen(s);
	char *p = (char*)kmalloc(len + 1, gfp);
	if (!p) { return nullptr; }

	p[len] = '\0';
	return p;
}


int memcmp(const void *p0, const void *p1, size_t size) {
	return Genode::memcmp(p0, p1, size); }


void *memcpy(void *dst, const void *src, size_t n)
{
	Genode::memcpy(dst, src, n);
	return dst;
}


void *memmove(void *dst, const void *src, size_t n)
{
	Genode::memmove(dst, src, n);
	return dst;
}


void *memset(void *s, int c, size_t n)
{
	Genode::memset(s, c, n);
	return s;
}


#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

/* verbatim copy from Linux */
static int bitmap_weight(const unsigned long *bitmap, unsigned int bits)
{
	unsigned int k, lim = bits/BITS_PER_LONG;
	int w = 0;

	for (k = 0; k < lim; k++)
		w += hweight_long(bitmap[k]);

	if (bits % BITS_PER_LONG)
		w += hweight_long(bitmap[k] & BITMAP_LAST_WORD_MASK(bits));

	return w;
}


/* verbatim copy from Linux */
size_t memweight(const void *ptr, size_t bytes)
{
	size_t ret = 0;
	size_t longs;
	const unsigned char *bitmap = reinterpret_cast<const unsigned char*>(ptr);

	for (; bytes > 0 && ((unsigned long)bitmap) % sizeof(long);
		 bytes--, bitmap++)
		ret += hweight8(*bitmap);

	longs = bytes / sizeof(long);
	if (longs) {
		BUG_ON(longs >= INT_MAX / BITS_PER_LONG);
		ret += bitmap_weight((unsigned long *)bitmap,
		                     longs * BITS_PER_LONG);
		bytes -= longs * sizeof(long);
		bitmap += longs * sizeof(long);
	}
	/*
	 * The reason that this last loop is distinct from the preceding
	 * bitmap_weight() call is to compute 1-bits in the last region smaller
	 * than sizeof(long) properly on big-endian systems.
	 */
	for (; bytes > 0; bytes--, bitmap++)
		ret += hweight8(*bitmap);

	return ret;
}


char *strchr(const char *p, int ch)
{
	char c;
	c = ch;
	for (;; ++p) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		Genode::memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}


size_t strlen(const char *s) {
	return Genode::strlen(s); }


int strncmp(const char *s1, const char *s2, size_t len) {
	return Genode::strcmp(s1, s2, len); }


char *strncpy(char *dst, const char *src, size_t count) {
	return Genode::strncpy(dst, src, count); }


char *strnchr(const char *p, size_t count, int ch)
{
	char c;
	c = ch;
	for (; count; ++p, count--) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


char *strreplace(char *s, char o, char n)
{
	for (; *s; ++s) { if (*s == o) { *s = n; } }
	return s;
}

/* taken from Linux */
char *strpbrk(const char *cs, const char *ct)
{
	const char *sc1, *sc2;

	for (sc1 = cs; *sc1 != '\0'; ++sc1) {
		for (sc2 = ct; *sc2 != '\0'; ++sc2) {
			if (*sc1 == *sc2)
				return (char *)sc1;
		}   
	}   
	return NULL;
}


/* taken from Linux */
char *strsep(char **s, const char *ct)
{
	return nullptr; /* XXX later */

	char *sbegin = *s;
	char *end;

	if (sbegin == NULL)
		return NULL;

	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;
	return sbegin;
}


/****************************
 ** asm-generic/atomic64.h **
 ****************************/

/**
 * This is not atomic on 32bit systems but this is not a problem
 * because we will not be preempted.
 */
void atomic64_add(long long i, atomic64_t *p) {
	p->counter += i; }


void atomic64_sub(long long i, atomic64_t *p) {
	p->counter -= i; }


void atomic64_set(atomic64_t *v, long long i) {
	v->counter = i; }

/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
                            unsigned long offset)
{
	unsigned long i  = offset / BITS_PER_LONG;
	offset -= (i * BITS_PER_LONG);

	for (; offset < size; offset++)
		if (addr[i] & (1UL << offset))
			return offset;

	return size;
}


unsigned long find_next_zero_bit(unsigned long const *addr, unsigned long size,
                                 unsigned long offset)
{
	unsigned long i, j;

	for (i = offset; i < (size / BITS_PER_LONG); i++) 
		if (addr[i] != ~0UL)
			break;

	if (i == size)
		return size;

	for (j = 0; j < BITS_PER_LONG; j++)
		if ((~addr[i]) & (1UL << j))
			break;

	return (i * BITS_PER_LONG) + j;
}


/******************
 ** linux/log2.h **
 ******************/

int ilog2(u32 n) { return Genode::log2(n); }


/**********************
 ** Memory allocation *
 **********************/

#include <lx_emul/impl/slab.h>


void kvfree(const void *p)
{
	kfree(p);
}


void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags)
{
	void *addr = kmem_cache_alloc(k, flags | __GFP_ZERO);
	if (addr) { memset(addr, 0, k->size()); }
	return addr;
}


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size) {
	return kmalloc(size, 0); }


void vfree(const void *addr) {
	kfree(addr); }


/********************
 ** linux/kernel.h **
 ********************/

#include <lx_emul/impl/kernel.h>

int strict_strtoul(const char *s, unsigned int base, unsigned long *res)
{
	unsigned long r = -EINVAL;
	Genode::ascii_to_unsigned(s, r, base);
	*res = r;

	return r;
}


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align) {
	return kmalloc(size, 0); }


/***********************
 ** linux/workqueue.h **
 ***********************/

#include <lx_emul/impl/work.h>


struct workqueue_struct *create_singlethread_workqueue(char const *name)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(Genode::env()->heap(), name);
	wq->task       = (void *)work;

	return wq;
}


struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...)
{
	return create_singlethread_workqueue(fmt);
}


/******************
 ** linux/wait.h **
 ******************/

#include <lx_emul/impl/completion.h>

long __wait_completion(struct completion *work, unsigned long timeout) {
	return timeout ? 1 : 0; }

#include <lx_emul/impl/wait.h>


/****************
 ** linux/fs.h **
 ****************/

static char const *block_device_name;


const char *__bdevname(dev_t, char *buffer)
{
	strncpy(buffer, block_device_name, strlen(block_device_name));
	return buffer;
}


const char *bdevname(struct block_device *bdev, char *buffer) {
	return __bdevname(0, buffer); }


int bdev_read_only(struct block_device *bdev)
{
	PWRN("force RO for now");
	return 1;
}


bool dir_emit(struct dir_context *ctx,
              const char *name, int namelen,
              u64 ino, unsigned type)
{
	using namespace File_system;

	ctx->lx_error = -EINVAL;
	if ((ctx->lx_max - ctx->lx_count) < sizeof(Directory_entry)) {
		return false;
	}

	Directory_entry *de = reinterpret_cast<Directory_entry*>(ctx->lx_buffer + ctx->lx_count);
	namelen += 1; /* NUL */
	if (namelen > sizeof(de->name)) {
		PWRN("Truncation of entry '%s' to %zu bytes", sizeof(de->name));
		namelen = sizeof(de->name);
	}

	strncpy(de->name, name, namelen);
	switch (type) {
	case DT_DIR: de->type = Directory_entry::TYPE_DIRECTORY; break;
	case DT_LNK: de->type = Directory_entry::TYPE_SYMLINK;   break;
	default:     de->type = Directory_entry::TYPE_FILE;      break;
	}

	ctx->lx_count += sizeof(Directory_entry);
	ctx->lx_error = 0;
	return true;
}


struct dentry *mount_bdev(struct file_system_type *fs_type, int flags,
                          const char *dev_name, void *data,
                          int (*fill_super)(struct super_block *, void *, int))
{
	struct super_block *s = (struct super_block*) kzalloc(sizeof(struct super_block), 0);
	if (!s) {
		PERR("Could not allocate super_block");
		return nullptr;
	}

	/* must be set before executine fill_super() */
	Lx::block_device->bd_holder = fs_type;
	Lx::block_device->bd_super  = s;

	s->s_bdev  = Lx::block_device;
	s->s_flags = flags;

	sb_set_blocksize(s, Lx::block_device->bd_block_size);

	strlcpy(s->s_id, dev_name, sizeof(s->s_id));
	block_device_name = dev_name;

	int const err = fill_super(s, data, 0 /* slient */);
	if (err) {
		PERR("Could not fill super block");
		return nullptr;
	}

	return s->s_root;
}


int register_filesystem(struct file_system_type * fs)
{
	int i;
	for (i = 0; i < Lx::MAX_FS_LIST; i++) {
		if (!Lx::fs_list[i]) { break; }

		if (Genode::strcmp(Lx::fs_list[i]->name, fs->name) == 0) {
			PWRN("File system %s already registered", fs->name);
			return -1;
		}
	}

	if (i == Lx::MAX_FS_LIST) {
		PERR("No space left to register file system %s", fs->name);
		return 1;
	}

	Lx::fs_list[i] = fs;
	PINF("Register file system %s", fs->name);
	return 0;
}


int sb_min_blocksize(struct super_block *s, int size)
{
	unsigned bsize = s->s_bdev->bd_block_size;
	if (bsize > size) { size = bsize; }
	return sb_set_blocksize(s, size);
}


int sb_set_blocksize(struct super_block *s, int size)
{
	s->s_blocksize      = size;
	s->s_blocksize_bits = blksize_bits(size);
	return s->s_blocksize;
}


struct inode *iget_locked(struct super_block *sb, unsigned long ino)
{
	/* from alloc_inode() */
	struct inode *inode = sb->s_op->alloc_inode(sb);
	if (!inode) { return nullptr; }
	inode->i_ino   = ino;
	inode->i_state = I_NEW;

	/* from inode_init_always() */
	struct address_space *const mapping = &inode->i_data;
	mapping->host = inode;
	inode->i_sb      = sb;
	inode->i_blkbits = sb->s_blocksize_bits;
	inode->i_mapping = mapping;
	atomic_set(&inode->i_count, 1);

	return inode;
}


void inode_init_once(struct inode *inode)
{
	NOTE;
	memset(inode, 0, sizeof(*inode));
}


void inode_set_flags(struct inode *inode, unsigned int flags, unsigned int mask)
{
	unsigned int old_flags = inode->i_flags;
	unsigned int new_flags = (old_flags & ~mask) | flags;
	inode->i_flags = new_flags;
}


void set_nlink(struct inode *inode, unsigned int nlink)
{
	inode->__i_nlink = nlink;
}


void unlock_new_inode(struct inode *inode)
{
	inode->i_state &= ~I_NEW;
}


struct inode *new_inode(struct super_block *s)
{
	struct inode *inode = s->s_op->alloc_inode(s);
	if (!inode) { return nullptr; }

	inode->i_state = 0;

	return inode;
}


struct dentry * d_splice_alias(struct inode *inode, struct dentry *dentry)
{
	if (!inode) { PWRN("inode is zero"); }

	/* just do d_add() dance and hope for the best */
	dentry->d_inode = inode;
	return NULL;
}


/*************************
 ** asm-generic/div64.h **
 *************************/

uint32_t __do_div(uint64_t *n, uint32_t base)
{
	uint32_t remainder = *n % base;
	*n = *n / base;
	return remainder;
}


/********************
 ** linux/random.h **
 ********************/

void get_random_bytes(void *buf, int nbytes)
{
	char *b = (char *)buf;

	/* FIXME not random */
	int i;
	for (i = 0; i < nbytes; i++) {
		b[i] = i + 1;
	}
}


/*************************
 ** linux/buffer_head.h **
 *************************/

struct buffer_head *sb_bread_unmovable(struct super_block *sb, sector_t block)
{
	char *data = (char*)kzalloc(sb->s_blocksize, 0);
	int const err = Lx::read_block(sb, block, 1, data, sb->s_blocksize);
	if (err) {
		kfree(data);
		return nullptr;
	}

	struct buffer_head *bh = (struct buffer_head *)kzalloc(sizeof(struct buffer_head), 0);
	bh->b_data = data;
	bh->b_size = sb->s_blocksize;

	atomic_set(&bh->b_count, 1);

	/* mark buffer as fresh, i.e. not from cache */
	set_buffer_uptodate(bh);

	return bh;
}


struct buffer_head *sb_getblk(struct super_block *sb, sector_t block)
{
	NOTE;
	return sb_bread_unmovable(sb, block);
}


struct buffer_head *getblk_unmovable(struct block_device *bdev, sector_t block, unsigned size)
{
	char *data = (char*)kzalloc(size, 0);
	int const err = Lx::read_block(bdev->bd_super, block, 1, data, size);
	if (err) {
		kfree(data);
		return nullptr;
	}

	struct buffer_head *bh = (struct buffer_head *)kzalloc(sizeof(struct buffer_head), 0);
	bh->b_data = data;
	atomic_set(&bh->b_count, 1);

	/* mark buffer as fresh, i.e. not from cache */
	set_buffer_uptodate(bh);

	return bh;
}


void __brelse(struct buffer_head *bh)
{
	if (atomic_read(&bh->b_count)) {
		put_bh(bh);
	}
}


void brelse(struct buffer_head *bh) {
	if (bh) { __brelse(bh); } }


void put_bh(struct buffer_head *bh)
{
	smp_mb__before_atomic();
	atomic_dec(&bh->b_count);

	/* XXX move to better place */
	if (!atomic_read(&bh->b_count)) {
		PWRN("freeing bh: %p", bh);
		kfree(bh->b_data);
		kfree(bh);
	}
}


void ll_rw_block(int rw, int nr, struct buffer_head * bh[])
{
	/*
	 * This function drops all READ requests when bh is uptodate,
	 * i.e., the block was read freshly from the block device. We
	 * assume that this is always true in and therefore do nothing
	 * in this function.
	 */

	NOTE;
}


void wait_on_buffer(struct buffer_head *bh)
{
	NOTE;
}


/*****************
 ** linux/bio.h **
 *****************/

int bio_add_page(struct bio *bio, struct page *page, unsigned int len,
                 unsigned int offset)
{
	if (bio->bi_vcnt >= bio->bi_max_vecs) { return 0; }

	if (offset != 0) {
		PWRN("bio: %p with page: %p offset: %u from: %p",
		     bio, page, offset, __builtin_return_address(0));
	}
	PERR("bio: %p page: %p offset: %u len: %u",
	      bio, page, offset, len);

	bio->bi_io_vec[bio->bi_vcnt].bv_page   = page;
	bio->bi_io_vec[bio->bi_vcnt].bv_len    = len;
	bio->bi_io_vec[bio->bi_vcnt].bv_offset = offset;

	bio->bi_vcnt++;

	/* not sure if we need this */
	bio->bi_iter.bi_size += len;

	return len;
}


struct bio *bio_alloc(gfp_t gfp_mask, unsigned int nr_iovecs)
{
	size_t const size = sizeof(struct bio) +
	                    sizeof(struct bio_vec) * nr_iovecs;

	struct bio *bio = (struct bio*)kzalloc(size, 0);
	if (!bio) { return nullptr; }

	/* point bio_io_vec to inline allocated memory */
	bio->bi_io_vec   = (struct bio_vec*) ((char*)bio + sizeof(struct bio));
	bio->bi_vcnt     = 0;
	bio->bi_max_vecs = nr_iovecs;

	return bio;
}


void bio_put(struct bio *bio)
{
	NOTE;

	kfree(bio);
}


blk_qc_t submit_bio(int rw, struct bio *bio)
{
	if (rw != READ) {
		PERR("rw: %d currently not implemented", rw);
		return 1;
	}

	if (bio->bi_vcnt != 1) {
		PERR("bi_vcnt: %u too large", bio->bi_vcnt);
		Genode::sleep_forever();
	}

	struct block_device * const bdev = bio->bi_bdev;
	struct super_block  * const sb   = bdev->bd_super;
	/* bio uses block number in size of s_blocksize */
	sector_t const block             = bio->bi_iter.bi_sector / 2;
	unsigned const size              = bio->bi_iter.bi_size;
	struct page * const page         = bio->bi_io_vec[0].bv_page;
	char * const data                = page->addr;
	unsigned const count             = size / sb->s_blocksize;

	PERR("bio: %p bdev: %p block: %llu size: %u data: %p count: %u",
	     bio, bdev, block, size, data, count);

	int const err = Lx::read_block(bdev->bd_super, block, count, data, size);
	if (err) {
		PERR("Could not read block");
		/* needed in bi_end_io() */
		bio->bi_error = 1;
	}

	bio->bi_end_io(bio);

	return 0;
}


/*********************
 ** linux/kthread.h **
 *********************/

void *kthread_run(int (*fn)(void *), void *arg, const char *n, ...)
{
	Lx::Task *t = new (Genode::env()->heap())
		Lx::Task((void (*)(void *))fn, arg, n, Lx::Task::PRIORITY_2,
	             Lx::scheduler());
	return t;
}


/*******************
 ** linux/sched.h **
 *******************/

void schedule(void)
{
	Lx::scheduler().current()->block_and_schedule();
}


/*****************
 ** linux/gfp.h **
 *****************/

/* no struct page shenanigans, its only used once as simple buffer */
unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	void *addr = kzalloc(4096, 0);
	return (unsigned long)addr;
}


/********************
 ** linux/dcache.h **
 ********************/

struct dentry *d_make_root(struct inode *root_inode)
{
	struct dentry *res = nullptr;

	res = (struct dentry*) kzalloc(sizeof(struct dentry), 0);
	if (!res) { return nullptr; }

	res->d_sb     = root_inode->i_sb;
	res->d_parent = res;

	res->d_inode = root_inode;

	res->d_name.name = "/";
	res->d_name.len  = 1;

	return res;
}


/*********************
 ** linux/highmem.h **
 *********************/

void zero_user_segment(struct page *page, unsigned start, unsigned end)
{
	if (end > PAGE_SIZE) {
		PERR("end: %u larger than PAGE_SIZE: %u", end, PAGE_SIZE);
		Genode::sleep_forever();
	}

	memset(page->addr + start, 0, end);
}

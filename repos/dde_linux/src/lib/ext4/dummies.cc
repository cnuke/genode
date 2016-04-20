/**
 * \brief  Dummy functions
 * \author Josef Soentgen
 * \date   2016-04-26
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

/* local includes */
#include <lx_emul.h>


#if 1
#define TRACE \
	do { \
		PLOG("%s not implemented from: %p", __func__, __builtin_return_address(0)); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif


#define TRACE_AND_STOP \
	do { \
		PWRN("%s not implemented from: %p", __func__, __builtin_return_address(0)); \
		Genode::sleep_forever(); \
	} while (0)


struct kobject *fs_kobj;


int ___ratelimit(struct ratelimit_state *, const char *)
{
	return 1; /* do not limit us */
}

int kstrtoul(const char *s, unsigned int base, unsigned long *res)
{
	TRACE_AND_STOP;
	return -1;
}


int kstrtoull(const char *s, unsigned int base, unsigned long long *res)
{
	TRACE_AND_STOP;
	return -1;
}

void __bforget(struct buffer_head *)
{
	TRACE_AND_STOP;
}

int __block_write_begin(struct page *page, loff_t pos, unsigned len, get_block_t *get_block)
{
	TRACE_AND_STOP;
	return -1;
}

ssize_t __blockdev_direct_IO(struct kiocb *iocb, struct inode *inode, struct block_device *bdev,
                             struct iov_iter *iter, loff_t offset, get_block_t get_block,
                             dio_iodone_t end_io, dio_submit_t submit_io, int flags)
{
	TRACE_AND_STOP;
	return -1;
}

void __bit_spin_unlock(int bitnum, unsigned long *addr)
{
	TRACE;
}

struct buffer_head * __bread(struct block_device *bdev, sector_t block, unsigned size)
{
	TRACE_AND_STOP;
	return NULL;
}

struct buffer_head *__find_get_block(struct block_device *bdev, sector_t block, unsigned size)
{
	TRACE_AND_STOP;
	return NULL;
}

ssize_t __generic_file_write_iter(struct kiocb *, struct iov_iter *)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long __get_free_pages(gfp_t, unsigned int)
{
	TRACE_AND_STOP;
	return -1;
}

struct buffer_head *__getblk(struct block_device *bdev, sector_t block, unsigned size)
{
	TRACE_AND_STOP;
	return NULL;
}

int __page_symlink(struct inode *inode, const char *symname, int len, int nofs)
{
	TRACE_AND_STOP;
	return -1;
}

int __set_page_dirty_nobuffers(struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

int __sync_dirty_buffer(struct buffer_head *bh, int rw)
{
	TRACE_AND_STOP;
	return -1;
}

void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
	TRACE_AND_STOP;
	return NULL;
}

int add_to_page_cache_lru(struct page *page, struct address_space *mapping, pgoff_t index,
                          gfp_t gfp_mask)
{
	TRACE_AND_STOP;
	return -1;
}

struct buffer_head *alloc_buffer_head(gfp_t gfp_flags)
{
	TRACE_AND_STOP;
	return NULL;
}

void assert_spin_locked(spinlock_t *lock)
{
	TRACE_AND_STOP;
}

long long atomic64_read(const atomic64_t *v)
{
	TRACE_AND_STOP;
	return -1;
}

struct request_queue *bdev_get_queue(struct block_device *bdev)
{
	TRACE_AND_STOP;
	return NULL;
}

unsigned short bdev_logical_block_size(struct block_device *bdev)
{
	TRACE_AND_STOP;
	return -1;
}

void bforget(struct buffer_head *bh)
{
	TRACE_AND_STOP;
}

int bh_submit_read(struct buffer_head *bh)
{
	TRACE_AND_STOP;
	return -1;
}

int bh_uptodate_or_lock(struct buffer_head *bh)
{
	TRACE_AND_STOP;
	return -1;
}

void bio_get(struct bio *bio)
{
	TRACE_AND_STOP;
}

int  bit_spin_is_locked(int bitnum, unsigned long *addr)
{
	TRACE_AND_STOP;
	return -1;
}

void bit_spin_lock(int bitnum, unsigned long *addr)
{
	TRACE_AND_STOP;
}

void bit_spin_unlock(int bitnum, unsigned long *addr)
{
	TRACE_AND_STOP;
}

wait_queue_head_t *bit_waitqueue(void *, int)
{
	TRACE_AND_STOP;
	return NULL;
}

void blk_finish_plug(struct blk_plug *)
{
	TRACE_AND_STOP;
}

bool blk_queue_discard(struct request_queue *)
{
	TRACE_AND_STOP;
	return -1;
}

void blk_start_plug(struct blk_plug *)
{
	TRACE_AND_STOP;
}

struct block_device *blkdev_get_by_dev(dev_t dev, fmode_t mode, void *holder)
{
	TRACE_AND_STOP;
	return NULL;
}

int blkdev_issue_flush(struct block_device *, gfp_t, sector_t *)
{
	TRACE_AND_STOP;
	return -1;
}

void blkdev_put(struct block_device *bdev, fmode_t mode)
{
	TRACE_AND_STOP;
}

int block_commit_write(struct page *page, unsigned from, unsigned to)
{
	TRACE_AND_STOP;
	return -1;
}

void block_invalidatepage(struct page *page, unsigned int offset, unsigned int length)
{
	TRACE_AND_STOP;
}

int block_is_partially_uptodate(struct page *page, unsigned long from, unsigned long count)
{
	TRACE_AND_STOP;
	return -1;
}

int block_page_mkwrite(struct vm_area_struct *vma, struct vm_fault *vmf, get_block_t get_block)
{
	TRACE_AND_STOP;
	return -1;
}

int block_page_mkwrite_return(int err)
{
	TRACE_AND_STOP;
	return -1;
}

int block_read_full_page(struct page*, get_block_t*)
{
	TRACE_AND_STOP;
	return -1;
}

int block_write_end(struct file *, struct address_space *, loff_t, unsigned, unsigned,
                    struct page *, void *)
{
	TRACE_AND_STOP;
	return -1;
}

void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *head))
{
	TRACE_AND_STOP;
}

void clear_inode(struct inode *)
{
	TRACE_AND_STOP;
}

void clear_nlink(struct inode *inode)
{
	TRACE_AND_STOP;
}

int clear_page_dirty_for_io(struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

int cond_resched(void)
{
	TRACE;
	return 0;
}

int cond_resched_lock(spinlock_t*)
{
	TRACE_AND_STOP;
	return -1;
}

long congestion_wait(int sync, long timeout)
{
	TRACE_AND_STOP;
	return -1;
}

u32  crc32_be(u32 crc, unsigned char const *p, size_t len)
{
	TRACE_AND_STOP;
	return -1;
}

void create_empty_buffers(struct page *, unsigned long, unsigned long b_state)
{
	TRACE_AND_STOP;
}

struct crypto_shash *crypto_alloc_shash(const char *alg_name, u32 type, u32 mask)
{
	TRACE_AND_STOP;
	return NULL;
}

void crypto_free_shash(struct crypto_shash *tfm)
{
	TRACE_AND_STOP;
}

unsigned int crypto_shash_descsize(struct crypto_shash *tfm)
{
	TRACE_AND_STOP;
	return -1;
}

int crypto_shash_update(struct shash_desc *desc, const u8 *data, unsigned int len)
{
	TRACE_AND_STOP;
	return -1;
}

struct timespec current_fs_time(struct super_block *sb)
{
	TRACE_AND_STOP;
	return ((struct timespec) { 0, 0 });
}

struct timespec current_kernel_time(void)
{
	TRACE_AND_STOP;
	return ((struct timespec) { 0, 0 });
}

struct dentry *d_find_any_alias(struct inode *inode)
{
	TRACE_AND_STOP;
	return NULL;
}

void d_instantiate(struct dentry *, struct inode *)
{
	TRACE_AND_STOP;
}

struct dentry * d_obtain_alias(struct inode *)
{
	TRACE_AND_STOP;
	return NULL;
}

char *d_path(const struct path *, char *, int)
{
	TRACE_AND_STOP;
	return NULL;
}

void d_tmpfile(struct dentry *, struct inode *)
{
	TRACE_AND_STOP;
}

int del_timer(struct timer_list *timer)
{
	TRACE_AND_STOP;
	return -1;
}

void destroy_workqueue(struct workqueue_struct *wq)
{
	TRACE_AND_STOP;
}

bool dir_relax(struct inode *inode)
{
	TRACE_AND_STOP;
	return -1;
}

u64 div64_u64(u64 dividend, u64 divisor)
{
	TRACE_AND_STOP;
	return -1;
}

u64 div_u64(u64 dividend, u32 divisor)
{
	TRACE_AND_STOP;
	return -1;
}

void down_read(struct rw_semaphore *sem)
{
	TRACE;
}

void down_write(struct rw_semaphore *sem)
{
	TRACE;
}

void dput(struct dentry *)
{
	TRACE_AND_STOP;
}

int dquot_alloc_block(struct inode *inode, qsize_t nr)
{
	TRACE_AND_STOP;
	return -1;
}

void dquot_alloc_block_nofail(struct inode *inode, qsize_t nr)
{
	TRACE_AND_STOP;
}

int dquot_claim_block(struct inode *inode, qsize_t nr)
{
	TRACE_AND_STOP;
	return -1;
}

void dquot_free_block(struct inode *inode, qsize_t nr)
{
	TRACE_AND_STOP;
}

void dquot_release_reservation_block(struct inode *inode, qsize_t nr)
{
	TRACE_AND_STOP;
}

int dquot_reserve_block(struct inode *inode, qsize_t nr)
{
	TRACE_AND_STOP;
	return -1;
}

void drop_nlink(struct inode *inode)
{
	TRACE_AND_STOP;
}

void dump_stack(void)
{
	TRACE_AND_STOP;
}

void end_buffer_read_sync(struct buffer_head *bh, int uptodate)
{
	TRACE_AND_STOP;
}

void end_buffer_write_sync(struct buffer_head *bh, int uptodate)
{
	TRACE_AND_STOP;
}

void end_page_writeback(struct page *page)
{
	TRACE_AND_STOP;
}

struct fd fdget(unsigned int fd)
{
	TRACE_AND_STOP;
	return ((struct fd){});
}

void fdput(struct fd fd)
{
	TRACE_AND_STOP;
}

int fiemap_check_flags(struct fiemap_extent_info *fieinfo, u32 fs_flags)
{
	TRACE_AND_STOP;
	return -1;
}

int fiemap_fill_next_extent(struct fiemap_extent_info *info, u64 logical, u64 phys, u64 len, u32 flags)
{
	TRACE_AND_STOP;
	return -1;
}

char *file_path(struct file *, char *, int)
{
	TRACE_AND_STOP;
	return NULL;
}

int file_update_time(struct file *file)
{
	TRACE_AND_STOP;
	return -1;
}

int filemap_fault(struct vm_area_struct *, struct vm_fault *)
{
	TRACE_AND_STOP;
	return -1;
}

int filemap_fdatawait(struct address_space *)
{
	TRACE_AND_STOP;
	return -1;
}

int filemap_fdatawrite_range(struct address_space *mapping, loff_t start, loff_t end)
{
	TRACE_AND_STOP;
	return -1;
}

int filemap_flush(struct address_space *)
{
	TRACE_AND_STOP;
	return -1;
}

void filemap_map_pages(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	TRACE_AND_STOP;
}

int filemap_write_and_wait(struct address_space *mapping)
{
	TRACE_AND_STOP;
	return -1;
}

int filemap_write_and_wait_range(struct address_space *mapping, loff_t lstart, loff_t lend)
{
	TRACE_AND_STOP;
	return -1;
}

struct page *find_get_page_flags(struct address_space *mapping, pgoff_t offset, int fgp_flags)
{
	TRACE_AND_STOP;
	return NULL;
}

struct inode *find_inode_nowait(struct super_block *, unsigned long, int (*match)(struct inode *, unsigned long, void *), void *data)
{
	TRACE_AND_STOP;
	return NULL;
}

struct page *find_lock_page(struct address_space *mapping, pgoff_t offset)
{
	TRACE_AND_STOP;
	return NULL;
}

struct page *find_or_create_page(struct address_space *mapping, pgoff_t offset, gfp_t gfp_mask)
{
	TRACE_AND_STOP;
	return NULL;
}

void flush_dcache_page(struct page *page)
{
	TRACE_AND_STOP;
}

void flush_workqueue(struct workqueue_struct *wq)
{
	TRACE_AND_STOP;
}

void free_buffer_head(struct buffer_head * bh)
{
	TRACE_AND_STOP;
}

void free_pages(unsigned long addr, unsigned int order)
{
	TRACE_AND_STOP;
}

int generic_block_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo, u64 start, u64 len, get_block_t *get_block)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_check_addressable(unsigned, u64)
{
	TRACE;
	return 0;
}

int generic_drop_inode(struct inode *inode)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_error_remove_page(struct address_space *mapping, struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

struct dentry *generic_fh_to_dentry(struct super_block *sb, struct fid *fid, int fh_len, int fh_type, struct inode *(*get_inode) (struct super_block *sb, u64 ino, u32 gen))
{
	TRACE_AND_STOP;
	return NULL;
}

struct dentry *generic_fh_to_parent(struct super_block *sb, struct fid *fid, int fh_len, int fh_type, struct inode *(*get_inode) (struct super_block *sb, u64 ino, u32 gen))
{
	TRACE_AND_STOP;
	return NULL;
}

int generic_file_fsync(struct file *, loff_t, loff_t, int)
{
	TRACE_AND_STOP;
	return -1;
}

loff_t generic_file_llseek_size(struct file *file, loff_t offset, int whence, loff_t maxsize, loff_t eof)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_file_open(struct inode * inode, struct file * filp)
{
	TRACE_AND_STOP;
	return -1;
}

ssize_t generic_file_read_iter(struct kiocb *, struct iov_iter *)
{
	TRACE_AND_STOP;
	return -1;
}

ssize_t generic_file_splice_read(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int)
{
	TRACE_AND_STOP;
	return -1;
}

void generic_fillattr(struct inode *, struct kstat *)
{
	TRACE_AND_STOP;
}

ssize_t generic_getxattr(struct dentry *dentry, const char *name, void *buffer, size_t size)
{
	TRACE_AND_STOP;
	return -1;
}

ssize_t generic_read_dir(struct file *, char __user *, size_t, loff_t *)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_readlink(struct dentry *, char __user *, int)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_removexattr(struct dentry *dentry, const char *name)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags)
{
	TRACE_AND_STOP;
	return -1;
}

ssize_t generic_write_checks(struct kiocb *, struct iov_iter *)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_write_end(struct file *, struct address_space *, loff_t, unsigned, unsigned, struct page *, void *)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_write_sync(struct file *file, loff_t pos, loff_t count)
{
	TRACE_AND_STOP;
	return -1;
}

int generic_writepages(struct address_space *mapping, struct writeback_control *wbc)
{
	TRACE_AND_STOP;
	return -1;
}

int get_order(unsigned long size)
{
	TRACE_AND_STOP;
	return -1;
}

void get_page(struct page *page)
{
	TRACE_AND_STOP;
}

unsigned long get_seconds(void)
{
	TRACE_AND_STOP;
	return -1;
}

bool gid_eq(kgid_t left, kgid_t right)
{
	TRACE_AND_STOP;
	return -1;
}

struct page *grab_cache_page_write_begin(struct address_space *mapping, pgoff_t index, unsigned flags)
{
	TRACE_AND_STOP;
	return NULL;
}

unsigned int hweight64(__u64 w)
{
	TRACE_AND_STOP;
	return -1;
}

void iget_failed(struct inode *)
{
	TRACE_AND_STOP;
}

struct inode * igrab(struct inode *)
{
	TRACE_AND_STOP;
	return NULL;
}

void ihold(struct inode * inode)
{
	TRACE_AND_STOP;
}

void inc_nlink(struct inode *inode)
{
	TRACE_AND_STOP;
}

void init_special_inode(struct inode *, umode_t, dev_t)
{
	TRACE_AND_STOP;
}

struct new_utsname *init_utsname(void)
{
	TRACE_AND_STOP;
	return NULL;
}

int inode_change_ok(const struct inode *, struct iattr *)
{
	TRACE_AND_STOP;
	return -1;
}

void inode_dio_wait(struct inode *inode)
{
	TRACE_AND_STOP;
}

void inode_inc_iversion(struct inode *inode)
{
	TRACE_AND_STOP;
}

void inode_init_owner(struct inode *inode, const struct inode *dir, umode_t mode)
{
	TRACE_AND_STOP;
}

int inode_needs_sync(struct inode *inode)
{
	TRACE_AND_STOP;
	return -1;
}

int inode_newsize_ok(const struct inode *, loff_t offset)
{
	TRACE_AND_STOP;
	return -1;
}

bool inode_owner_or_capable(const struct inode *inode)
{
	TRACE_AND_STOP;
	return -1;
}

struct backing_dev_info *inode_to_bdi(struct inode *inode)
{
	TRACE_AND_STOP;
	return NULL;
}

int insert_inode_locked(struct inode *)
{
	TRACE_AND_STOP;
	return -1;
}

void invalidate_bdev(struct block_device *)
{
	TRACE_AND_STOP;
}

void invalidate_inode_buffers(struct inode *)
{
	TRACE_AND_STOP;
}

unsigned long iov_iter_alignment(const struct iov_iter *i)
{
	TRACE_AND_STOP;
	return -1;
}

void iput(struct inode *)
{
	TRACE_AND_STOP;
}

int is_bad_inode(struct inode *)
{
	TRACE_AND_STOP;
	return -1;
}

bool is_quota_modification(struct inode *inode, struct iattr *ia)
{
	TRACE_AND_STOP;
	return -1;
}

ssize_t iter_file_splice_write(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int)
{
	TRACE_AND_STOP;
	return -1;
}

int kern_path(const char *, unsigned, struct path *)
{
	TRACE_AND_STOP;
	return -1;
}

void kill_block_super(struct super_block *sb)
{
	TRACE_AND_STOP;
}

void kmem_cache_destroy(struct kmem_cache *)
{
	TRACE_AND_STOP;
}

void kobject_del(struct kobject *kobj)
{
	TRACE_AND_STOP;
}

int kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype, struct kobject *parent, const char *fmt, ...)
{
	TRACE;
	return 0;
}

void kobject_put(struct kobject *)
{
	TRACE_AND_STOP;
}

int kobject_set_name(struct kobject *kobj, const char *name, ...)
{
	TRACE;
	return 0;
}

int kset_register(struct kset *kset)
{
	TRACE;
	return 0;
}

void kset_unregister(struct kset *kset)
{
	TRACE_AND_STOP;
}

bool kthread_should_stop(void)
{
	TRACE_AND_STOP;
	return -1;
}

int kthread_stop(struct task_struct *k)
{
	TRACE_AND_STOP;
	return -1;
}

ktime_t ktime_add_ns(const ktime_t kt, u64 nsec)
{
	TRACE_AND_STOP;
	return ((ktime_t){});
}

ktime_t ktime_sub(const ktime_t, const ktime_t)
{
	TRACE_AND_STOP;
	return ((ktime_t){});
}

void lock_buffer(struct buffer_head *bh)
{
	TRACE_AND_STOP;
}

void lock_page(struct page *page)
{
	TRACE_AND_STOP;
}

void lock_two_nondirectories(struct inode *, struct inode*)
{
	TRACE_AND_STOP;
}

void make_bad_inode(struct inode *)
{
	TRACE_AND_STOP;
}

gfp_t mapping_gfp_constraint(struct address_space *mapping, gfp_t gfp_mask)
{
	TRACE_AND_STOP;
	return -1;
}

void mapping_set_error(struct address_space *mapping, int error)
{
	TRACE_AND_STOP;
}

int mapping_tagged(struct address_space *mapping, int tag)
{
	TRACE;
	return 0;
}

void mark_buffer_dirty(struct buffer_head *bh)
{
	TRACE_AND_STOP;
}

void mark_buffer_dirty_inode(struct buffer_head *bh, struct inode *inode)
{
	TRACE_AND_STOP;
}

void mark_inode_dirty(struct inode *inode)
{
	TRACE_AND_STOP;
}

int match_int(substring_t *, int *result)
{
	TRACE_AND_STOP;
	return -1;
}

char *match_strdup(const substring_t *)
{
	TRACE_AND_STOP;
	return NULL;
}

int match_token(char *, const match_table_t table, substring_t args[])
{
	TRACE_AND_STOP;
	return -1;
}

struct mb_cache *mb_cache_create(const char *, int)
{
	TRACE_AND_STOP;
	return NULL;
}

void mb_cache_destroy(struct mb_cache *)
{
	TRACE_AND_STOP;
}

struct mb_cache_entry *mb_cache_entry_alloc(struct mb_cache *, gfp_t)
{
	TRACE_AND_STOP;
	return NULL;
}

struct mb_cache_entry *mb_cache_entry_find_first(struct mb_cache *cache, struct block_device *,  unsigned int)
{
	TRACE_AND_STOP;
	return NULL;
}

struct mb_cache_entry *mb_cache_entry_find_next(struct mb_cache_entry *, struct block_device *,  unsigned int)
{
	TRACE_AND_STOP;
	return NULL;
}

void mb_cache_entry_free(struct mb_cache_entry *)
{
	TRACE_AND_STOP;
}

struct mb_cache_entry *mb_cache_entry_get(struct mb_cache *, struct block_device *, sector_t)
{
	TRACE_AND_STOP;
	return NULL;
}

int mb_cache_entry_insert(struct mb_cache_entry *, struct block_device *, sector_t, unsigned int)
{
	TRACE_AND_STOP;
	return -1;
}

void mb_cache_entry_release(struct mb_cache_entry *)
{
	TRACE_AND_STOP;
}

void mb_cache_shrink(struct block_device *)
{
	TRACE_AND_STOP;
}

void might_sleep()
{
	TRACE_AND_STOP;
}

void mnt_drop_write_file(struct file *file)
{
	TRACE_AND_STOP;
}

int mnt_want_write_file(struct file *file)
{
	TRACE_AND_STOP;
	return -1;
}

int mod_timer(struct timer_list *timer, unsigned long expires)
{
	TRACE_AND_STOP;
	return -1;
}

void mutex_init(struct mutex *m)
{
	TRACE;
}

int mutex_is_locked(struct mutex *m)
{
	TRACE_AND_STOP;
	return 0;
}

void mutex_lock(struct mutex *m)
{
	TRACE;
}

void mutex_unlock(struct mutex *m)
{
	TRACE;
}

dev_t new_decode_dev(u32 dev)
{
	TRACE;
	return 0;
}

u32 new_encode_dev(dev_t dev)
{
	TRACE_AND_STOP;
	return -1;
}

dev_t old_decode_dev(u16 val)
{
	TRACE_AND_STOP;
	return -1;
}

u16 old_encode_dev(dev_t dev)
{
	TRACE_AND_STOP;
	return -1;
}

bool old_valid_dev(dev_t dev)
{
	TRACE_AND_STOP;
	return -1;
}

void page_cache_sync_readahead(struct address_space *mapping, struct file_ra_state *ra, struct file *filp, pgoff_t offset, unsigned long size)
{
	TRACE_AND_STOP;
}

const char *page_follow_link_light(struct dentry *, void **)
{
	TRACE_AND_STOP;
	return NULL;
}

void page_put_link(struct inode *, void *)
{
	TRACE_AND_STOP;
}

void pagecache_isize_extended(struct inode *inode, loff_t from, loff_t to)
{
	TRACE_AND_STOP;
}

unsigned pagevec_lookup(struct pagevec *pvec, struct address_space *mapping, pgoff_t start, unsigned nr_pages)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned pagevec_lookup_tag(struct pagevec *pvec, struct address_space *mapping, pgoff_t *index, int tag, unsigned nr_pages)
{
	TRACE_AND_STOP;
	return -1;
}

void pagevec_release(struct pagevec *pvec)
{
	TRACE_AND_STOP;
}

void path_put(const struct path *)
{
	TRACE_AND_STOP;
}

int timer_pending(const struct timer_list *timer)
{
	TRACE_AND_STOP;
	return 0;
}

u32 prandom_u32(void)
{
	TRACE_AND_STOP;
	return -1;
}

void put_page(struct page *page)
{
	TRACE_AND_STOP;
}

void ratelimit_state_init(struct ratelimit_state *rs, int interval, int burst)
{
	TRACE;
}

void rcu_barrier(void)
{
	TRACE_AND_STOP;
}

void read_lock(rwlock_t *)
{
	TRACE;
}

void read_unlock(rwlock_t *)
{
	TRACE;
}

int register_shrinker(struct shrinker *)
{
	TRACE;
	return 0;
}

int redirty_page_for_writepage(struct writeback_control *wbc, struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long round_jiffies_up(unsigned long j)
{
	TRACE_AND_STOP;
	return -1;
}

void rwlock_init(rwlock_t *)
{
	TRACE;
}

int rwsem_is_locked(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
	return -1;
}

struct buffer_head * sb_bread(struct super_block *sb, sector_t block)
{
	TRACE_AND_STOP;
	return NULL;
}

void sb_breadahead(struct super_block *sb, sector_t block)
{
	TRACE_AND_STOP;
}

void sb_end_intwrite(struct super_block *sb)
{
	TRACE_AND_STOP;
}

void sb_end_pagefault(struct super_block *sb)
{
	TRACE_AND_STOP;
}

void sb_end_write(struct super_block *sb)
{
	TRACE_AND_STOP;
}

struct buffer_head * sb_find_get_block(struct super_block *sb, sector_t block)
{
	TRACE_AND_STOP;
	return NULL;
}

struct buffer_head * sb_getblk_gfp(struct super_block *sb, sector_t block, gfp_t gfp)
{
	TRACE_AND_STOP;
	return NULL;
}

int sb_issue_discard(struct super_block *sb, sector_t block, sector_t nr_blocks, gfp_t gfp_mask, unsigned long flags)
{
	TRACE_AND_STOP;
	return -1;
}

int sb_issue_zeroout(struct super_block *sb, sector_t block, sector_t nr_blocks, gfp_t gfp_mask)
{
	TRACE_AND_STOP;
	return -1;
}

void sb_start_intwrite(struct super_block *sb)
{
	TRACE_AND_STOP;
}

void sb_start_pagefault(struct super_block *sb)
{
	TRACE_AND_STOP;
}

void sb_start_write(struct super_block *sb)
{
	TRACE_AND_STOP;
}

extern int schedule_hrtimeout(ktime_t *expires, const enum hrtimer_mode mode)
{
	TRACE_AND_STOP;
	return -1;
}

signed long schedule_timeout_interruptible(signed long timeout)
{
	TRACE_AND_STOP;
	return -1;
}

signed long schedule_timeout_uninterruptible(signed long timeout)
{
	TRACE_AND_STOP;
	return -1;
}

int set_blocksize(struct block_device *, int)
{
	TRACE_AND_STOP;
	return -1;
}

loff_t seq_lseek(struct file *, loff_t, int)
{
	TRACE_AND_STOP;
	return -1;
}

int seq_open(struct file *, const struct seq_operations *)
{
	TRACE_AND_STOP;
	return -1;
}

void seq_printf(struct seq_file *m, const char *fmt, ...)
{
	TRACE_AND_STOP;
}

void seq_puts(struct seq_file *m, const char *s)
{
	TRACE_AND_STOP;
}

ssize_t seq_read(struct file *, char __user *, size_t, loff_t *)
{
	TRACE_AND_STOP;
	return -1;
}

int seq_release(struct inode *, struct file *)
{
	TRACE_AND_STOP;
	return -1;
}

void set_bh_page(struct buffer_head *bh, struct page *page, unsigned long offset)
{
	TRACE_AND_STOP;
}

void set_current_state(int)
{
	TRACE_AND_STOP;
}

void set_page_writeback(struct page *page)
{
	TRACE_AND_STOP;
}

void set_page_writeback_keepwrite(struct page *page)
{
	TRACE_AND_STOP;
}

int set_task_ioprio(struct task_struct *task, int ioprio)
{
	TRACE;
	return 0;
}

void setattr_copy(struct inode *inode, const struct iattr *attr)
{
	TRACE_AND_STOP;
}

void setup_timer(struct timer_list *timer, void (*function)(unsigned long), unsigned long data)
{
	TRACE;
}

const char *simple_follow_link(struct dentry *, void **)
{
	TRACE_AND_STOP;
	return NULL;
}

long simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	TRACE_AND_STOP;
	return -1;
}

int single_open(struct file *, int (*)(struct seq_file *, void *), void *)
{
	TRACE_AND_STOP;
	return -1;
}

int single_release(struct inode *, struct file *)
{
	TRACE_AND_STOP;
	return -1;
}

char *skip_spaces(const char *)
{
	TRACE_AND_STOP;
	return NULL;
}

void spin_lock(spinlock_t *lock)
{
	TRACE;
}

void spin_lock_init(spinlock_t *lock)
{
	TRACE;
}

void spin_lock_irqsave(spinlock_t *lock, unsigned long flags)
{
	TRACE_AND_STOP;
}

int spin_trylock(spinlock_t *lock)
{
	TRACE;
	return 0;
}

void spin_unlock(spinlock_t *lock)
{
	TRACE;
}

void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	TRACE_AND_STOP;
}

int submit_bh(int, struct buffer_head *)
{
	TRACE_AND_STOP;
	return -1;
}

int sync_blockdev(struct block_device *bdev)
{
	TRACE_AND_STOP;
	return -1;
}

int sync_dirty_buffer(struct buffer_head *bh)
{
	TRACE_AND_STOP;
	return -1;
}

int sync_filesystem(struct super_block *)
{
	TRACE_AND_STOP;
	return -1;
}

int sync_inode_metadata(struct inode *inode, int wait)
{
	TRACE_AND_STOP;
	return -1;
}

int sync_mapping_buffers(struct address_space *mapping)
{
	TRACE_AND_STOP;
	return -1;
}

void tag_pages_for_writeback(struct address_space *mapping, pgoff_t start, pgoff_t end)
{
	TRACE_AND_STOP;
}

void truncate_inode_pages(struct address_space *, loff_t)
{
	TRACE_AND_STOP;
}

void truncate_inode_pages_final(struct address_space *)
{
	TRACE_AND_STOP;
}

void truncate_pagecache(struct inode *inode, loff_t new_)
{
	TRACE_AND_STOP;
}

void truncate_pagecache_range(struct inode *inode, loff_t offset, loff_t end)
{
	TRACE_AND_STOP;
}

int try_to_free_buffers(struct page *)
{
	TRACE_AND_STOP;
	return -1;
}

int try_to_release_page(struct page * page, gfp_t gfp_mask)
{
	TRACE_AND_STOP;
	return -1;
}

bool try_to_writeback_inodes_sb(struct super_block *, enum wb_reason reason)
{
	TRACE_AND_STOP;
	return -1;
}

int trylock_page(struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

bool uid_eq(kuid_t, kuid_t)
{
	TRACE_AND_STOP;
	return -1;
}

void unlock_buffer(struct buffer_head *bh)
{
	TRACE_AND_STOP;
}

void unlock_page(struct page *page)
{
	TRACE;
}

void unlock_two_nondirectories(struct inode *, struct inode*)
{
	TRACE_AND_STOP;
}

void unmap_underlying_metadata(struct block_device *bdev, sector_t block)
{
	TRACE_AND_STOP;
}

int unregister_filesystem(struct file_system_type *)
{
	TRACE_AND_STOP;
	return -1;
}

void unregister_shrinker(struct shrinker *)
{
	TRACE_AND_STOP;
}

void up_read(struct rw_semaphore *sem)
{
	TRACE;
}

void up_write(struct rw_semaphore *sem)
{
	TRACE;
}

unsigned long vfs_pressure_ratio(unsigned long val)
{
	TRACE_AND_STOP;
	return 0;
}

loff_t vfs_setpos(struct file *file, loff_t offset, loff_t maxsize)
{
	TRACE_AND_STOP;
	return -1;
}

struct page *virt_to_page(const void *x)
{
	TRACE_AND_STOP;
	return NULL;
}

void wait_for_stable_page(struct page *page)
{
	TRACE_AND_STOP;
}

int wait_on_bit_io(unsigned long *word, int bit, unsigned mode)
{
	TRACE_AND_STOP;
	return -1;
}

void wait_on_page_writeback(struct page *page)
{
	TRACE_AND_STOP;
}

int wake_bit_function(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
	TRACE_AND_STOP;
	return -1;
}

void wake_up_bit(void *, int)
{
	TRACE_AND_STOP;
}

int wake_up_process(struct task_struct *tsk)
{
	TRACE_AND_STOP;
	return -1;
}

int write_cache_pages(struct address_space *mapping, struct writeback_control *wbc, writepage_t writepage, void *data)
{
	TRACE_AND_STOP;
	return -1;
}

void write_dirty_buffer(struct buffer_head *bh, int rw)
{
	TRACE_AND_STOP;
}

void write_lock(rwlock_t *)
{
	TRACE;
}

int  write_trylock(rwlock_t *)
{
	TRACE_AND_STOP;
	return -1;
}

void write_unlock(rwlock_t *)
{
	TRACE;
}

void zero_user(struct page *page, unsigned start, unsigned size)
{
	TRACE_AND_STOP;
}


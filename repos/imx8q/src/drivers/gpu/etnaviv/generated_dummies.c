/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2021-04-13
 */

#include <lx_emul.h>


#include <linux/ratelimit.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <asm-generic/delay.h>

void __const_udelay(unsigned long xloops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_print.h>

void __drm_printfn_seq_file(struct drm_printer * p,struct va_format * vaf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_print.h>

void __drm_puts_seq_file(struct drm_printer * p,const char * str)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pagevec.h>

void __pagevec_release(struct pagevec * pvec)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * __vmalloc(unsigned long size,gfp_t gfp_mask,pgprot_t prot)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/wait.h>

void __wake_up(struct wait_queue_head * wq_head,unsigned int mode,int nr_exclusive,void * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <asm-generic/bug.h>

void __warn_printk(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwlock_api_smp.h>

void __lockfunc _raw_read_lock(rwlock_t * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwlock_api_smp.h>

void __lockfunc _raw_read_unlock(rwlock_t * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcupdate.h>

void call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool cancel_delayed_work_sync(struct delayed_work * dwork)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/swap.h>

void check_move_unevictable_pages(struct pagevec * pvec)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

void delayed_work_timer_fn(struct timer_list * t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

void destroy_workqueue(struct workqueue_struct * wq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/devcoredump.h>

void dev_coredumpv(struct device * dev,void * data,size_t datalen,gfp_t gfp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

int dma_buf_mmap(struct dma_buf * dmabuf,struct vm_area_struct * vma,unsigned long pgoff)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void dma_buf_put(struct dma_buf * dmabuf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void * dma_buf_vmap(struct dma_buf * dmabuf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void dma_buf_vunmap(struct dma_buf * dmabuf,void * vaddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

int dma_direct_map_sg(struct device * dev,struct scatterlist * sgl,int nents,enum dma_data_direction dir,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_direct_sync_sg_for_cpu(struct device * dev,struct scatterlist * sgl,int nents,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_direct_sync_sg_for_device(struct device * dev,struct scatterlist * sgl,int nents,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_direct_unmap_sg(struct device * dev,struct scatterlist * sgl,int nents,enum dma_data_direction dir,unsigned long attrs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_ioctl.h>

long drm_compat_ioctl(struct file * filp,unsigned int cmd,unsigned long arg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_debugfs.h>

int drm_debugfs_create_files(const struct drm_info_list * files,int count,struct dentry * root,struct drm_minor * minor)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_drv.h>

bool drm_dev_enter(struct drm_device * dev,int * idx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_drv.h>

void drm_dev_exit(int idx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_drv.h>

void drm_dev_put(struct drm_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_drv.h>

void drm_dev_unregister(struct drm_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_print.h>

void drm_err(const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_prime.h>

int drm_gem_prime_fd_to_handle(struct drm_device * dev,struct drm_file * file_priv,int prime_fd,uint32_t * handle)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_prime.h>

int drm_gem_prime_handle_to_fd(struct drm_device * dev,struct drm_file * file_priv,uint32_t handle,uint32_t flags,int * prime_fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_ioctl.h>

long drm_ioctl(struct file * filp,unsigned int cmd,unsigned long arg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_file.h>

int drm_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_file.h>

__poll_t drm_poll(struct file * filp,struct poll_table_struct * wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_prime.h>

void drm_prime_gem_destroy(struct drm_gem_object * obj,struct sg_table * sg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_prime.h>

struct sg_table * drm_prime_pages_to_sg(struct page ** pages,unsigned int nr_pages)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_prime.h>

int drm_prime_sg_to_page_addr_arrays(struct sg_table * sgt,struct page ** pages,dma_addr_t * addrs,int max_entries)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_print.h>

void drm_printf(struct drm_printer * p,const char * f,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_file.h>

ssize_t drm_read(struct file * filp,char __user * buffer,size_t count,loff_t * offset)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_file.h>

int drm_release(struct inode * inode,struct file * filp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_vma_manager.h>

int drm_vma_node_allow(struct drm_vma_offset_node * node,struct drm_file * tag)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_vma_manager.h>

bool drm_vma_node_is_allowed(struct drm_vma_offset_node * node,struct drm_file * tag)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_vma_manager.h>

void drm_vma_node_revoke(struct drm_vma_offset_node * node,struct drm_file * tag)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_vma_manager.h>

int drm_vma_offset_add(struct drm_vma_offset_manager * mgr,struct drm_vma_offset_node * node,unsigned long pages)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_vma_manager.h>

struct drm_vma_offset_node * drm_vma_offset_lookup_locked(struct drm_vma_offset_manager * mgr,unsigned long start,unsigned long pages)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_vma_manager.h>

void drm_vma_offset_remove(struct drm_vma_offset_manager * mgr,struct drm_vma_offset_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void fd_install(unsigned int fd,struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <asm-generic/bitops/find.h>

unsigned long find_next_bit(const unsigned long * addr,unsigned long size,unsigned long offset)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/wait.h>

void finish_wait(struct wait_queue_head * wq_head,struct wait_queue_entry * wq_entry)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

void flush_workqueue(struct workqueue_struct * wq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void fput(struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

int get_unused_fd_flags(unsigned flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int get_user_pages_fast(unsigned long start,int nr_pages,unsigned int gup_flags,struct page ** pages)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutree.h>

void kfree_call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void * krealloc(const void * p,size_t new_size,gfp_t flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

size_t ksize(const void * objp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timekeeping.h>

ktime_t ktime_get(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timekeeping.h>

void ktime_get_ts64(struct timespec64 * ts)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void kvfree(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void * kvmalloc_node(size_t size,gfp_t flags,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/swap.h>

void mark_page_accessed(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mutex.h>

int __sched mutex_lock_interruptible(struct mutex * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mutex.h>

int __sched mutex_lock_killable(struct mutex * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

loff_t no_llseek(struct file * file,loff_t offset,int whence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <asm-generic/preempt.h>

asmlinkage __visible void __sched notrace preempt_schedule(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void print_hex_dump(const char * level,const char * prefix_str,int prefix_type,int rowsize,int groupsize,const void * buf,size_t len,bool ascii)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void put_unused_fd(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool queue_delayed_work_on(int cpu,struct workqueue_struct * wq,struct delayed_work * dwork,unsigned long delay)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool queue_work_on(int cpu,struct workqueue_struct * wq,struct work_struct * work)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pagemap.h>

void release_pages(struct page ** pages,int nr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

signed long __sched schedule_timeout(signed long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

loff_t seq_lseek(struct file * file,loff_t offset,int whence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_printf(struct seq_file * m,const char * f,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_puts(struct seq_file * m,const char * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

ssize_t seq_read(struct file * file,char __user * buf,size_t size,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int set_page_dirty(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

void sg_free_table(struct sg_table * table)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

struct scatterlist * sg_next(struct scatterlist * sg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

int single_open(struct file * file,int (* show)(struct seq_file *,void *),void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

int single_release(struct inode * inode,struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sync_file.h>

struct sync_file * sync_file_create(struct dma_fence * fence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sync_file.h>

struct dma_fence * sync_file_get_fence(int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

struct workqueue_struct *system_wq;


#include <linux/thermal.h>

void thermal_cooling_device_unregister(struct thermal_cooling_device * cdev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int vm_insert_page(struct vm_area_struct * vma,unsigned long addr,struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * vmap(struct page ** pages,unsigned int count,unsigned long flags,pgprot_t prot)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void vunmap(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/completion.h>

unsigned long __sched wait_for_completion_timeout(struct completion * x,unsigned long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

int wake_up_state(struct task_struct * p,unsigned int state)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ww_mutex.h>

int __sched ww_mutex_lock_interruptible(struct ww_mutex * lock,struct ww_acquire_ctx * ctx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ww_mutex.h>

void __sched ww_mutex_unlock(struct ww_mutex * lock)
{
	lx_emul_trace_and_stop(__func__);
}


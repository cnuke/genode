/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2021-04-27
 */

#include <lx_emul.h>


#include <linux/ratelimit.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
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


#include <linux/sched.h>

pid_t __task_pid_nr_ns(struct task_struct * task,enum pid_type type,struct pid_namespace * ns)
{
	lx_emul_trace_and_stop(__func__);
}


#include <asm-generic/bug.h>

void __warn_printk(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool cancel_delayed_work_sync(struct delayed_work * dwork)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/capability.h>

bool capable(int cap)
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

struct dma_buf_attachment * dma_buf_attach(struct dma_buf * dmabuf,struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void dma_buf_detach(struct dma_buf * dmabuf,struct dma_buf_attachment * attach)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct dma_buf * dma_buf_export(const struct dma_buf_export_info * exp_info)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

int dma_buf_fd(struct dma_buf * dmabuf,int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct dma_buf * dma_buf_get(int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct sg_table * dma_buf_map_attachment(struct dma_buf_attachment * attach,enum dma_data_direction direction)
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

void dma_buf_unmap_attachment(struct dma_buf_attachment * attach,struct sg_table * sg_table,enum dma_data_direction direction)
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

void dma_direct_sync_sg_for_cpu(struct device * dev,struct scatterlist * sgl,int nents,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-mapping.h>

void dma_direct_sync_sg_for_device(struct device * dev,struct scatterlist * sgl,int nents,enum dma_data_direction dir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_ioctl.h>

long drm_compat_ioctl(struct file * filp,unsigned int cmd,unsigned long arg)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_connector_property_set_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_connector_property_set_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_crtc_get_sequence_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_crtc_get_sequence_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_crtc_queue_sequence_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_crtc_queue_sequence_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_debugfs.h>

int drm_debugfs_create_files(const struct drm_info_list * files,int count,struct dentry * root,struct drm_minor * minor)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_drv.h>

void drm_dev_get(struct drm_device * dev)
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


extern int drm_dropmaster_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_dropmaster_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_print.h>

void drm_err(const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_getmagic(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_getmagic(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_irq_by_busid(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_irq_by_busid(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_auth.h>

bool drm_is_current_master(struct drm_file * fpriv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_addbufs(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_addbufs(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_addctx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_addctx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_addmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_addmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_dma_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_dma_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_freebufs(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_freebufs(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_getctx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_getctx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_getmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_getmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_getsareactx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_getsareactx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_infobufs(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_infobufs(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_irq_control(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_irq_control(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_lock(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_lock(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_mapbufs(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_mapbufs(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_markbufs(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_markbufs(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_modeset_ctl_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_modeset_ctl_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_newctx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_newctx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_resctx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_resctx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_rmctx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_rmctx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_rmmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_rmmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_setsareactx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_setsareactx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_sg_alloc(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_sg_alloc(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_sg_free(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_sg_free(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_switchctx(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_switchctx(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_legacy_unlock(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_legacy_unlock(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_addfb2_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_addfb2_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_addfb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_addfb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_atomic_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_atomic_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_create_dumb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_create_dumb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

int drm_mode_create_lease_ioctl(struct drm_device * dev,void * data,struct drm_file * lessor_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_createblob_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_createblob_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_cursor2_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_cursor2_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_cursor_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_cursor_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_destroy_dumb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_destroy_dumb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_destroyblob_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_destroyblob_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_dirtyfb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_dirtyfb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_gamma_get_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_gamma_get_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_gamma_set_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_gamma_set_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

int drm_mode_get_lease_ioctl(struct drm_device * dev,void * data,struct drm_file * lessee_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getblob_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getblob_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getconnector(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getconnector(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getcrtc(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getcrtc(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getencoder(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getencoder(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getfb(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getfb(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getplane(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getplane(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getplane_res(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getplane_res(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getproperty_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getproperty_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_getresources(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_getresources(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

int drm_mode_list_lessees_ioctl(struct drm_device * dev,void * data,struct drm_file * lessor_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_mmap_dumb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_mmap_dumb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_obj_get_properties_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_obj_get_properties_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_obj_set_property_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_obj_set_property_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_page_flip_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_page_flip_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

int drm_mode_revoke_lease_ioctl(struct drm_device * dev,void * data,struct drm_file * lessor_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_rmfb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_rmfb_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_setcrtc(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_setcrtc(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_mode_setplane(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_mode_setplane(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_file.h>

int drm_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_pci_set_busid(struct drm_device * dev,struct drm_master * master);
int drm_pci_set_busid(struct drm_device * dev,struct drm_master * master)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_file.h>

__poll_t drm_poll(struct file * filp,struct poll_table_struct * wait)
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


extern int drm_setmaster_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_setmaster_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_destroy_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_destroy_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_fd_to_handle_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_fd_to_handle_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_handle_to_fd_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_handle_to_fd_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_query_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_query_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_reset_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_reset_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_signal_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_signal_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_timeline_signal_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_timeline_signal_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_timeline_wait_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_timeline_wait_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_transfer_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_transfer_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_syncobj_wait_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private);
int drm_syncobj_wait_ioctl(struct drm_device * dev,void * data,struct drm_file * file_private)
{
	lx_emul_trace_and_stop(__func__);
}


extern int drm_wait_vblank_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int drm_wait_vblank_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
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


#include <linux/workqueue.h>

void flush_workqueue(struct workqueue_struct * wq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

void free_pages(unsigned long addr,unsigned int order)
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


#include <linux/string.h>

char * kstrdup(const char * s,gfp_t gfp)
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


#include <linux/highuid.h>

int overflowuid;


#include <linux/pci.h>

struct bus_type pci_bus_type;


#include <linux/file.h>

void put_unused_fd(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pagemap.h>

void release_pages(struct page ** pages,int nr)
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


#include <linux/sched.h>

int wake_up_state(struct task_struct * p,unsigned int state)
{
	lx_emul_trace_and_stop(__func__);
}


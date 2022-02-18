/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Josef Soentgen
 * \date   2022-02-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/cpuhotplug.h>

int __cpuhp_setup_state(enum cpuhp_state state,const char * name,bool invoke,int (* startup)(unsigned int cpu),int (* teardown)(unsigned int cpu),bool multi_instance)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/timekeeper_internal.h>

void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}


#include <linux/clocksource.h>

void clocksource_arch_init(struct clocksource * cs)
{
	lx_emul_trace(__func__);
}


#include <linux/sched/signal.h>

void ignore_signals(struct task_struct * t)
{
	lx_emul_trace(__func__);
}


#include <linux/sched/loadavg.h>

void calc_global_load(void)
{
	lx_emul_trace(__func__);
}


#include <linux/kernel_stat.h>

void account_process_tick(struct task_struct * p,int user_tick)
{
	lx_emul_trace(__func__);
}


#include <linux/rcupdate.h>

void rcu_sched_clock_irq(int user)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

int sysfs_create_bin_file(struct kobject * kobj,const struct bin_attribute * attr)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysfs.h>

int sysfs_create_file_ns(struct kobject * kobj,const struct attribute * attr,const void * ns)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysfs.h>

int sysfs_create_groups(struct kobject * kobj,const struct attribute_group ** groups)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysfs.h>

int sysfs_create_group(struct kobject * kobj,const struct attribute_group * grp)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysfs.h>

int sysfs_create_link(struct kobject * kobj,struct kobject * target,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysfs.h>

void sysfs_remove_link(struct kobject * kobj,const char * name)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

void sysfs_remove_file_ns(struct kobject * kobj,const struct attribute * attr,const void * ns)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

void sysfs_remove_groups(struct kobject * kobj,const struct attribute_group ** groups)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

void sysfs_remove_dir(struct kobject * kobj)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

void sysfs_remove_bin_file(struct kobject * kobj,const struct bin_attribute * attr)
{
	lx_emul_trace(__func__);
}


#include <linux/kernfs.h>

void kernfs_get(struct kernfs_node * kn)
{
	lx_emul_trace(__func__);
}


#include <linux/kernfs.h>

void kernfs_put(struct kernfs_node * kn)
{
	lx_emul_trace(__func__);
}


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/random.h>

int __must_check get_random_bytes_arch(void * buf,int nbytes)
{
	lx_emul_trace(__func__);
	printk("%s: leaving buffer unmodified!\n", __func__);
	return 0;
}


#include <linux/random.h>

void get_random_bytes(void * buf,int nbytes)
{
	lx_emul_trace(__func__);
	printk("%s: leaving buffer unmodified!\n", __func__);
}


#include <linux/random.h>

int add_random_ready_callback(struct random_ready_callback * rdy)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/random.h>

void add_device_randomness(const void * buf,unsigned int size)
{
	lx_emul_trace(__func__);
}


#include <linux/random.h>

void add_interrupt_randomness(int irq,int irq_flags)
{
	lx_emul_trace(__func__);
}


extern bool irq_wait_for_poll(struct irq_desc * desc);
bool irq_wait_for_poll(struct irq_desc * desc)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

void note_interrupt(struct irq_desc * desc,irqreturn_t action_ret)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int __register_chrdev(unsigned int major,unsigned int baseminor,unsigned int count,const char * name,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/fs.h>

int register_chrdev_region(dev_t from,unsigned count,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void register_handler_proc(unsigned int irq,struct irqaction * action);
void register_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace(__func__);
}


extern void register_irq_proc(unsigned int irq,struct irq_desc * desc);
void register_irq_proc(unsigned int irq,struct irq_desc * desc)
{
	lx_emul_trace(__func__);
}


#include <linux/cdev.h>

void cdev_init(struct cdev * cdev,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
}


#include <linux/cdev.h>

int cdev_add(struct cdev * p,dev_t dev,unsigned count)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/cdev.h>

void cdev_del(struct cdev * p)
{
	lx_emul_trace(__func__);
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/proc_fs.h>

struct proc_dir_entry { int dummy; };

struct proc_dir_entry * proc_create_seq_private(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct seq_operations * ops,unsigned int state_size,void * data)
{
	static struct proc_dir_entry ret;
	lx_emul_trace(__func__);
	return &ret;
}


#include <linux/property.h>

int software_node_notify(struct device * dev,unsigned long action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/utsname.h>
#include <linux/user_namespace.h>

struct user_namespace init_user_ns;

struct uts_namespace init_uts_ns;



/*
 * linux/seq_file.h depends on user_namespace being defined, add
 * all dummies pulling in this header below here
 */


#include <linux/seq_file.h>

void seq_vprintf(struct seq_file * m,const char * f,va_list args)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vt_kern.h>

void unblank_screen(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pci_allocate_vc_save_buffers(struct pci_dev * dev);
void pci_allocate_vc_save_buffers(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


extern void pci_vpd_init(struct pci_dev * dev);
void pci_vpd_init(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


extern int pci_proc_attach_device(struct pci_dev * dev);
int pci_proc_attach_device(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/kernel.h>

bool parse_option_str(const char * str,const char * option)
{
	lx_emul_trace(__func__);
	return false;
}


extern bool pat_enabled(void);
bool pat_enabled(void)
{
	// XXX pat_enabled necessary?
	lx_emul_trace(__func__);
	return false;
}


#include <linux/mm.h>

bool is_vmalloc_addr(const void * x)
{
	lx_emul_trace(__func__);
	return false;
}


unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];


extern int pci_dev_specific_acs_enabled(struct pci_dev * dev,u16 acs_flags);
int pci_dev_specific_acs_enabled(struct pci_dev * dev,u16 acs_flags)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_disable_acs_redir(struct pci_dev * dev);
int pci_dev_specific_disable_acs_redir(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_enable_acs(struct pci_dev * dev);
int pci_dev_specific_enable_acs(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_reset(struct pci_dev * dev,int probe);
int pci_dev_specific_reset(struct pci_dev * dev,int probe)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/pci.h>

void pci_fixup_device(enum pci_fixup_pass pass,struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


const unsigned long module_cert_size = 0;
const u8 system_certificate_list[] = { };
const unsigned long system_certificate_list_size = sizeof (system_certificate_list);

const u8 shipped_regdb_certs[] = { };
unsigned int shipped_regdb_certs_len = sizeof (shipped_regdb_certs);



/*
 * Generate_dummies.c will otherwise pull in <linux/rcutree.h>
 * that clashes with rcutiny.h.
 */
void rcu_barrier(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>
#include <linux/jump_label.h> /* for DEFINE_STATIC_KEY_FALSE */

void bpf_prog_change_xdp(struct bpf_prog *prev_prog, struct bpf_prog *prog)
{
	lx_emul_trace(__func__);
}

DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);


__wsum csum_partial(const void * buff,int len,__wsum sum)
{
	lx_emul_trace_and_stop(__func__);
}


struct static_key_false init_on_alloc;


#include <linux/proc_ns.h>

int proc_alloc_inum(unsigned int * inum)
{
	*inum = 1; /* according to linux/proc_ns.h without CONFIG_PROC_FS */
	return 0;
}


#include <linux/random.h>

u32 get_random_u32(void)
{
	static unsigned counter = 42;
	printk("%s: return %u\n", __func__, counter);
	return counter++;
}


#include <linux/prandom.h>

void prandom_bytes(void * buf,size_t bytes)
{
	lx_emul_trace(__func__);
	printk("%s: do nothing\n", __func__);
}


#include <linux/prandom.h>

u32 prandom_u32(void)
{
	printk("%s: return 4\n", __func__);
	return 4;
}


#include <net/net_namespace.h>

__init int net_sysctl_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/proc_fs.h>

struct proc_dir_entry * proc_create_net_data(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct seq_operations * ops,unsigned int state_size,void * data)
{
	static struct proc_dir_entry _proc_dir_entry;
	lx_emul_trace(__func__);
	return &_proc_dir_entry;
}


#include <linux/fs.h>

struct inode * new_inode_pseudo(struct super_block * sb)
{
	struct inode *inode;

	inode = kzalloc(sizeof (struct inode), 0);
	if (!inode)
		return (struct inode*)ERR_PTR(-ENOMEM);

	return inode;
}


#include <linux/fs.h>

unsigned int get_next_ino(void)
{
	static unsigned int count = 0;
	return ++count;
}


#include <linux/netdevice.h>

int __init dev_proc_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/stringhash.h>

unsigned int full_name_hash(const void * salt,const char * name,unsigned int len)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/key.h>

static struct key _key;

struct key * keyring_alloc(const char * description,kuid_t uid,kgid_t gid,const struct cred * cred,key_perm_t perm,unsigned long flags,struct key_restriction * restrict_link,struct key * dest)
{
	lx_emul_trace(__func__);
	return &_key;
}


#include <linux/kobject.h>

int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	printk("%s:%d\n", __func__, __LINE__);
	lx_backtrace();
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sched.h>

void sched_set_fifo(struct task_struct * p)
{
	lx_emul_trace(__func__);
}


#include <linux/moduleparam.h>

void kernel_param_lock(struct module * mod)
{
	lx_emul_trace(__func__);
}


#include <linux/moduleparam.h>

void kernel_param_unlock(struct module * mod)
{
	lx_emul_trace(__func__);
}


unsigned long lpj_fine = 0;


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace(__func__);
}


#include <linux/filter.h>

int sk_filter_trim_cap(struct sock * sk,struct sk_buff * skb,unsigned int cap)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/capability.h>

bool file_ns_capable(const struct file * file,struct user_namespace * ns,int cap)
{
	lx_emul_trace(__func__);
	return true;
}

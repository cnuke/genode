/*
 * \brief  Emulation of the Linux kernel API
 * \author Josef Soentgen
 * \date   2016-04-21
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

#include <lx_emul/extern_c_begin.h>


#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(4,4,3)


/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100 };

#define DEBUG_LINUX_PRINTK 1

#include <lx_emul/printf.h>

/***************
 ** asm/bug.h **
 ***************/

#include <lx_emul/bug.h>

#define BUILD_BUG_ON(condition)

#define BUILD_BUG_ON_NOT_POWER_OF_2(n)          \
	BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))


/******************
 ** asm/atomic.h **
 ******************/

#include <lx_emul/atomic.h>


/*******************
 ** linux/types.h **
 *******************/

#include <lx_emul/types.h>

typedef int clockid_t;

typedef size_t    __kernel_size_t;
typedef long      __kernel_time_t;
typedef long      __kernel_suseconds_t;
typedef long long __kernel_loff_t;

typedef __kernel_loff_t loff_t;

/* use LBDAF */
typedef u64 sector_t;
typedef u64 blkcnt_t;

typedef unsigned short umode_t;
typedef unsigned fmode_t;

typedef unsigned short ushort;

#define __aligned_u64 __u64 __attribute__((aligned(8)))

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]


/************************
 ** uapi/linux/types.h **
 ************************/

#define __bitwise__

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;

/*
 * needed by include/net/cfg80211.h
 */
struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head


/************************************
 ** uapi/asm-generic/posix_types.h **
 ************************************/

typedef struct {
	    int val[2];
} __kernel_fsid_t;


/*******************
 ** asm/barrier.h **
 *******************/

#include <lx_emul/barrier.h>

#define smp_load_acquire(p)     *(p)
#define smp_store_release(p, v) *(p) = v;
#define smp_mb__before_atomic() mb()
#define smp_mb__after_atomic()  mb()


/**********************
 ** asm-generic/io.h **
 **********************/

#include <lx_emul/mmio.h>

#define mmiowb() barrier()
struct device;

void *ioremap(resource_size_t offset, unsigned long size);
void  iounmap(volatile void *addr);
void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size);
void *devm_ioremap_nocache(struct device *dev, resource_size_t offset,
                           unsigned long size);

void *ioremap_wc(resource_size_t phys_addr, unsigned long size);

#define ioremap_nocache ioremap

void *phys_to_virt(unsigned long address);


/*************************
 ** asm-generic/cache.h **
 *************************/

/* XXX is 64 for CA15 */
#define L1_CACHE_BYTES  32
#define SMP_CACHE_BYTES L1_CACHE_BYTES

#define ____cacheline_aligned __attribute__((__aligned__(SMP_CACHE_BYTES)))
#define ____cacheline_aligned_in_smp


/**********************
 ** linux/compiler.h **
 **********************/

#include <lx_emul/compiler.h>

#define __cond_lock(x,c) (c)

#define noinline_for_stack noinline

static inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile __u8  *)p = *(__u8  *)res; break;
	case 2: *(volatile __u16 *)p = *(__u16 *)res; break;
	case 4: *(volatile __u32 *)p = *(__u32 *)res; break;
	case 8: *(volatile __u64 *)p = *(__u64 *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

#define WRITE_ONCE(x, val)                        \
({                                                \
	union { typeof(x) __val; char __c[1]; } __u = \
	{ .__val = (__force typeof(x)) (val) };       \
	__write_once_size(&(x), __u.__c, sizeof(x));  \
	__u.__val;                                    \
})

static inline void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
		case 1: *(__u8  *)res = *(volatile __u8  *)p; break;
		case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
		case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
		case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
		default:
			barrier();
			__builtin_memcpy((void *)res, (const void *)p, size);
			barrier();
	}
}

#define READ_ONCE(x) \
({                                               \
	union { typeof(x) __val; char __c[1]; } __u; \
	__read_once_size(&(x), __u.__c, sizeof(x));  \
	__u.__val;                                   \
})


/**************************
 ** linux/compiler-gcc.h **
 **************************/

#ifdef __aligned
#undef __aligned
#endif
#define __aligned(x)  __attribute__((aligned(x)))
#define __visible     __attribute__((externally_visible))

#define OPTIMIZER_HIDE_VAR(var) __asm__ ("" : "=r" (var) : "0" (var))


/********************
 ** linux/module.h **
 ********************/

#include <lx_emul/module.h>

static inline bool module_sig_ok(struct module *module) { return true; }

#define module_name(mod) "foobar"

/* XXX MODULE_ALIAS_FS might be needed */
#define MODULE_ALIAS_FS(NAME)


/*************************
 ** linux/moduleparam.h **
 *************************/

#define __MODULE_INFO(tag, name, info)

static inline void kernel_param_lock(struct module *mod) { }
static inline void kernel_param_unlock(struct module *mod) { }


/*******************
 ** linux/errno.h **
 *******************/

#include <lx_emul/errno.h>

enum {
	EBADF       =   9,
	ENOTTY      =  25,
	EROFS       =  30,
	EMLINK      =  31,
	ENOTEMPTY   =  66,
	EDQUOT      =  69,
	ESTALE      =  70,
	EUCLEAN     = 210,
	EBADR       = 211,
	ENOKEY      = 212,
	EIOCBQUEUED = 529,
};


/*****************
 ** linux/err.h **
 *****************/

static inline int PTR_ERR_OR_ZERO(const void *ptr)
{
	if (IS_ERR(ptr)) return PTR_ERR(ptr);
	else             return 0;
}


/********************
 ** linux/poison.h **
 ********************/

#include <lx_emul/list.h>


/*****************
 ** linux/gfp.h **
 *****************/

#include <lx_emul/gfp.h>

enum {
	__GFP_DIRECT_RECLAIM = 0x00400000u,
	__GFP_RECLAIM = (__GFP_DIRECT_RECLAIM), //|___GFP_KSWAPD_RECLAIM))

	__GFP_BITS_SHIFT = 26,

	GFP_NOIO = (__GFP_RECLAIM),
	GFP_NOFS = (__GFP_RECLAIM | __GFP_IO),
};

struct page *alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order);

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

unsigned long get_zeroed_page(gfp_t gfp_mask);
#define free_page(p) kfree((void *)p)

bool gfp_pfmemalloc_allowed(gfp_t);
unsigned long __get_free_page(gfp_t);
unsigned long __get_free_pages(gfp_t, unsigned int);
void free_pages(unsigned long, unsigned int);
void __free_pages(struct page *page, unsigned int order);
void __free_page_frag(void *addr);

bool gfpflags_allow_blocking(const gfp_t gfp_flags);

struct page_frag_cache;

void *__alloc_page_frag(struct page_frag_cache *nc,
                        unsigned int fragsz, gfp_t gfp_mask);


/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded
 */
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE-1))

enum {
	PAGE_SHIFT = 12U,
};

#define PAGE_CACHE_SHIFT PAGE_SHIFT

struct page
{
	unsigned long flags;
	struct address_space *mapping;
	struct {
		union {
			pgoff_t index;
		};
	};
	int       pfmemalloc;
	atomic_t _count;
	void     *addr;
	union {
		struct list_head lru;
	};
	unsigned long private;
} __attribute((packed));

typedef struct {
	unsigned long pgprot;
} pgprot_t;


/************************
 ** asm/ptable_types.h **
 ************************/

#define PAGE_KERNEL ((pgprot_t) { (0) } )


/*********************
 ** linux/pagemap.h **
 *********************/

enum {
	PAGE_CACHE_SIZE  = PAGE_SIZE,
	PAGE_CACHE_MASK  = PAGE_MASK,

	FGP_ACCESSED = 0x00000001,

	AS_EIO      = __GFP_BITS_SHIFT + 0,
};

static inline loff_t page_offset(struct page *page) {
	return ((loff_t)page->index) << PAGE_CACHE_SHIFT; }

extern void unlock_page(struct page *page);
void lock_page(struct page *page);

struct page *grab_cache_page_write_begin(struct address_space *mapping,
                                         pgoff_t index, unsigned flags);

#define page_cache_get(page)      get_page(page)
#define page_cache_release(page)  put_page(page)

void wait_for_stable_page(struct page *page);
void wait_on_page_writeback(struct page *page);
extern void end_page_writeback(struct page *page);

void mapping_set_error(struct address_space *mapping, int error);

int add_to_page_cache_lru(struct page *page, struct address_space *mapping, pgoff_t index, gfp_t gfp_mask);

gfp_t mapping_gfp_constraint(struct address_space *mapping, gfp_t gfp_mask);
struct page *find_or_create_page(struct address_space *mapping, pgoff_t offset, gfp_t gfp_mask);
struct page *find_get_page_flags(struct address_space *mapping, pgoff_t offset, int fgp_flags);
struct page *find_lock_page(struct address_space *mapping, pgoff_t offset);

int trylock_page(struct page *page);



/**********************
 ** asm/cacheflush.h **
 **********************/

void flush_dcache_page(struct page *page);
enum { ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE = 1 }; /* XXX */


/************************
 ** linux/cleancache.h **
 ************************/

static inline int cleancache_get_page(struct page *page) { return -1; }

struct super_block;

static inline void cleancache_init_fs(struct super_block *sb) { }


/**********************
 ** linux/mm-types.h **
 **********************/

struct vm_operations_struct;

struct vm_area_struct
{
	unsigned long vm_start;
	unsigned long vm_end;
	unsigned long vm_flags;
	const struct vm_operations_struct *vm_ops;
	unsigned long vm_pgoff;
	struct file * vm_file;
};

struct page_frag
{
	struct page *page;
	__u16        offset;
	__u16        size;
};

struct page_frag_cache
{
	bool pfmemalloc;
};


/********************
 ** linux/string.h **
 ********************/

#include <lx_emul/string.h>

char *strreplace(char *s, char old, char new);
size_t memweight(const void *ptr, size_t bytes);
extern char *skip_spaces(const char *);


/**********************
 ** linux/spinlock.h **
 **********************/

#include <lx_emul/spinlock.h>


/*******************
 ** linux/mutex.h **
 *******************/

//#include <lx_emul/mutex.h> we cannot use this here because DEFINE_MUTEX is called in a function

struct mutex
{
	unsigned dummy;
};

#define DEFINE_MUTEX(mutexname) \
	struct mutex mutexname

void mutex_init(struct mutex *);
void mutex_destroy(struct mutex *);
void mutex_lock(struct mutex *);
void mutex_unlock(struct mutex *);
int  mutex_trylock(struct mutex *);
int  mutex_is_locked(struct mutex *);


/*******************
 ** linux/rwsem.h **
 *******************/

#include <lx_emul/semaphore.h>

#define down_write_nested(sem, subclass)   down_write(sem)

int rwsem_is_locked(struct rw_semaphore *sem);


/********************
 ** linux/kernel.h **
 ********************/

#include <lx_emul/kernel.h>

#define KERN_CONT   ""

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

char *kasprintf(gfp_t gfp, const char *fmt, ...);
int kstrtouint(const char *s, unsigned int base, unsigned int *res);

#define PTR_ALIGN(p, a) ({               \
	unsigned long _p = (unsigned long)p; \
	_p = (_p + a - 1) & ~(a - 1);        \
	p = (typeof(p))_p;                   \
	p;                                   \
})

static inline u32 reciprocal_scale(u32 val, u32 ep_ro)
{
	return (u32)(((u64) val * ep_ro) >> 32);
}

int kstrtoul(const char *s, unsigned int base, unsigned long *res);
int kstrtoull(const char *s, unsigned int base, unsigned long long *res);

int strict_strtoul(const char *s, unsigned int base, unsigned long *res);
long simple_strtoul(const char *cp, char **endp, unsigned int base);
long simple_strtol(const char *,char **,unsigned int);

int hex_to_bin(char ch);

/* needed by drivers/net/wireless/iwlwifi/iwl-drv.c */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) __attribute__((format(printf, 3, 0)));
int sprintf(char *buf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int scnprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

int sscanf(const char *, const char *, ...);

/* XXX */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

#define SIZE_MAX   (~(size_t)0)
#define LLONG_MAX  ((long long)(~0ULL>>1))

#define U8_MAX   ((u8)~0U)
#define S8_MAX   ((s8)(U8_MAX>>1))
#define S8_MIN   ((s8)(-S8_MAX - 1))
#define U16_MAX  ((u16)~0U)
#define U32_MAX  ((u32)~0U)
#define S32_MAX  ((s32)(U32_MAX>>1))
#define S32_MIN  ((s32)(-S32_MAX - 1))


/*********************
 ** linux/jiffies.h **
 *********************/

#include <lx_emul/jiffies.h>

static inline unsigned int jiffies_to_usecs(const unsigned long j) { return j * JIFFIES_TICK_US; }


/******************
 ** linux/time.h **
 ******************/

#include <lx_emul/time.h>

enum {
	MSEC_PER_SEC  = 1000L,
	USEC_PER_SEC  = MSEC_PER_SEC * 1000L,
	USEC_PER_MSEC = 1000L,
};

unsigned long get_seconds(void);
void          getnstimeofday(struct timespec *);
#define do_posix_clock_monotonic_gettime(ts) ktime_get_ts(ts)

#define ktime_to_ns(kt) ((kt).tv64)

struct timespec ktime_to_timespec(const ktime_t kt);
bool ktime_to_timespec_cond(const ktime_t kt, struct timespec *ts);

int     ktime_equal(const ktime_t, const ktime_t);
s64     ktime_us_delta(const ktime_t, const ktime_t);

static inline s64 ktime_to_ms(const ktime_t kt)
{
	return kt.tv64 / NSEC_PER_MSEC;
}

static inline void ktime_get_ts(struct timespec *ts)
{
	ts->tv_sec  = jiffies * (1000/HZ);
	ts->tv_nsec = 0;
}

#define CURRENT_TIME_SEC ((struct timespec) { get_seconds(), 0 })


/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/timer.h>


/*********************
 ** linux/hrtimer.h **
 *********************/

extern int schedule_hrtimeout(ktime_t *expires, const enum hrtimer_mode mode);


/*********************
 ** linux/kconfig.h **
 *********************/

#define config_enabled(cfg) 0


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

#include <lx_emul/byteorder.h>

#define cpu_to_be64  __cpu_to_be64
#define be64_to_cpup __be64_to_cpup
#define be64_to_cpu __be64_to_cpu

#define le64_to_cpup __le64_to_cpup

#define htonl(x) __cpu_to_be32(x)
#define htons(x) __cpu_to_be16(x)
#define ntohl(x) __be32_to_cpu(x)
#define ntohs(x) __be16_to_cpu(x)


/*************************************
 ** linux/unaligned/packed_struct.h **
 *************************************/

struct __una_u16 { u16 x; } __attribute__((packed));
struct __una_u32 { u32 x; } __attribute__((packed));
struct __una_u64 { u64 x; } __attribute__((packed));


/*******************************
 ** linux/unaligned/generic.h **
 *******************************/

static inline void put_unaligned_le16(u16 val, void *p) {
	*((__le16 *)p) = cpu_to_le16(val); }

static inline void put_unaligned_be16(u16 val, void *p) {
	*((__be16 *)p) = cpu_to_be16(val); }

static inline void put_unaligned_le32(u32 val, void *p) {
	*((__le32 *)p) = cpu_to_le32(val); }

static inline u16 get_unaligned_le16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
}

static inline u32 get_unaligned_le32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}

static inline u16 get_unaligned_be16(const void *p)
{
	const __u8 *be = (__u8*)p;
	return (be[1]<<0)|(be[0]<<8);
}

void put_unaligned_le64(u64 val, void *p);

#define put_unaligned(val, ptr) ({              \
	void *__gu_p = (ptr);                       \
	switch (sizeof(*(ptr))) {                   \
	case 1:                                     \
		*(u8 *)__gu_p = (u8)(val);              \
		break;                                  \
	case 2:                                     \
		put_unaligned_le16((u16)(val), __gu_p); \
		break;                                  \
	case 4:                                     \
		put_unaligned_le32((u32)(val), __gu_p); \
		break;                                  \
	case 8:                                     \
		put_unaligned_le64((u64)(val), __gu_p); \
		break;                                  \
	}                                           \
	(void)0; })

static inline void le16_add_cpu(__le16 *var, u16 val) {
	*var = cpu_to_le16(le16_to_cpu(*var) + val); }

static inline void le32_add_cpu(__le32 *var, u32 val) {
	*var = cpu_to_le32(le32_to_cpu(*var) + val); }

static inline u32 __get_unaligned_cpu32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}


/****************************************
 ** asm-generic/bitops/const_hweight.h **
 ****************************************/

#define __const_hweight8(w)     \
	( (!!((w) & (1ULL << 0))) + \
	  (!!((w) & (1ULL << 1))) + \
	  (!!((w) & (1ULL << 2))) + \
	  (!!((w) & (1ULL << 3))) + \
	  (!!((w) & (1ULL << 4))) + \
	  (!!((w) & (1ULL << 5))) + \
	  (!!((w) & (1ULL << 6))) + \
	  (!!((w) & (1ULL << 7))) )

#define hweight8(w)  (__const_hweight8(w))

unsigned int hweight16(unsigned int w);
unsigned int hweight32(unsigned int w);
unsigned int hweight64(__u64 w);


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

#include <lx_emul/bitops.h>

unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);

static inline unsigned long find_next_bit_le(const void *addr, unsigned long size, unsigned long offset) {
	return find_next_bit((const unsigned long*)addr, size, offset); }

static inline int test_bit_le(int nr, const void *addr) {
	return test_bit(nr ^ 0, (const unsigned long*)addr); }

static inline void __set_bit_le(int nr, void *addr) {
	__set_bit(nr ^ 0, (unsigned long*)addr); }

static inline int __test_and_set_bit_le(int nr, void *addr) {
	return __test_and_set_bit(nr ^ 0, (unsigned long*)addr); }

static inline int __test_and_clear_bit_le(int nr, void *addr) {
	return __test_and_clear_bit(nr ^ 0, (unsigned long*)addr); }

static inline void __clear_bit_le(int nr, void *addr) {
	__clear_bit(nr ^ 0, (unsigned long*)addr); }

static inline unsigned long hweight_long(unsigned long w) {
	return sizeof(w) == 4 ? hweight32(w) : hweight64(w); }

#define test_and_set_bit_lock(nr, addr)  test_and_set_bit(nr, addr)

#define clear_bit_unlock(nr, addr)  \
	do {                    \
		smp_mb__before_atomic();    \
		clear_bit(nr, addr);        \
	} while (0)


/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
unsigned long find_last_bit(const unsigned long *addr, unsigned long size);

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)
#define find_next_zero_bit_le find_next_zero_bit


/*************************
 ** asm-generic/div64.h **
 *************************/

#if BITS_PER_LONG == 64

# define do_div(n,base) ({ \
	uint32_t __base = (base);          \
	uint32_t __rem;                    \
	__rem = ((uint64_t)(n)) % __base;  \
	(n) = ((uint64_t)(n)) / __base;    \
	__rem;                             \
})

#elif BITS_PER_LONG == 32

uint32_t __div64_32(uint64_t *dividend, uint32_t divisor);

/* The unnecessary pointer compare is there
 *  * to check for type safety (n must be 64bit)
 *   */
# define do_div(n,base) ({ \
	uint32_t __base = (base);                       \
	uint32_t __rem;                                 \
	(void)(((typeof((n)) *)0) == ((uint64_t *)0));  \
	if (likely(((n) >> 32) == 0)) {                 \
		__rem = (uint32_t)(n) % __base;  \
		(n) = (uint32_t)(n) / __base;    \
	} else                             \
	__rem = __div64_32(&(n), __base);  \
	__rem;                             \
})

#endif


/************************
 ** linux/page-flags.h **
 ************************/

enum pageflags
{
	PG_locked,
	PG_error,
	PG_uptodate,
	PG_dirty,
	PG_slab,
	PG_writeback,
	PG_mappedtodisk,
	PG_checked,
};

#define PageLocked(page)  test_bit(PG_locked, &(page)->flags)
#define PageDirty(page)   test_bit(PG_dirty,  &(page)->flags)
#define PageSlab(page)    test_bit(PG_slab,   &(page)->flags)
#define PageWriteback(page)  test_bit(PG_writeback,   &(page)->flags)
#define PageChecked(page)  test_bit(PG_checked,   &(page)->flags)

#define SetPageError(page)         set_bit(PG_error,        &(page)->flags)
#define SetPageDirty(page)         set_bit(PG_dirty,        &(page)->flags)
#define SetPageMappedToDisk(page)  set_bit(PG_mappedtodisk, &(page)->flags)
#define SetPageChecked(page)       set_bit(PG_checked,      &(page)->flags)

#define ClearPageError(page)     clear_bit(PG_error,    &(page)->flags)
#define ClearPageUptodate(page)  clear_bit(PG_uptodate, &(page)->flags)
#define ClearPageDirty(page)     clear_bit(PG_dirty,    &(page)->flags)
#define ClearPageChecked(page)   clear_bit(PG_checked,  &(page)->flags)

static inline int PageUptodate(struct page *page)
{
	int ret = test_bit(PG_uptodate, &(page)->flags);

	if (ret)
		smp_rmb();

	return ret;
}

static inline void SetPageUptodate(struct page *page)
{
	smp_wmb();
	set_bit(PG_uptodate, &(page)->flags);
}

static inline int page_has_private(struct page *page)
{
	return !!(page->private);
}

void set_page_writeback(struct page *page);
void set_page_writeback_keepwrite(struct page *page);


/****************
 ** linux/mm.h **
 ****************/

enum {
	VM_MIXEDMAP = 0x10000000u,
	VM_HUGEPAGE = 0x20000000u,
};

int is_vmalloc_addr(const void *x);

extern unsigned long totalram_pages;
extern unsigned long num_physpages;

static inline struct page *compound_head(struct page *page) { return page; }
static inline void *page_address(struct page *page) { return page->addr; };

void get_page(struct page *page);
void put_page(struct page *page);

struct address_space;
struct file_ra_state;
struct file;

void page_cache_sync_readahead(struct address_space *mapping, struct file_ra_state *ra,
                               struct file *filp, pgoff_t offset, unsigned long size);


#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)

struct page *virt_to_head_page(const void *x);
struct page *virt_to_page(const void *x);
struct page *vmalloc_to_page(const void *addr);

struct sysinfo;
void si_meminfo(struct sysinfo *);

#define page_private(page)      ((page)->private)
#define set_page_private(page, v)   ((page)->private = (v))

enum {
	VM_FAULT_SIGBUS = 0x0002,
	VM_FAULT_NOPAGE = 0x0100,
	VM_FAULT_LOCKED = 0x0200,
};

struct vm_fault
{
	struct page *page;
};

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	int  (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
	void (*map_pages)(struct vm_area_struct *vma, struct vm_fault *vmf);
	int  (*page_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);
};

int get_user_pages_fast(unsigned long start, int nr_pages, int write, struct page **pages);
int vm_insert_page(struct vm_area_struct *, unsigned long addr, struct page *);

bool page_is_pfmemalloc(struct page *page);

#define PAGE_ALIGNED(addr) IS_ALIGNED((unsigned long)addr, PAGE_SIZE)

extern int filemap_fault(struct vm_area_struct *, struct vm_fault *);
extern void filemap_map_pages(struct vm_area_struct *vma, struct vm_fault *vmf);
extern int filemap_write_and_wait(struct address_space *mapping);
extern int filemap_fdatawait(struct address_space *);
extern int filemap_fdatawrite_range(struct address_space *mapping, loff_t start, loff_t end);

int generic_error_remove_page(struct address_space *mapping, struct page *page);

extern void truncate_inode_pages(struct address_space *, loff_t);
extern void truncate_inode_pages_final(struct address_space *);
struct inode;
extern void truncate_pagecache(struct inode *inode, loff_t new);
void truncate_pagecache_range(struct inode *inode, loff_t offset, loff_t end);

extern int try_to_release_page(struct page * page, gfp_t gfp_mask);

struct writeback_control;
int redirty_page_for_writepage(struct writeback_control *wbc, struct page *page);

void pagecache_isize_extended(struct inode *inode, loff_t from, loff_t to);
int clear_page_dirty_for_io(struct page *page);

int __set_page_dirty_nobuffers(struct page *page);


/*********************
 ** linux/kobject.h **
 *********************/

#include <lx_emul/kobject.h>

enum kobject_action
{
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
};

struct kobj_type
{
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
	struct attribute **default_attrs;
	const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
	const void *(*namespace)(struct kobject *kobj);
};

struct kset
{
	struct list_head list;
	spinlock_t list_lock;
	struct kobject kobj;
	const struct kset_uevent_ops *uevent_ops;
};

extern int kset_register(struct kset *kset);
extern void kset_unregister(struct kset *kset);

void kobject_put(struct kobject *);
int kobject_uevent(struct kobject *, enum kobject_action);
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action, char *envp[]);

int kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype, struct kobject *parent, const char *fmt, ...);
extern void kobject_del(struct kobject *kobj);
int kobject_set_name(struct kobject *kobj, const char *name, ...);


/*********************
 ** linux/vmalloc.h **
 *********************/

extern void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot);

void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void vfree(const void *addr);


/**********************
 ** linux/highmem.h  **
 **********************/

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }
static inline void kunmap(struct page *page) { }
static inline void kunmap_atomic(void *addr) { }


/******************
 ** linux/slab.h **
 ******************/

#define ARCH_KMALLOC_MINALIGN __alignof__(unsigned long long)

enum {
	SLAB_HWCACHE_ALIGN   = 0x00002000ul,
	SLAB_CACHE_DMA       = 0x00004000ul,
	SLAB_RECLAIM_ACCOUNT = 0x00020000UL,
	SLAB_TEMPORARY       = SLAB_RECLAIM_ACCOUNT,
	SLAB_PANIC           = 0x00040000ul,
	SLAB_DESTROY_BY_RCU  = 0x00080000UL,
	SLAB_MEM_SPREAD      = 0x00100000UL,

	SLAB_LX_DMA        = 0x80000000ul,
};

void *kzalloc(size_t size, gfp_t flags);
void  kfree(const void *);
void  kzfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);
void  kvfree(const void *);

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
void   kmem_cache_destroy(struct kmem_cache *);
void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void  kmem_cache_free(struct kmem_cache *, void *);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);

static inline void *kmem_cache_alloc_node(struct kmem_cache *s, gfp_t flags, int node)
{
	return kmem_cache_alloc(s, flags);
}

#define KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
		sizeof(struct __struct), __alignof__(struct __struct),\
		(__flags), NULL)

#define ZERO_OR_NULL_PTR(x) ((x) ? 1 : 0)

/*************************
 ** linux/irq_cpustat.h **
 *************************/

int local_softirq_pending(void);


/**********************
 ** linux/irqflags.h **
 **********************/

#define local_irq_enable(a )     do { } while (0)
#define local_irq_disable()      do { } while (0)
#define local_irq_save(flags)    do { (void)flags; } while (0)
#define local_irq_restore(flags) do { (void)flags; } while (0)


/********************
 ** linux/printk.h **
 ********************/

static inline int _printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static inline int no_printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline int no_printk(const char *fmt, ...) { return 0; }

#define printk_once(fmt, ...) ({})

#define printk_ratelimit(x) (0)

#define printk_ratelimited(fmt, ...) printk(fmt, ##__VA_ARGS__)

#define pr_emerg(fmt, ...)    printk(KERN_EMERG  fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)    printk(KERN_ALERT  fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)     printk(KERN_CRIT   fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)      printk(KERN_ERR    fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...)  printk(KERN_WARN   fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)     printk(KERN_WARN   fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)   printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)     printk(KERN_INFO   fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)     printk(KERN_CONT   fmt, ##__VA_ARGS__)
/* pr_devel() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_devel(fmt, ...)    printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)    printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) no_printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) no_printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#endif

#define pr_warn_ratelimited(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice_ratelimited(fmt, ...) printk(KERN_NOTICE fmt, ##__VA_ARGS__)

enum {
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET,
};

int snprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));

static inline void print_hex_dump(const char *level, const char *prefix_str,
                                  int prefix_type, int rowsize, int groupsize,
                                  const void *buf, size_t len, bool ascii)
{
	_printk("hex_dump: ");
	size_t i;
	for (i = 0; i < len; i++) {
		_printk("%x ", ((char*)buf)[i]);
	}
	_printk("\n");
}

void hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
                        char *linebuf, size_t linebuflen, bool ascii);
void dump_stack(void);

unsigned long int_sqrt(unsigned long);


/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct; /* XXX fix in lx_emul/work.h */

#include <lx_emul/work.h>

enum {
	WQ_UNBOUND        = 1<<1,
	// WQ_MEM_RECLAIM    = 1<<3,
	WQ_HIGHPRI        = 1<<4,
	// WQ_CPU_INTENSIVE  = 1<<5,
};


/******************
 ** linux/wait.h **
 ******************/

struct wait_bit_key
{
	void            *flags;
	int         bit_nr;
#define WAIT_ATOMIC_T_BIT_NR    -1
	unsigned long       timeout;
};

struct wait_bit_queue
{
	struct wait_bit_key key;
	wait_queue_t        wait;
};

wait_queue_head_t *bit_waitqueue(void *, int);
int wake_bit_function(wait_queue_t *wait, unsigned mode, int sync, void *key);

#define __WAIT_BIT_KEY_INITIALIZER(word, bit) \
	{ .flags = word, .bit_nr = bit, }

#define DEFINE_WAIT_BIT(name, word, bit) \
	struct wait_bit_queue name = {                  \
		.key = __WAIT_BIT_KEY_INITIALIZER(word, bit),       \
		.wait   = {                     \
			.private    = current,          \
			.func       = wake_bit_function,        \
		},                          \
	}

void wake_up_bit(void *, int);
int wait_on_bit_io(unsigned long *word, int bit, unsigned mode);


/************************
 ** linux/completion.h **
 ************************/

#include <lx_emul/completion.h>

struct completion
{
	unsigned  done;
	void     *task;
};

long __wait_completion(struct completion *work, unsigned long timeout);


/**************************
 ** linux/bit_spinlock.h **
 **************************/

void bit_spin_lock(int bitnum, unsigned long *addr);
int  bit_spin_trylock(int bitnum, unsigned long *addr);
void bit_spin_unlock(int bitnum, unsigned long *addr);
int  bit_spin_is_locked(int bitnum, unsigned long *addr);

void __bit_spin_unlock(int bitnum, unsigned long *addr);


/********************
 ** linux/rwlock.h **
 ********************/

typedef unsigned rwlock_t;

void rwlock_init(rwlock_t *);

void read_lock(rwlock_t *);
void read_unlock(rwlock_t *);

void write_lock(rwlock_t *);
void write_unlock(rwlock_t *);
int  write_trylock(rwlock_t *);


/*******************
 ** linux/mount.h **
 *******************/

struct vfsmount
{
	struct dentry *mnt_root;
};

extern int mnt_want_write_file(struct file *file);
extern void mnt_drop_write_file(struct file *file);


/******************
 ** linux/path.h **
 ******************/

struct path
{
	struct vfsmount *mnt;
	struct dentry *dentry;
};

extern void path_put(const struct path *);


/***********************
 ** linux/blk_types.h **
 ***********************/

enum rq_flag_bits {
	__REQ_WRITE,
	__REQ_SYNC,
	__REQ_META,
	__REQ_PRIO,
	__REQ_NOIDLE,
	__REQ_FUA,
	__REQ_FLUSH,
};

#define REQ_WRITE   (1ULL << __REQ_WRITE)
#define REQ_SYNC    (1ULL << __REQ_SYNC)
#define REQ_META    (1ULL << __REQ_META)
#define REQ_PRIO    (1ULL << __REQ_PRIO)
#define REQ_NOIDLE  (1ULL << __REQ_NOIDLE)
#define REQ_FUA     (1ULL << __REQ_FUA)
#define REQ_FLUSH   (1ULL << __REQ_FLUSH)

struct bio_vec
{
	struct page  *bv_page;
	unsigned int  bv_len;
	unsigned int  bv_offset;
};

struct bvec_iter
{
	sector_t     bi_sector;
	unsigned int bi_size;
	unsigned int bi_idx;
	unsigned int bi_bvec_done;
};

struct bio;
typedef void (bio_end_io_t) (struct bio *);

struct bio
{
	struct block_device *bi_bdev;
	int                  bi_error;
	struct bvec_iter     bi_iter;
	bio_end_io_t        *bi_end_io;
	void                *bi_private;
	unsigned short       bi_vcnt;
	struct bio_vec      *bi_io_vec;

	unsigned short       bi_max_vecs; /* only used by us */
};

typedef unsigned int blk_qc_t;


/*****************
 ** linux/bio.h **
 *****************/

enum {
	BIO_MAX_PAGES = 256,
};

#define bio_for_each_segment_all(bvl, bio, i)               \
	for (i = 0, bvl = (bio)->bi_io_vec; i < (bio)->bi_vcnt; i++, bvl++)

struct bio *bio_alloc(gfp_t gfp_mask, unsigned int nr_iovecs);
extern int bio_add_page(struct bio *, struct page *, unsigned int,unsigned int);
extern void bio_put(struct bio *);
void bio_get(struct bio *bio);


/*************************
 ** asm-generic/ioctl.h **
 *************************/

#define _IOC_NRBITS     8
#define _IOC_TYPEBITS   8

#define _IOC_SIZEBITS  14  /* XXX is arch specific in original code but 14 matches x86 and ARM */

#define _IOC_NONE      0U
#define _IOC_WRITE     1U
#define _IOC_READ      2U

#define _IOC_NRSHIFT    0
#define _IOC_TYPESHIFT  (_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT  (_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT   (_IOC_SIZESHIFT+_IOC_SIZEBITS)

#define _IOC_TYPECHECK(t) (sizeof(t))

#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))

#define _IO(type,nr)            _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)      _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOW(type,nr,size)      _IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOWR(type,nr,size)     _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))


/*********************
 ** linux/lockdep.h **
 *********************/

enum {
	SINGLE_DEPTH_NESTING = 1,
};

struct lock_class_key { unsigned dummy; };

#define lockdep_init_map(x, y, z, w)
#define lock_map_acquire(l)
#define lock_map_release(l)


/**************************
 ** linux/migrate_mode.h **
 **************************/

enum migrate_mode { MIGRATE_ASYNC, MIGRATE_SYNC_LIGHT, MIGRATE_SYNC, };


/***********************
 ** linux/posix_acl.h **
 ***********************/

static inline int posix_acl_chmod(struct inode *inode, umode_t mode) { return 0; }


/*********************
 ** uapi/linux/fs.h **
 *********************/

enum {
	BLOCK_SIZE = (1<<10),
};

enum {
	MS_RDONLY = 1,

	MS_POSIXACL  = (1<<16),
	MS_I_VERSION = (1<<23),
	MS_LAZYTIME  = (1<<25),
};

enum {
	SEEK_SET  = 0,
	SEEK_CUR  = 1,
	SEEK_END  = 2,
	SEEK_DATA = 3,
	SEEK_HOLE = 4,
};

enum {
	RENAME_NOREPLACE = (1 << 0),
	RENAME_EXCHANGE  = (1 << 1),
	RENAME_WHITEOUT  = (1 << 2),
};

struct fstrim_range
{
	__u64 start;
	__u64 len;
	__u64 minlen;
};


/****************
 ** linux/fs.h **
 ****************/

#if BITS_PER_LONG==32
#define MAX_LFS_FILESIZE    (((loff_t)PAGE_CACHE_SIZE << (BITS_PER_LONG-1))-1) 
#elif BITS_PER_LONG==64
#define MAX_LFS_FILESIZE    ((loff_t)0x7fffffffffffffffLL)
#endif

struct inode;

enum {
	BDEVNAME_SIZE = 32,

	DIO_LOCKING     = 0x01,
	DIO_SKIP_HOLES  = 0x02,

	AOP_FLAG_NOFS = 0x0004,

	WHITEOUT_MODE = 0,
	WHITEOUT_DEV  = 0,
};

enum {
	DT_UNKNOWN  = 0,
	DT_FIFO     = 1,
	DT_CHR      = 2,
	DT_DIR      = 4,
	DT_BLK      = 6,
	DT_REG      = 8,
	DT_LNK      = 10,
	DT_SOCK     = 12,
	DT_WHT      = 14,
};

enum {
	RW_MASK  = REQ_WRITE,

	READ  = 0,
	WRITE = RW_MASK,

	READ_SYNC  = (READ | REQ_SYNC),
	WRITE_SYNC = (WRITE | REQ_SYNC | REQ_NOIDLE),
	WRITE_FUA  = (WRITE | REQ_SYNC | REQ_NOIDLE | REQ_FUA),
	WRITE_FLUSH_FUA = (WRITE | REQ_SYNC | REQ_NOIDLE | REQ_FLUSH | REQ_FUA)
};

struct address_space
{
	struct inode *host;
	unsigned long nrpages;
	pgoff_t       writeback_index;
	const struct address_space_operations *a_ops;
	unsigned long       flags;
};

struct kiocb;
struct iov_iter;
struct writeback_control;

struct address_space_operations {
	int (*writepage)(struct page *page, struct writeback_control *wbc);
	int (*readpage)(struct file *, struct page *);

	/* Write back some dirty pages from this mapping. */
	int (*writepages)(struct address_space *, struct writeback_control *);

	/* Set a page dirty.  Return true if this dirtied it */
	int (*set_page_dirty)(struct page *page);

	int (*readpages)(struct file *filp, struct address_space *mapping,
			struct list_head *pages, unsigned nr_pages);

	int (*write_begin)(struct file *, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata);
	int (*write_end)(struct file *, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata);

	/* Unfortunately this kludge is needed for FIBMAP. Don't use it */
	sector_t (*bmap)(struct address_space *, sector_t);
	void (*invalidatepage) (struct page *, unsigned int, unsigned int);
	int (*releasepage) (struct page *, gfp_t);
	void (*freepage)(struct page *);
	ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *iter, loff_t offset);
	/*
	 * migrate the contents of a page to the specified target. If
	 * migrate_mode is MIGRATE_ASYNC, it must not block.
	 */
	int (*migratepage) (struct address_space *,
			struct page *, struct page *, enum migrate_mode);
	int (*is_partially_uptodate) (struct page *, unsigned long,
			unsigned long);
	void (*is_dirty_writeback) (struct page *, bool *, bool *);
	int (*error_remove_page)(struct address_space *, struct page *);
};

enum {
	ATTR_MODE = (1 << 0),
	ATTR_UID  = (1 << 1),
	ATTR_GID  = (1 << 2),
	ATTR_SIZE = (1 << 3),
};

struct iattr
{
	unsigned int ia_valid;
	kuid_t       ia_uid;
	kgid_t       ia_gid;
	loff_t       ia_size;
};

enum {
	SB_FREEZE_COMPLETE = 4,
	SB_FREEZE_LEVELS   = (SB_FREEZE_COMPLETE - 1),
};

struct block_device
{
	struct inode     *bd_inode; /* a.o. needed by geom check */
	void             *bd_holder;
	struct hd_struct *bd_part; /* only needed for check if it is the whole disk */
	struct gendisk   *bd_disk; /* only needed for DAX check */

	/* only used by Lx but match orig Linux field */
	struct super_block *bd_super;
	unsigned            bd_block_size;

	void *lx_block; /* private Lx Block_client */
};

extern const char *bdevname(struct block_device *bdev, char *buffer);

struct sb_writers
{
	int frozen;
};

enum {
	SB_I_CGROUPWB = 0x00000001,
};

struct super_block
{
	unsigned char        s_blocksize_bits;
	unsigned long        s_blocksize;
	loff_t               s_maxbytes;
	const struct super_operations *s_op;
	const struct export_operations *s_export_op;
	unsigned long        s_flags;
	unsigned long        s_iflags;
	unsigned long        s_magic;
	struct dentry       *s_root;
	const struct xattr_handler **s_xattr;
	struct block_device *s_bdev;
	struct sb_writers    s_writers;
	char                 s_id[32];
	u8                   s_uuid[16];
	void                *s_fs_info;
	u32                  s_time_gran;
};

void sb_start_intwrite(struct super_block *sb);
void sb_end_intwrite(struct super_block *sb);
void sb_start_write(struct super_block *sb);
void sb_end_write(struct super_block *sb);
void sb_start_pagefault(struct super_block *sb);
void sb_end_pagefault(struct super_block *sb);
extern int sb_min_blocksize(struct super_block *, int);
struct buffer_head * sb_bread_unmovable(struct super_block *sb, sector_t block);
extern int sb_set_blocksize(struct super_block *, int);

enum {
	S_SYNC      =  1,
	S_NOATIME   =  2,
	S_APPEND    =  4,
	S_IMMUTABLE =  8,
	S_NOQUOTA   = 32,
	S_DIRSYNC   = 64,
	S_DAX       =  0, /* no DAX for now */

	__I_DIO_WAKEUP = 9,

	I_DIRTY_SYNC         = (1 <<  0),
	I_DIRTY_DATASYNC     = (1 <<  1),
	I_DIRTY_PAGES        = (1 <<  2),
	I_NEW                = (1 <<  3),
	I_WILL_FREE          = (1 <<  4),
	I_FREEING            = (1 <<  5),
	I_DIRTY_TIME         = (1 << 11),
	I_DIRTY_TIME_EXPIRED = (1 << 12),
};

#define __IS_FLG(inode, flg)    ((inode)->i_sb->s_flags & (flg))

#define IS_SYNC(inode)      ((inode)->i_flags & S_SYNC)
#define IS_DIRSYNC(inode)   ((inode)->i_flags & (S_SYNC|S_DIRSYNC))
#define IS_I_VERSION(inode) __IS_FLG(inode, MS_I_VERSION)
#define IS_NOQUOTA(inode)   ((inode)->i_flags & S_NOQUOTA)
#define IS_APPEND(inode)    ((inode)->i_flags & S_APPEND)
#define IS_IMMUTABLE(inode) ((inode)->i_flags & S_IMMUTABLE)
#define IS_SWAPFILE(inode)  (0)
#define IS_DAX(inode)       ((inode)->i_flags & S_DAX)

struct inode
{
	umode_t             i_mode;
	kuid_t              i_uid;
	kgid_t              i_gid;
	unsigned int        i_flags;
	const struct inode_operations   *i_op;
	struct super_block *i_sb;
	struct address_space    *i_mapping;
	unsigned long       i_ino;
	union {
		const unsigned int i_nlink;
		unsigned int __i_nlink;
	};
	dev_t               i_rdev;
	loff_t              i_size;
	struct timespec     i_atime;
	struct timespec     i_mtime;
	struct timespec     i_ctime;
	spinlock_t          i_lock;
	unsigned short      i_bytes;
	unsigned int        i_blkbits;
	blkcnt_t            i_blocks;
	unsigned long       i_state;
	struct mutex        i_mutex;
	union {
		struct rcu_head     i_rcu;
		struct hlist_head   i_dentry;
	};
	u64                 i_version;
	atomic_t            i_count;
	atomic_t            i_dio_count;
	atomic_t            i_writecount;
	struct address_space i_data;
	const struct file_operations    *i_fop;
	union {
		char            *i_link;
	};
	__u32               i_generation;
	void               *i_private;
};

static inline void inode_dio_begin(struct inode *inode) {
	atomic_inc(&inode->i_dio_count); }

static inline void inode_dio_end(struct inode *inode)
{
	if (atomic_dec_and_test(&inode->i_dio_count))
		wake_up_bit(&inode->i_state, __I_DIO_WAKEUP);
}

extern void init_special_inode(struct inode *, umode_t, dev_t);

extern void inode_set_flags(struct inode *inode, unsigned int flags, unsigned int mask);
extern int inode_change_ok(const struct inode *, struct iattr *);
void inode_inc_iversion(struct inode *inode);
extern void inode_init_once(struct inode *);
int generic_drop_inode(struct inode *inode);
extern void clear_inode(struct inode *);

void inode_dio_wait(struct inode *inode);

extern int inode_newsize_ok(const struct inode *, loff_t offset);

int sync_inode_metadata(struct inode *inode, int wait);

extern void make_bad_inode(struct inode *);

static inline void   i_size_write(struct inode *inode, loff_t i_size) { inode->i_size = i_size; }
static inline loff_t i_size_read(const struct inode *inode) { return inode->i_size; }
static inline void   i_uid_write(struct inode *inode, uid_t uid) { inode->i_uid = uid; }
static inline void   i_gid_write(struct inode *inode, gid_t gid) { inode->i_gid = gid; }
static inline uid_t  i_uid_read(const struct inode *inode) { return inode->i_uid; }
static inline gid_t  i_gid_read(const struct inode *inode) { return inode->i_gid; }

void mark_inode_dirty(struct inode *inode);

struct kstat;
struct fiemap_extent_info;

struct inode_operations {
	struct dentry * (*lookup) (struct inode *,struct dentry *, unsigned int);
	const char * (*follow_link) (struct dentry *, void **);
	struct posix_acl * (*get_acl)(struct inode *, int);
	int (*readlink) (struct dentry *, char __user *,int);
	void (*put_link) (struct inode *, void *);
	int (*create) (struct inode *,struct dentry *, umode_t, bool);
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	int (*unlink) (struct inode *,struct dentry *);
	int (*symlink) (struct inode *,struct dentry *,const char *);
	int (*mkdir) (struct inode *,struct dentry *,umode_t);
	int (*rmdir) (struct inode *,struct dentry *);
	int (*setattr) (struct dentry *, struct iattr *);
	int (*mknod) (struct inode *,struct dentry *,umode_t,dev_t);
	int (*rename2) (struct inode *, struct dentry *, struct inode *, struct dentry *, unsigned int);
	int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
	int (*setxattr) (struct dentry *, const char *,const void *,size_t,int);
	ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*removexattr) (struct dentry *, const char *);
	int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start,
			u64 len);
	int (*tmpfile) (struct inode *, struct dentry *, umode_t);
	int (*set_acl)(struct inode *, struct posix_acl *, int);
} ____cacheline_aligned;

extern struct timespec current_fs_time(struct super_block *sb);

struct dir_context;
typedef int (*filldir_t)(struct dir_context *, const char *, int, loff_t, u64, unsigned);

struct dir_context
{
	const filldir_t actor;
	loff_t pos;

	/* private Lx fields */
	char *lx_buffer;
	int   lx_count;
	int   lx_max;
	int   lx_error;
};

enum {
	FMODE_READ  = 0x01,
	FMODE_WRITE = 0x02,
	FMODE_EXCL  = 0x80,
};

#define FMODE_32BITHASH  ((__force fmode_t)0x200)
#define FMODE_64BITHASH  ((__force fmode_t)0x400)

struct file_ra_state
{
	pgoff_t start;
	unsigned int size;
	loff_t prev_pos;
};

static inline int ra_has_index(struct file_ra_state *ra, pgoff_t index)
{
	return (index >= ra->start && index <  ra->start + ra->size);
}

struct buffer_head;

typedef int (get_block_t)(struct inode *inode, sector_t iblock,
                          struct buffer_head *bh_result, int create);
typedef void (dio_submit_t)(int rw, struct bio *bio, struct inode *inode,
                            loff_t file_offset);
typedef void (dio_iodone_t)(struct kiocb *iocb, loff_t offset,
                            ssize_t bytes, void *private);


struct file
{
	struct path           f_path;
	struct inode         *f_inode;
	fmode_t               f_mode;
	unsigned int          f_flags;
	struct file_ra_state  f_ra;
	u64                   f_version;
	void                 *private_data;
	struct address_space *f_mapping;

	/* used by us to mimick iterate_dir() */
	loff_t                f_pos;
};

static inline struct inode *file_inode(const struct file *f) {
	return f->f_inode; }

static inline void file_accessed(struct file *file) { } /* no atime for us */

extern int file_update_time(struct file *file);
extern char *file_path(struct file *, char *, int);

struct pipe_inode_info;

struct file_operations
{
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iterate) (struct file *, struct dir_context *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
	long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};

extern loff_t vfs_setpos(struct file *file, loff_t offset, loff_t maxsize);

extern blk_qc_t submit_bio(int, struct bio *);

extern int generic_file_open(struct inode * inode, struct file * filp);
extern ssize_t generic_read_dir(struct file *, char __user *, size_t, loff_t *);
extern ssize_t generic_file_read_iter(struct kiocb *, struct iov_iter *);
extern ssize_t generic_file_splice_read(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
extern int generic_file_fsync(struct file *, loff_t, loff_t, int);
extern int generic_readlink(struct dentry *, char __user *, int);
extern loff_t generic_file_llseek_size(struct file *file, loff_t offset, int whence, loff_t maxsize, loff_t eof);
extern ssize_t generic_write_checks(struct kiocb *, struct iov_iter *);
extern ssize_t __generic_file_write_iter(struct kiocb *, struct iov_iter *);
extern int generic_block_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo, u64 start, u64 len, get_block_t *get_block);
extern sector_t bmap(struct inode *, sector_t);
int generic_write_sync(struct file *file, loff_t pos, loff_t count);
extern void generic_fillattr(struct inode *, struct kstat *);
extern int generic_check_addressable(unsigned, u64);

extern void page_put_link(struct inode *, void *);
extern int __page_symlink(struct inode *inode, const char *symname, int len, int nofs);
const char *simple_follow_link(struct dentry *, void **);
extern const char *page_follow_link_light(struct dentry *, void **);
extern ssize_t iter_file_splice_write(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);

extern int filemap_write_and_wait_range(struct address_space *mapping, loff_t lstart, loff_t lend);
extern int filemap_flush(struct address_space *);

void lock_two_nondirectories(struct inode *, struct inode*);
void unlock_two_nondirectories(struct inode *, struct inode*);

extern void set_nlink(struct inode *inode, unsigned int nlink);
extern void inc_nlink(struct inode *inode);
extern void drop_nlink(struct inode *inode);

extern struct inode * igrab(struct inode *);
extern void iput(struct inode *);
extern struct inode * iget_locked(struct super_block *, unsigned long);
extern void iget_failed(struct inode *);
extern void ihold(struct inode * inode);

extern struct inode *find_inode_nowait(struct super_block *, unsigned long, int (*match)(struct inode *, unsigned long, void *), void *data);

enum {
	PAGECACHE_TAG_DIRTY   = 0,
	PAGECACHE_TAG_TOWRITE = 2,
};

int mapping_tagged(struct address_space *mapping, int tag);

struct fiemap_extent_info
{
	unsigned int fi_flags;
};

int fiemap_fill_next_extent(struct fiemap_extent_info *info, u64 logical, u64 phys, u64 len, u32 flags);
int fiemap_check_flags(struct fiemap_extent_info *fieinfo, u32 fs_flags);

#define buffer_migrate_page NULL


#define FS_IOC_GETFLAGS         _IOR('f', 1, long)
#define FS_IOC_SETFLAGS         _IOW('f', 2, long)
#define FS_IOC_GETVERSION       _IOR('v', 1, long)
#define FS_IOC_SETVERSION       _IOW('v', 2, long)
#define FITRIM                  _IOWR('X', 121, struct fstrim_range)

struct file_system_type {
	const char *name;
	int fs_flags;
#define FS_REQUIRES_DEV             1
#define FS_BINARY_MOUNTDATA         2
#define FS_HAS_SUBTYPE              4
#define FS_USERNS_MOUNT             8  /* Can be mounted by userns root */
#define FS_USERNS_DEV_MOUNT        16  /* A userns mount does not imply MNT_NODEV */
#define FS_USERNS_VISIBLE          32  /* FS must already be visible */
#define FS_RENAME_DOES_D_MOVE   32768  /* FS will handle d_move() during rename() internally. */
	struct dentry *(*mount) (struct file_system_type *, int, const char *, void *);
	void (*kill_sb) (struct super_block *);
	struct module *owner;
	struct file_system_type * next;
	struct hlist_head fs_supers;

	struct lock_class_key s_lock_key;
	struct lock_class_key s_umount_key;
	struct lock_class_key s_vfs_rename_key;
	struct lock_class_key s_writers_key[SB_FREEZE_LEVELS];

	struct lock_class_key i_lock_key;
	struct lock_class_key i_mutex_key;
	struct lock_class_key i_mutex_dir_key;
};

extern struct dentry *mount_bdev(struct file_system_type *fs_type, int flags, const char *dev_name, void *data,
                                 int (*fill_super)(struct super_block *, void *, int));
void kill_block_super(struct super_block *sb);
extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);

struct seq_file;
struct shrink_control;
struct kstatfs;

struct super_operations
{
	struct inode *(*alloc_inode)(struct super_block *sb);
	void (*destroy_inode)(struct inode *);

	void (*dirty_inode) (struct inode *, int flags);
	int (*write_inode) (struct inode *, struct writeback_control *wbc);
	int (*drop_inode) (struct inode *);
	void (*evict_inode) (struct inode *);
	void (*put_super) (struct super_block *);
	int (*sync_fs)(struct super_block *sb, int wait);
	int (*freeze_super) (struct super_block *);
	int (*freeze_fs) (struct super_block *);
	int (*thaw_super) (struct super_block *);
	int (*unfreeze_fs) (struct super_block *);
	int (*statfs) (struct dentry *, struct kstatfs *);
	int (*remount_fs) (struct super_block *, int *, char *);
	void (*umount_begin) (struct super_block *);

	int (*show_options)(struct seq_file *, struct dentry *);
	int (*show_devname)(struct seq_file *, struct dentry *);
	int (*show_path)(struct seq_file *, struct dentry *);
	int (*show_stats)(struct seq_file *, struct dentry *);

	int (*bdev_try_to_free_page)(struct super_block*, struct page*, gfp_t);
	long (*nr_cached_objects)(struct super_block *,
			struct shrink_control *);
	long (*free_cached_objects)(struct super_block *,
			struct shrink_control *);
};

extern struct kobject *fs_kobj;

bool dir_emit(struct dir_context *ctx, const char *name, int namelen, u64 ino, unsigned type);
bool dir_relax(struct inode *inode);

struct buffer_head;

void dio_end_io(struct bio *bio, int error);

ssize_t __blockdev_direct_IO(struct kiocb *iocb, struct inode *inode,
                             struct block_device *bdev, struct iov_iter *iter,
                             loff_t offset, get_block_t get_block,
                             dio_iodone_t end_io, dio_submit_t submit_io,
                             int flags);
static inline ssize_t blockdev_direct_IO(struct kiocb *iocb,
                                         struct inode *inode,
                                         struct iov_iter *iter, loff_t offset,
                                         get_block_t get_block)
{
	return __blockdev_direct_IO(iocb, inode, inode->i_sb->s_bdev, iter,
			offset, get_block, NULL, NULL, DIO_LOCKING | DIO_SKIP_HOLES);
}

extern struct block_device *blkdev_get_by_dev(dev_t dev, fmode_t mode, void *holder);

extern void setattr_copy(struct inode *inode, const struct iattr *attr);

extern int sync_blockdev(struct block_device *bdev);
extern int bdev_read_only(struct block_device *);
extern void blkdev_put(struct block_device *bdev, fmode_t mode);
extern void invalidate_bdev(struct block_device *);

extern const char *__bdevname(dev_t, char *buffer);

extern int set_blocksize(struct block_device *, int);

extern int sync_filesystem(struct super_block *);


/*************************
 ** linux/buffer_head.h **
 *************************/

enum {
	MAX_BUF_PER_PAGE  = (PAGE_CACHE_SIZE / 512),
};

enum bh_state_bits
{
	BH_Uptodate,    /* Contains valid data */
	BH_Dirty,   /* Is dirty */
	BH_Lock,    /* Is locked */
	BH_Req,     /* Has been submitted for I/O */
	BH_Uptodate_Lock,/* Used by the first bh in a page, to serialise
					  * IO completion of other buffers in the page
					  *               */

	BH_Mapped,  /* Has a disk mapping */
	BH_New,     /* Disk mapping was newly created by get_block */
	BH_Async_Read,  /* Is under end_buffer_async_read I/O */
	BH_Async_Write, /* Is under end_buffer_async_write I/O */
	BH_Delay,   /* Buffer is not yet allocated on disk */
	BH_Boundary,    /* Block is followed by a discontiguity */
	BH_Write_EIO,   /* I/O error on write */
	BH_Unwritten,   /* Buffer is allocated on disk but not written */
	BH_Quiet,   /* Buffer Error Prinks to be quiet */
	BH_Meta,    /* Buffer contains metadata */
	BH_Prio,    /* Buffer should be submitted with REQ_PRIO */
	BH_Defer_Completion, /* Defer AIO completion to workqueue */

	BH_PrivateStart,
};

struct buffer_head;

typedef void (bh_end_io_t)(struct buffer_head *bh, int uptodate);

struct buffer_head
{
	unsigned long         b_state;
	struct buffer_head   *b_this_page;
	struct page          *b_page;
	sector_t              b_blocknr;
	size_t                b_size;
	char                 *b_data;
	struct block_device  *b_bdev;
	bh_end_io_t          *b_end_io;
	void                 *b_private;
	struct list_head      b_assoc_buffers;
	struct address_space *b_assoc_map;
	atomic_t              b_count;
};


void brelse(struct buffer_head *bh);
void bforget(struct buffer_head *bh);

#define BUFFER_FNS(bit, name)                       \
static inline void set_buffer_##name(struct buffer_head *bh)        \
{                                   \
	set_bit(BH_##bit, &(bh)->b_state);              \
}                                   \
static inline void clear_buffer_##name(struct buffer_head *bh)      \
{                                   \
	clear_bit(BH_##bit, &(bh)->b_state);                \
}                                   \
static inline int buffer_##name(const struct buffer_head *bh)       \
{                                   \
	return test_bit(BH_##bit, &(bh)->b_state);          \
}

#define TAS_BUFFER_FNS(bit, name)                   \
static inline int test_set_buffer_##name(struct buffer_head *bh)    \
{                                   \
	return test_and_set_bit(BH_##bit, &(bh)->b_state);      \
}                                   \
static inline int test_clear_buffer_##name(struct buffer_head *bh)  \
{                                   \
	return test_and_clear_bit(BH_##bit, &(bh)->b_state);        \
}


BUFFER_FNS(Uptodate, uptodate)
BUFFER_FNS(Dirty, dirty)
TAS_BUFFER_FNS(Dirty, dirty)
BUFFER_FNS(Lock, locked)
BUFFER_FNS(Req, req)
BUFFER_FNS(Mapped, mapped)
BUFFER_FNS(New, new)
BUFFER_FNS(Async_Write, async_write)
BUFFER_FNS(Delay, delay)
BUFFER_FNS(Write_EIO, write_io_error)
BUFFER_FNS(Unwritten, unwritten)
BUFFER_FNS(Meta, meta)
BUFFER_FNS(Prio, prio)
BUFFER_FNS(Defer_Completion, defer_completion)

static inline bool page_has_buffers(struct page *page) {
	return page->private ? true : false; }

static inline struct buffer_head *page_buffers(struct page *page) {
	return (struct buffer_head*)page->private; }

static inline void get_bh(struct buffer_head *bh) {
	atomic_inc(&bh->b_count); }

void put_bh(struct buffer_head *bh);
// static inline void put_bh(struct buffer_head *bh)
// {
// 	smp_mb__before_atomic();
// 	atomic_dec(&bh->b_count);
// }

static inline void map_bh(struct buffer_head *bh, struct super_block *sb, sector_t block)
{
	set_buffer_mapped(bh);
	bh->b_bdev = sb->s_bdev;
	bh->b_blocknr = block;
	bh->b_size = sb->s_blocksize;
}

void mark_buffer_dirty(struct buffer_head *bh);
void set_bh_page(struct buffer_head *bh, struct page *page, unsigned long offset);
void create_empty_buffers(struct page *, unsigned long, unsigned long b_state);
void end_buffer_write_sync(struct buffer_head *bh, int uptodate);
void end_buffer_read_sync(struct buffer_head *bh, int uptodate);
void write_dirty_buffer(struct buffer_head *bh, int rw);
struct buffer_head *__getblk(struct block_device *bdev, sector_t block, unsigned size);
struct buffer_head *__find_get_block(struct block_device *bdev, sector_t block, unsigned size);
void invalidate_inode_buffers(struct inode *);

struct inode;

void mark_buffer_dirty_inode(struct buffer_head *bh, struct inode *inode);
int sync_dirty_buffer(struct buffer_head *bh);
int try_to_free_buffers(struct page *);

struct address_space;

int sync_mapping_buffers(struct address_space *mapping);

int block_is_partially_uptodate(struct page *page, unsigned long from, unsigned long count);

struct super_block;

/* some belong to fs.h */
extern int inode_needs_sync(struct inode *inode);
extern struct inode *new_inode(struct super_block *sb);
extern void inode_init_owner(struct inode *inode, const struct inode *dir, umode_t mode);
extern int insert_inode_locked(struct inode *);
extern void clear_nlink(struct inode *inode);
extern void unlock_new_inode(struct inode *);
extern int is_bad_inode(struct inode *);
extern bool inode_owner_or_capable(const struct inode *inode);

struct buffer_head * sb_getblk(struct super_block *sb, sector_t block);
struct buffer_head * sb_getblk_gfp(struct super_block *sb, sector_t block, gfp_t gfp);
void sb_breadahead(struct super_block *sb, sector_t block);
struct buffer_head * sb_find_get_block(struct super_block *sb, sector_t block);

void lock_buffer(struct buffer_head *bh);
void unlock_buffer(struct buffer_head *bh);

int submit_bh(int, struct buffer_head *);
void wait_on_buffer(struct buffer_head *bh);
struct buffer_head *getblk_unmovable(struct block_device *bdev, sector_t block, unsigned size);

int bh_uptodate_or_lock(struct buffer_head *bh);
int bh_submit_read(struct buffer_head *bh);

struct buffer_head * sb_bread(struct super_block *sb, sector_t block);

void block_invalidatepage(struct page *page, unsigned int offset, unsigned int length);
int block_write_end(struct file *, struct address_space *, loff_t, unsigned, unsigned, struct page *, void *);
int __block_write_begin(struct page *page, loff_t pos, unsigned len, get_block_t *get_block);
int block_commit_write(struct page *page, unsigned from, unsigned to);
int block_page_mkwrite(struct vm_area_struct *vma, struct vm_fault *vmf, get_block_t get_block);
int block_page_mkwrite_return(int err);
int block_read_full_page(struct page*, get_block_t*);
int generic_write_end(struct file *, struct address_space *, loff_t, unsigned, unsigned, struct page *, void *);
sector_t generic_block_bmap(struct address_space *, sector_t, get_block_t *);

void unmap_underlying_metadata(struct block_device *bdev, sector_t block);

#define bh_offset(bh)  ((unsigned long)(bh)->b_data & ~PAGE_MASK)

void ll_rw_block(int, int, struct buffer_head * bh[]);

void __bforget(struct buffer_head *);
void __brelse(struct buffer_head *);
struct buffer_head * __bread(struct block_device *bdev, sector_t block, unsigned size);

struct buffer_head *alloc_buffer_head(gfp_t gfp_flags);
void free_buffer_head(struct buffer_head * bh);

int __sync_dirty_buffer(struct buffer_head *bh, int rw);


/*******************
 ** linux/sched.h **
 *******************/

enum {
	TASK_COMM_LEN = 16,

	TASK_INTERRUPTIBLE   = 1,
	TASK_UNINTERRUPTIBLE = 2,

	PF_MEMALLOC = 0x00000800,
	PF_KSWAPD   = 0x00040000
};

struct task_struct
{
	unsigned int flags;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	void *journal_info;
	struct io_context *io_context;
};

void schedule(void);
int cond_resched(void);
int cond_resched_lock(spinlock_t*);
extern signed long schedule_timeout_interruptible(signed long timeout);
extern signed long schedule_timeout_uninterruptible(signed long timeout);

extern int wake_up_process(struct task_struct *tsk);

static inline int fatal_signal_pending(struct task_struct *p) { return 0; }
static inline bool need_resched(void) { return 0; }
static inline int spin_needbreak(spinlock_t *lock) { return 0; }

void set_current_state(int);


/*********************
 ** linux/freezer.h **
 *********************/

static inline void set_freezable(void) {}
static inline bool freezing(struct task_struct *p) { return false; }
static inline bool try_to_freeze(void) { return false; }


/**********************
 ** linux/exportfs.h **
 **********************/

struct fid;

struct export_operations {
	struct dentry * (*fh_to_dentry)(struct super_block *sb, struct fid *fid,
			int fh_len, int fh_type);
	struct dentry * (*fh_to_parent)(struct super_block *sb, struct fid *fid,
			int fh_len, int fh_type);
	struct dentry * (*get_parent)(struct dentry *child);
};

extern struct dentry *generic_fh_to_dentry(struct super_block *sb, struct fid *fid, int fh_len, int fh_type,
                                           struct inode *(*get_inode) (struct super_block *sb, u64 ino, u32 gen));
extern struct dentry *generic_fh_to_parent(struct super_block *sb, struct fid *fid, int fh_len, int fh_type,
                                           struct inode *(*get_inode) (struct super_block *sb, u64 ino, u32 gen));


/***************************
 ** asm-generic/current.h **
 ***************************/

/* use pointer instead of macro fasching */
extern struct task_struct *current;


/*******************
 ** crypto/hash.h **
 *******************/

struct crypto_shash
{
	unsigned dummy;
};

struct shash_desc
{
	struct crypto_shash *tfm;
	u32 flags;
};

struct crypto_shash *crypto_alloc_shash(const char *alg_name, u32 type, u32 mask);
void crypto_free_shash(struct crypto_shash *tfm);
unsigned int crypto_shash_descsize(struct crypto_shash *tfm);
int crypto_shash_update(struct shash_desc *desc, const u8 *data, unsigned int len);


/**********************
 ** linux/rcupdate.h **
 **********************/

#define rcu_assign_pointer(p, v) p = v

void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *head));


/***********************
 ** uapi/linux/stat.h **
 ***********************/

/* needed by ext4.h */
#include <uapi/linux/stat.h>


/******************
 ** linux/stat.h **
 ******************/

#define S_IRWXUGO  (S_IRWXU|S_IRWXG|S_IRWXO)
#define S_IRUGO    (S_IRUSR|S_IRGRP|S_IROTH)

struct kstat
{
	loff_t              size;
	unsigned long long  blocks;
};


/****************************
 ** linux/percpu_counter.h **
 ****************************/

struct percpu_counter
{
	s64 count;
};

static inline int percpu_counter_init(struct percpu_counter *fbc, s64 amount, gfp_t gfp)
{
	fbc->count = amount;
	return 0;
}

static inline int percpu_counter_initialized(struct percpu_counter *fbc) {
	return 1; }

static inline void percpu_counter_destroy(struct percpu_counter *fbc) { }

static inline void percpu_counter_add(struct percpu_counter *fbc, s64 amount) {
	fbc->count += amount; }

static inline void percpu_counter_sub(struct percpu_counter *fbc, s64 amount) {
	percpu_counter_add(fbc, -amount); }

static inline s64 percpu_counter_read(struct percpu_counter *fbc) {
	return fbc->count; }

static inline s64 percpu_counter_read_positive(struct percpu_counter *fbc) {
	return fbc->count; }

static inline s64 percpu_counter_sum_positive(struct percpu_counter *fbc) {
	return percpu_counter_read_positive(fbc); }

static inline s64 percpu_counter_sum(struct percpu_counter *fbc) {
	return percpu_counter_read(fbc); }

static inline void percpu_counter_inc(struct percpu_counter *fbc) {
	percpu_counter_add(fbc, 1); }

static inline void percpu_counter_dec(struct percpu_counter *fbc) {
	percpu_counter_add(fbc, -1); }


/******************
 ** linux/file.h **
 ******************/

struct fd {
	struct file *file;
	unsigned int flags;
};

struct fd fdget(unsigned int fd);
void fdput(struct fd fd);


/**************************
 ** linux/seq_file.h **
 **************************/

#define SEQ_START_TOKEN ((void *)1)

struct seq_file
{
	void *private;
};

struct seq_operations
{
	void * (*start)(struct seq_file *m, loff_t *pos);
	void   (*stop) (struct seq_file *m, void *v);
	void * (*next) (struct seq_file *m, void *v, loff_t *pos);
	int    (*show) (struct seq_file *m, void *v);
};

int seq_open(struct file *, const struct seq_operations *);
ssize_t seq_read(struct file *, char __user *, size_t, loff_t *);
loff_t seq_lseek(struct file *, loff_t, int);
int seq_release(struct inode *, struct file *);
void seq_printf(struct seq_file *m, const char *fmt, ...);
void seq_puts(struct seq_file *m, const char *s);

int single_open(struct file *, int (*)(struct seq_file *, void *), void *);
int single_release(struct inode *, struct file *);


/**********************
 ** linux/shrinker.h **
 **********************/

enum {
	DEFAULT_SEEKS  = 2,
};

struct shrink_control
{
	gfp_t gfp_mask;
	unsigned long nr_to_scan;
};

struct shrinker
{
	unsigned long (*count_objects)(struct shrinker *,
	                               struct shrink_control *);
	unsigned long (*scan_objects)(struct shrinker *,
	                              struct shrink_control *);
	int seeks;
};

extern int register_shrinker(struct shrinker *);
extern void unregister_shrinker(struct shrinker *);


/********************
 ** linux/dcache.h **
 ********************/

struct qstr
{
	union
	{
		struct
		{
			u32 hash;
			u32 len; /* XXX little endian only */
		};
		u64 hash_len;
	};
	const unsigned char *name;
};

#define QSTR_INIT(n,l) { { { .len = l } }, .name = n }

struct dentry
{
	struct dentry *d_parent;
	struct qstr d_name;
	struct inode *d_inode;
	struct super_block *d_sb;
};

static inline struct inode *d_inode(const struct dentry *dentry)
{
	return dentry->d_inode;
}

extern struct dentry *d_find_any_alias(struct inode *inode);
extern void dput(struct dentry *);
extern char *d_path(const struct path *, char *, int);
extern void d_instantiate(struct dentry *, struct inode *);
extern struct dentry * d_splice_alias(struct inode *, struct dentry *);
extern struct dentry * d_obtain_alias(struct inode *);
extern void d_tmpfile(struct dentry *, struct inode *);
extern struct dentry * d_make_root(struct inode *);

unsigned long vfs_pressure_ratio(unsigned long val);


/************************
 ** uapi/linux/quota.h **
 ************************/

enum {
	QFMT_VFS_OLD = 1,
	QFMT_VFS_V0  = 2,
	QFMT_VFS_V1  = 4,
};


/*******************
 ** linux/quota.h **
 *******************/

typedef long long qsize_t;

enum {
	DQUOT_USAGE_ENABLED  = (1 << 0),
	DQUOT_LIMITS_ENABLED = (1 << 0),
};


/***********************
 ** linux/ratelimit.h **
 ***********************/

struct ratelimit_state
{
	int     interval;
	int     burst;
};

#define DEFINE_RATELIMIT_STATE(name, interval_init, burst_init) \
	struct ratelimit_state name

extern int ___ratelimit(struct ratelimit_state *rs, const char *func);
#define __ratelimit(state) ___ratelimit(state, __func__)

void ratelimit_state_init(struct ratelimit_state *rs, int interval, int burst);


/****************************
 ** linux/user_namespace.h **
 ****************************/

extern struct user_namespace init_user_ns;


/********************
 ** linux/uidgid.h **
 ********************/

#define GLOBAL_ROOT_GID 0

bool uid_eq(kuid_t, kuid_t);
bool gid_eq(kgid_t left, kgid_t right);

#define KUIDT_INIT(value) (kuid_t){ value }
#define KGIDT_INIT(value) (kgid_t){ value }

static inline kuid_t make_kuid(struct user_namespace *from, uid_t uid) {
	return KUIDT_INIT(uid); }

static inline kgid_t make_kgid(struct user_namespace *from, gid_t gid) {
	return KGIDT_INIT(gid); }

static inline bool uid_valid(kuid_t uid) {
	return uid != (uid_t) -1; }

static inline bool gid_valid(kgid_t gid) {
	return gid != (gid_t) -1; }

static inline uid_t from_kuid_munged(struct user_namespace *to, kuid_t kuid) {
	return kuid; }

static inline gid_t from_kgid_munged(struct user_namespace *to, kgid_t kgid) {
	return kgid; }


/******************
 ** linux/cred.h **
 ******************/

static inline kuid_t current_fsuid() { return 0; }
static inline int in_group_p(kgid_t grp) { return 1; }

#define current_user_ns()   (&init_user_ns)


/*****************************
 ** uapi/linux/capability.h **
 *****************************/

enum {
	CAP_LINUX_IMMUTABLE =  9,
	CAP_SYS_ADMIN       = 21,
	CAP_SYS_RESOURCE    = 24,
};


/************************
 ** linux/capability.h **
 ************************/

static inline bool capable(int cap) { return true; }


/*************************
 ** uapi/linux/fiemap.h **
 *************************/

enum {
	FIEMAP_EXTENT_LAST        = 0x00000001u,
	FIEMAP_EXTENT_UNKNOWN     = 0x00000002u,
	FIEMAP_EXTENT_DELALLOC    = 0x00000004u,
	FIEMAP_EXTENT_NOT_ALIGNED = 0x00000100u,
	FIEMAP_EXTENT_DATA_INLINE = 0x00000200u,
	FIEMAP_EXTENT_UNWRITTEN   = 0x00000800u,
};

enum {
	FIEMAP_FLAG_SYNC  = 0x00000001u,
	FIEMAP_FLAG_XATTR = 0x00000002u,
	FIEMAP_FLAG_CACHE = 0x00000004u,
};


/******************************
 ** linux/backing-dev-defs.h **
 ******************************/

enum {
	BLK_RW_ASYNC = 0,
};

struct backing_dev_info
{
	struct device *dev;
};


/*************************
 ** linux/backing-dev.h **
 *************************/

long congestion_wait(int sync, long timeout);

struct backing_dev_info *inode_to_bdi(struct inode *inode);


/*************************
 ** uapi/linux/falloc.h **
 *************************/

enum {
	FALLOC_FL_KEEP_SIZE      = 0x01u,
	FALLOC_FL_PUNCH_HOLE     = 0x02u,
	FALLOC_FL_COLLAPSE_RANGE = 0x08u,
	FALLOC_FL_ZERO_RANGE     = 0x10u,
	FALLOC_FL_INSERT_RANGE   = 0x20u,
};


/******************************
 ** uapi/asm-generic/fcntl.h **
 ******************************/

enum {
	O_SYNC = 04000000,
};


/*****************
 ** linux/aio.h **
 *****************/

enum {
	IOCB_EVENTFD = (1 << 0),
	IOCB_APPEND  = (1 << 1),
	IOCB_DIRECT  = (1 << 2),
};

struct kiocb
{
	struct file *ki_filp;
	loff_t       ki_pos;
	void       (*ki_complete)(struct kiocb *iocb, long ret, long ret2);
	void        *private;
	int          ki_flags;
};

static inline bool is_sync_kiocb(struct kiocb *kiocb) {
	return kiocb->ki_complete == NULL; }


/********************
 ** linux/blkdev.h **
 ********************/

struct blk_plug {
	struct list_head list;    /* requests */
	struct list_head mq_list; /* blk-mq requests */
	struct list_head cb_list; /* md requires an unplug callback */
};

struct queue_limits
{
	unsigned int discard_granularity;
};

struct request_queue
{
	struct queue_limits limits;
};

struct block_device_operations {
	long (*direct_access)(struct block_device *, sector_t, void **, unsigned long *pfn);
};

extern void blk_start_plug(struct blk_plug *);
extern void blk_finish_plug(struct blk_plug *);
extern int blkdev_issue_flush(struct block_device *, gfp_t, sector_t *);

int sb_issue_zeroout(struct super_block *sb, sector_t block, sector_t nr_blocks, gfp_t gfp_mask);
int sb_issue_discard(struct super_block *sb, sector_t block, sector_t nr_blocks, gfp_t gfp_mask, unsigned long flags);

struct request_queue *bdev_get_queue(struct block_device *bdev);

bool blk_queue_discard(struct request_queue *);
unsigned short bdev_logical_block_size(struct block_device *bdev);

static inline unsigned int blksize_bits(unsigned int size)
{
	unsigned int bits = 8;
	do { 
		bits++;
		size >>= 1;
	} while (size > 256);
	return bits;
}


/*********************
 ** linux/pagevec.h **
 *********************/

enum {
	PAGEVEC_SIZE = 14,
};

struct pagevec
{
	unsigned long nr;
	unsigned long cold;
	struct page *pages[PAGEVEC_SIZE];
};

static inline void pagevec_init(struct pagevec *pvec, int cold)
{
	pvec->nr = 0;
	pvec->cold = cold;
}

unsigned pagevec_lookup(struct pagevec *pvec, struct address_space *mapping,
                        pgoff_t start, unsigned nr_pages);
unsigned pagevec_lookup_tag(struct pagevec *pvec, struct address_space *mapping,
                            pgoff_t *index, int tag, unsigned nr_pages);

void pagevec_release(struct pagevec *pvec);


/************************
 ** uapi/linux/xattr.h **
 ************************/

enum {
	XATTR_CREATE  = 0x1,
	XATTR_REPLACE = 0x2,
};

#define XATTR_TRUSTED_PREFIX "trusted."
#define XATTR_TRUSTED_PREFIX_LEN (sizeof(XATTR_TRUSTED_PREFIX) - 1)

#define XATTR_USER_PREFIX "user."
#define XATTR_USER_PREFIX_LEN (sizeof(XATTR_USER_PREFIX) - 1)


/*******************
 ** linux/xattr.h **
 *******************/

struct xattr_handler
{
	const char *prefix;
	size_t (*list)(const struct xattr_handler *, struct dentry *dentry,
	               char *list, size_t list_size, const char *name, size_t name_len);
	int (*get)(const struct xattr_handler *, struct dentry *dentry,
	           const char *name, void *buffer, size_t size);
	int (*set)(const struct xattr_handler *, struct dentry *dentry,
	           const char *name, const void *buffer, size_t size, int flags);
};

int generic_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
ssize_t generic_getxattr(struct dentry *dentry, const char *name, void *buffer, size_t size);
int generic_removexattr(struct dentry *dentry, const char *name);


/*****************
 ** linux/uio.h **
 *****************/

enum { UIO_MAXIOV = 1024 };

struct iovec
{
	void           *iov_base;
	__kernel_size_t iov_len;
};

struct kvec
{
	void  *iov_base;
	size_t iov_len;
};

struct iov_iter {
	int type;
	size_t iov_offset;
	size_t count;
	union {
		const struct iovec *iov;
		const struct kvec *kvec;
		const struct bio_vec *bvec;
	};   
	unsigned long nr_segs;
};

#define iov_iter_rw(i) ((0 ? (struct iov_iter *)0 : (i))->type & RW_MASK)

static inline size_t iov_iter_count(struct iov_iter *i) {
	return i->count; }

static inline void iov_iter_truncate(struct iov_iter *i, u64 count) {
	if (i->count > count) i->count = count; }


unsigned long iov_iter_alignment(const struct iov_iter *i);


/***********************
 ** linux/writeback.h **
 ***********************/

enum wb_reason {
	WB_REASON_FS_FREE_SPACE,
};

enum writeback_sync_modes {
	WB_SYNC_NONE,
	WB_SYNC_ALL,
};

struct writeback_control
{
	long nr_to_write;
	loff_t range_start;
	loff_t range_end;
	enum writeback_sync_modes sync_mode;

	unsigned tagged_writepages:1;
	unsigned range_cyclic:1;
	unsigned for_sync:1;
};

static inline void wbc_init_bio(struct writeback_control *wbc, struct bio *bio) { }
static inline void wbc_account_io(struct writeback_control *wbc, struct page *page, size_t bytes) { }

typedef int (*writepage_t)(struct page *page, struct writeback_control *wbc, void *data);

int write_cache_pages(struct address_space *mapping, struct writeback_control *wbc, writepage_t writepage, void *data);
void tag_pages_for_writeback(struct address_space *mapping, pgoff_t start, pgoff_t end);
bool try_to_writeback_inodes_sb(struct super_block *, enum wb_reason reason);

int generic_writepages(struct address_space *mapping, struct writeback_control *wbc);


/*********************
 ** linux/utsname.h **
 *********************/

struct new_utsname
{
	char nodename[64 + 1];
};

struct new_utsname *init_utsname(void);


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align);

#define alloc_percpu(type)      \
	(typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))

#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu);(typeof(*(ptr)) *)(ptr); })
#define raw_cpu_ptr(ptr) per_cpu_ptr(ptr, 0)

#define free_percpu(pdata) kfree(pdata)


/*********************
 ** linux/cpumask.h **
 *********************/

extern const struct cpumask *const cpu_possible_mask;

#define nr_cpu_ids 1

#define for_each_cpu(cpu, mask)                 \
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)

#define for_each_possible_cpu(cpu) for_each_cpu((cpu), cpu_possible_mask)


/*********************
 ** linux/rculist.h **
 *********************/

#define list_for_each_entry_rcu(pos, head, member) \
	list_for_each_entry(pos, head, member)

#define list_add_rcu  list_add
#define list_del_rcu  list_del
#define list_add_tail_rcu  list_add_tail


/********************
 ** linux/parser.h **
 ********************/

struct match_token
{
	int token;
	const char *pattern;
};

typedef struct match_token match_table_t[];

enum {
	MAX_OPT_ARGS = 3,
};

typedef struct
{
	char *from;
	char *to;
} substring_t;

int match_int(substring_t *, int *result);
char *match_strdup(const substring_t *);
int match_token(char *, const match_table_t table, substring_t args[]);


/*******************
 ** linux/namei.h **
 *******************/

enum {
	LOOKUP_FOLLOW = 0x0001,
};

static inline void nd_terminate_link(void *name, size_t len, size_t maxlen) {
	((char *) name)[min(len, maxlen)] = '\0'; }

extern int kern_path(const char *, unsigned, struct path *);


/********************
 ** linux/ioprio.h **
 ********************/

enum {
	IOPRIO_CLASS_BE,
};

#define IOPRIO_CLASS_SHIFT  (13)
#define IOPRIO_PRIO_VALUE(class, data)  (((class) << IOPRIO_CLASS_SHIFT) | data)

extern int set_task_ioprio(struct task_struct *task, int ioprio);


/*****************************
 ** linux/blockgroup_lock.h **
 *****************************/

enum {
	NR_BG_LOCKS = 1,
};

struct bgl_lock {
	spinlock_t lock;
};

struct blockgroup_lock {
	struct bgl_lock locks[NR_BG_LOCKS];
};

static inline void bgl_lock_init(struct blockgroup_lock *bgl)
{
	int i;

	for (i = 0; i < NR_BG_LOCKS; i++)
		spin_lock_init(&bgl->locks[i].lock);
}

static inline spinlock_t *bgl_lock_ptr(struct blockgroup_lock *bgl, unsigned int block_group)
{
	return &bgl->locks[(block_group) & (NR_BG_LOCKS-1)].lock;
}


/*******************
 ** linux/genhd.h **
 *******************/

struct disk_stats
{
	unsigned long sectors[2];
};

struct hd_struct
{
	struct disk_stats dkstats;
};

#define part_stat_read(part, field) ((part)->dkstats.field)

struct gendisk
{
	const struct block_device_operations *fops;
};


/*******************
 ** linux/magic.h **
 *******************/

enum {
	EXT4_SUPER_MAGIC = 0xEF53,
};


/*******************
 ** linux/sysfs.h **
 *******************/

struct attribute
{
	const char *name;
	umode_t     mode;
};

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *); 
	ssize_t (*store)(struct kobject *, struct attribute *, const char *, size_t);
};


/***********************
 ** linux/iocontext.h **
 ***********************/

struct io_context
{
	unsigned short ioprio;
};


/********************
 ** linux/statfs.h **
 ********************/

struct kstatfs
{
	long f_type;
	long f_bsize;
	u64 f_blocks;
	u64 f_bfree;
	u64 f_bavail;
	u64 f_files;
	u64 f_ffree;
	__kernel_fsid_t f_fsid;
	long f_namelen;
};


/*********************
 ** linux/stringify **
 *********************/

#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)


// /*********************
//  ** linux/mbcache.h **
//  *********************/

// struct mb_cache_entry
// {
// 	sector_t            e_block;
// };

// struct mb_cache
// {
// 	unsigned dummy;
// };

// struct mb_cache *mb_cache_create(const char *, int);
// void mb_cache_shrink(struct block_device *);
// void mb_cache_destroy(struct mb_cache *);

// struct mb_cache_entry *mb_cache_entry_alloc(struct mb_cache *, gfp_t);
// int mb_cache_entry_insert(struct mb_cache_entry *, struct block_device *, sector_t, unsigned int);
// struct mb_cache_entry *mb_cache_entry_find_first(struct mb_cache *cache, struct block_device *,  unsigned int);
// struct mb_cache_entry *mb_cache_entry_find_next(struct mb_cache_entry *, struct block_device *,  unsigned int);
// struct mb_cache_entry *mb_cache_entry_get(struct mb_cache *, struct block_device *, sector_t);
// void mb_cache_entry_release(struct mb_cache_entry *);
// void mb_cache_entry_free(struct mb_cache_entry *);


/*********************
 ** linux/proc_fs.h **
 *********************/

static inline void *PDE_DATA(const struct inode *inode) { return NULL;}
static inline struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *parent) {return NULL;}
#define proc_create_data(name, mode, parent, proc_fops, data) ({NULL;})
#define remove_proc_entry(name, parent) do {} while (0)


/**********************
 ** linux/quotaops.h **
 **********************/

static inline int dquot_initialize(struct inode *inode) { return 0; }
static inline void dquot_free_inode(struct inode *inode) { }
static inline void dquot_drop(struct inode *inode) { }
static inline int dquot_alloc_inode(struct inode *inode) { return 0; }
static inline int dquot_transfer(struct inode *inode, struct iattr *iattr) { return 0; }
static inline int dquot_disable(struct super_block *sb, int type, unsigned int flags) { return 0; }
static inline int dquot_writeback_dquots(struct super_block *sb, int type) { return 0; }
static inline int dquot_suspend(struct super_block *sb, int type) { return 0; }
static inline int sb_any_quota_loaded(struct super_block *sb) { return 0; }

#define dquot_file_open     generic_file_open

int dquot_alloc_block(struct inode *inode, qsize_t nr);
int dquot_claim_block(struct inode *inode, qsize_t nr);
int dquot_reserve_block(struct inode *inode, qsize_t nr);
void dquot_alloc_block_nofail(struct inode *inode, qsize_t nr);
void dquot_free_block(struct inode *inode, qsize_t nr);
void dquot_release_reservation_block(struct inode *inode, qsize_t nr);

bool is_quota_modification(struct inode *inode, struct iattr *ia);


/************************
 ** linux/cryptohash.h **
 ************************/

__u32 half_md4_transform(__u32 buf[4], __u32 const in[8]);


/********************
 ** linux/math64.h **
 ********************/

u64 div_u64(u64 dividend, u32 divisor);
u64 div64_u64(u64 dividend, u64 divisor);


/*****************
 ** linux/dax.h **
 *****************/

ssize_t dax_do_io(struct kiocb *, struct inode *, struct iov_iter *, loff_t,
                  get_block_t, dio_iodone_t, int flags);

int dax_zero_page_range(struct inode *, loff_t from, unsigned len, get_block_t);


/********************
 ** linux/random.h **
 ********************/

extern void get_random_bytes(void *buf, int nbytes);

u32 prandom_u32(void);
void generate_random_uuid(unsigned char uuid_out[16]);


/******************
 ** linux/log2.h **
 ******************/

static inline bool is_power_of_2(unsigned long n) {
	return (n != 0 && ((n & (n - 1)) == 0)); }

int ilog2(u32 n);
// int roundup_pow_of_two(u32 n);
static inline unsigned long roundup_pow_of_two(unsigned long n) {
	return 1UL << fls_long(n - 1); }


#define order_base_2(n) ilog2(roundup_pow_of_two(n))


/*********************
 ** linux/highmem.h **
 *********************/

void zero_user_segment(struct page *page, unsigned start, unsigned end);
void zero_user(struct page *page, unsigned start, unsigned size);


/*********************
 ** linux/uaccess.h **
 *********************/

#define put_user(x, ptr)  ({ *(ptr) =  (x); 0; })
#define get_user(x, ptr)  ({  x     = *ptr; 0; })

static inline long copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

static inline long copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


/*********************
 ** linux/kthread.h **
 *********************/

void *kthread_run(int (*threadfn)(void *), void *data, char const *name, ...);
int kthread_stop(struct task_struct *k);
bool kthread_should_stop(void);


/**********************
 ** linux/prefetch.h **
 **********************/

#define prefetch(x)  __builtin_prefetch(x)
#define prefetchw(x) __builtin_prefetch(x,1)


/********************
 ** linux/kdev_t.h **
 ********************/

dev_t old_decode_dev(u16 val);
dev_t new_decode_dev(u32 dev);
u16 old_encode_dev(dev_t dev);
u32 new_encode_dev(dev_t dev);

bool old_valid_dev(dev_t dev);


/*********************
 ** linux/highuid.h **
 *********************/

#define fs_high2lowuid(uid) (0)
#define fs_high2lowgid(gid) (0)

#define low_16_bits(x)  ((x) & 0xFFFF)
#define high_16_bits(x) (((x) & 0xFFFF0000) >> 16)


/*************************
 ** asm-generic/timex.h **
 *************************/

typedef unsigned long cycles_t;
static inline cycles_t get_cycles(void) { return 0; }


/*********************
 ** linux/rcutree.h **
 *********************/

void rcu_barrier(void);
static inline void rcu_read_lock(void) { }
static inline void rcu_read_unlock(void) { }


/*******************
 ** linux/crc16.h **
 *******************/

extern u16 crc16(u16 crc, const u8 *buffer, size_t len);


/*******************
 ** linux/crc32.h **
 *******************/

#define CONFIG_CRC32_SLICEBY8 /* the default from lib/Kconfig */

extern u32  crc32_be(u32 crc, unsigned char const *p, size_t len);


/******************
 ** linux/hash.h **
 ******************/

#if BITS_PER_LONG == 32
#define hash_long(val, bits) hash_32(val, bits)
#elif BITS_PER_LONG == 64
#define hash_long(val, bits) hash_64(val, bits)
#endif

static inline u32 hash_32(u32 val, unsigned int bits)
{
	enum  { GOLDEN_RATIO_PRIME_32 = 0x9e370001UL };
	u32 hash = val * GOLDEN_RATIO_PRIME_32;
	hash =  hash >> (32 - bits);
	return hash;
}

static inline u64 hash_64(u64 val, unsigned int bits)
{
	u64 hash = val;
	u64 n = hash;
	n <<= 18;
	hash -= n;
	n <<= 33;
	hash -= n;
	n <<= 3;
	hash += n;
	n <<= 3;
	hash -= n;
	n <<= 4;
	hash += n;
	n <<= 2;
	hash += n;
	return hash >> (64 - bits);
}


/****************************
 ** asm-generic/getorder.h **
 ****************************/

int get_order(unsigned long size);


/***********************
 ** linux/interrupt.h **
 ***********************/

struct tasklet_struct
{
	void (*func)(unsigned long);
	unsigned long data;
};


/*********************
 ** trace functions **
 *********************/

#define trace_ext4_alloc_da_blocks(...)
#define trace_ext4_allocate_blocks(...)
#define trace_ext4_allocate_inode(...)
#define trace_ext4_begin_ordered_truncate(...)
#define trace_ext4_collapse_range(...)
#define trace_ext4_da_release_space(...)
#define trace_ext4_da_reserve_space(...)
#define trace_ext4_da_update_reserve_space(...)
#define trace_ext4_da_write_begin(...)
#define trace_ext4_da_write_end(...)
#define trace_ext4_da_write_pages(...)
#define trace_ext4_da_write_pages_extent(...)
#define trace_ext4_direct_IO_enter(...)
#define trace_ext4_direct_IO_exit(...)
#define trace_ext4_discard_blocks(...)
#define trace_ext4_discard_preallocations(...)
#define trace_ext4_drop_inode(...)
#define trace_ext4_es_cache_extent(...)
#define trace_ext4_es_find_delayed_extent_range_enter(...)
#define trace_ext4_es_find_delayed_extent_range_exit(...)
#define trace_ext4_es_insert_extent(...)
#define trace_ext4_es_lookup_extent_enter(...)
#define trace_ext4_es_lookup_extent_exit(...)
#define trace_ext4_es_remove_extent(...)
#define trace_ext4_es_shrink(...)
#define trace_ext4_es_shrink_count(...)
#define trace_ext4_es_shrink_scan_enter(...)
#define trace_ext4_es_shrink_scan_exit(...)
#define trace_ext4_evict_inode(...)
#define trace_ext4_ext_convert_to_initialized_enter(...)
#define trace_ext4_ext_convert_to_initialized_fastpath(...)
#define trace_ext4_ext_handle_unwritten_extents(...)
#define trace_ext4_ext_load_extent(...)
#define trace_ext4_ext_map_blocks_enter(...)
#define trace_ext4_ext_map_blocks_exit(...)
#define trace_ext4_ext_remove_space(...)
#define trace_ext4_ext_remove_space_done(...)
#define trace_ext4_ext_rm_idx(...)
#define trace_ext4_ext_rm_leaf(...)
#define trace_ext4_ext_show_extent(...)
#define trace_ext4_fallocate_enter(...)
#define trace_ext4_fallocate_exit(...)
#define trace_ext4_forget(...)
#define trace_ext4_free_blocks(...)
#define trace_ext4_free_inode(...)
#define trace_ext4_get_implied_cluster_alloc_exit(...)
#define trace_ext4_get_reserved_cluster_alloc(...)
#define trace_ext4_ind_map_blocks_enter(...)
#define trace_ext4_ind_map_blocks_exit(...)
#define trace_ext4_insert_range(...)
#define trace_ext4_invalidatepage(...)
#define trace_ext4_journal_start(...)
#define trace_ext4_journal_start_reserved(...)
#define trace_ext4_journalled_invalidatepage(...)
#define trace_ext4_journalled_write_end(...)
#define trace_ext4_load_inode(...)
#define trace_ext4_load_inode_bitmap(...)
#define trace_ext4_mark_inode_dirty(...)
#define trace_ext4_mb_bitmap_load(...)
#define trace_ext4_mb_buddy_bitmap_load(...)
#define trace_ext4_mb_discard_preallocations(...)
#define trace_ext4_mb_new_group_pa(...)
#define trace_ext4_mb_new_inode_pa(...)
#define trace_ext4_mb_release_group_pa(...)
#define trace_ext4_mb_release_inode_pa(...)
#define trace_ext4_mballoc_alloc(...)
#define trace_ext4_mballoc_discard(...)
#define trace_ext4_mballoc_free(...)
#define trace_ext4_mballoc_prealloc(...)
#define trace_ext4_other_inode_update_time(...)
#define trace_ext4_punch_hole(...)
#define trace_ext4_read_block_bitmap_load(...)
#define trace_ext4_readpage(...)
#define trace_ext4_releasepage(...)
#define trace_ext4_remove_blocks(...)
#define trace_ext4_request_blocks(...)
#define trace_ext4_request_inode(...)
#define trace_ext4_sync_file_enter(...)
#define trace_ext4_sync_file_exit(...)
#define trace_ext4_sync_fs(...)
#define trace_ext4_trim_all_free(...)
#define trace_ext4_trim_extent(...)
#define trace_ext4_truncate_enter(...)
#define trace_ext4_truncate_exit(...)
#define trace_ext4_unlink_enter(...)
#define trace_ext4_unlink_exit(...)
#define trace_ext4_write_begin(...)
#define trace_ext4_write_end(...)
#define trace_ext4_writepage(...)
#define trace_ext4_writepages(...)
#define trace_ext4_writepages_result(...)
#define trace_ext4_zero_range(...)
#define trace_jbd2_checkpoint(...)
#define trace_jbd2_checkpoint_stats(...)
#define trace_jbd2_commit_flushing(...)
#define trace_jbd2_commit_locking(...)
#define trace_jbd2_commit_logging(...)
#define trace_jbd2_drop_transaction(...)
#define trace_jbd2_end_commit(...)
#define trace_jbd2_handle_extend(...)
#define trace_jbd2_handle_start(...)
#define trace_jbd2_handle_stats(...)
#define trace_jbd2_lock_buffer_stall(...)
#define trace_jbd2_run_stats(...)
#define trace_jbd2_start_commit(...)
#define trace_jbd2_submit_inode_data(...)
#define trace_jbd2_update_log_tail(...)
#define trace_jbd2_write_superblock(...)
#define trace_printk(...)

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */

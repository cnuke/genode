/**
 * \brief  Backend implementation for Linux
 * \author Josef Soentgen
 * \date   2021-03-25
 */

#ifndef _LX_EMUL_CC_H_
#define _LX_EMUL_CC_H_

#ifdef __cplusplus
extern "C" {
#endif

/*****************
 ** DRM session **
 *****************/

void lx_emul_announce_drm_session(void);


/***********************
 ** memory management **
 ***********************/

struct Lx_dma
{
	unsigned long vaddr;
	unsigned long paddr;
};
struct Lx_dma lx_emul_dma_alloc_attrs(void const *, unsigned long, int);
void          lx_emul_dma_free_attrs(void const *, unsigned long, unsigned long, unsigned long);


void *lx_emul_vzalloc(unsigned long);
void  lx_emul_vfree(void const*);
void *lx_emul_kmalloc(unsigned long, unsigned int);
void  lx_emul_kfree(void const*);

struct lx_emul_kmem_cache
{
	void *cache; /* opaque kmem_cache */
	unsigned int size;
	unsigned int align;
};

int   lx_emul_kmem_cache_create(void const *, unsigned int, unsigned int);
void  lx_emul_kmem_cache_free(void const *);
void *lx_emul_kmem_cache_alloc(void const *);

int            lx_emul_alloc_address_space(void *, unsigned long);
int            lx_emul_add_dma_to_address_space(void *, struct Lx_dma);
void          *lx_emul_look_up_address_space_page(void *, unsigned long);
int            lx_emul_insert_page_to_address_page(void *, void *, unsigned long);
struct Lx_dma  lx_emul_get_dma_address_for_page(void *, void *);


/******************
 ** timing stuff **
 ******************/

unsigned long long lx_emul_ktime_get_mono_fast_ns(void);
void               lx_emul_usleep(unsigned long);


/*******************
 ** tasking stuff **
 *******************/

int           lx_emul_create_task(void *, int (*threadfn)(void *), void *);
unsigned long lx_emul_current_task(void);
void          lx_emul_block_current_task(void);
void          lx_emul_unblock_task(unsigned long);

struct workqueue_struct;
struct workqueue_struct *lx_emul_alloc_workqueue(char const *, unsigned int);

/********************
 ** platform stuff **
 ********************/

enum {
 	/* interrupts = <0x0 0x3 0x4> */
	GPU3D      = 0,
	GPU3D_INTR = 32 + 0x3,
};

int lx_emul_devm_request_threaded_irq(int, int, int (*handler)(int, void*), void *, int (*thread_fn)(int, void*));

void *lx_emul_devm_platform_ioremap_resource(void const *, unsigned int);

struct clk    *lx_emul_devm_clk_get(void const *, char const *id);
unsigned long  lx_emul_clk_get_rate(struct clk *);



#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL_CC_H_ */

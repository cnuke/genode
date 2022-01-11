/*
 * \brief  Shadows Linux kernel asm/page.h
 * \author Norman Feske
 * \date   2021-06-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_GENERIC_PAGE_H
#define __ASM_GENERIC_PAGE_H

#ifndef __ASSEMBLY__

#include <asm/page_types.h> /* for PAGE_SHIFT */

/*
 * The 'virtual' member of 'struct page' is needed by 'lx_emul_virt_to_phys'
 * and 'page_to_virt'.
 */
#define WANT_PAGE_VIRTUAL

#define clear_page(page)	memset((page), 0, PAGE_SIZE)
#define copy_page(to,from)	memcpy((to), (from), PAGE_SIZE)

#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)

#ifndef __pa
#define __pa(v) lx_emul_mem_dma_addr((void *)(v))
#endif

#ifndef __va
#include <lx_emul/debug.h>
#define __va(x) ( lx_emul_trace_and_stop("__va"), (void *)0 )
#endif


struct page;

#define page_to_virt(p)     ((p)->virtual)


#define pfn_to_page(pfn) ( (struct page *)(__va((pfn) << PAGE_SHIFT)) )
#define page_to_pfn(page) ( __pa((page)->virtual) >> PAGE_SHIFT )

typedef struct page *pgtable_t;

#define virt_addr_valid(kaddr) (kaddr != 0UL)

#include <lx_emul/alloc.h>
#include <lx_emul/page_virt.h>

static inline struct page *virt_to_page(void *v) { return lx_emul_virt_to_pages(v, 1U); }
#define pfn_to_kaddr(pfn)      __va((pfn) << PAGE_SHIFT)

#endif /* __ASSEMBLY__ */

#include <linux/pfn.h>

#include <asm-generic/getorder.h>

#endif /* __ASM_GENERIC_PAGE_H */

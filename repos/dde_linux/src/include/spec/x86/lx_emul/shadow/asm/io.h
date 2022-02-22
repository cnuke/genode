/**
 * \brief  Shadow copy of asm/io.h
 * \author Josef Soentgen
 * \date   2022-02-21
 */

#ifndef _ASM_X86_IO_H
#define _ASM_X86_IO_H

#include <linux/string.h>
#include <linux/compiler.h>
#include <asm/page.h>
// #include <asm/early_ioremap.h>
#include <asm/pgtable_types.h>

void __iomem *ioremap(resource_size_t offset, unsigned long size);
void iounmap(volatile void __iomem *addr);

// #include <asm-generic/iomap.h>

u8  _inb(unsigned long addr);
u16 _inw(unsigned long addr);
u32 _inl(unsigned long addr);

#define inb _inb
#define inw _inw
#define inl _inl

void _outb(u8 value, unsigned long addr);
void _outw(u16 value, unsigned long addr);
void _outl(u32 value, unsigned long addr);

#define outb _outb
#define outw _outw
#define outl _outl

#include <asm-generic/io.h>

#endif /* _ASM_X86_IO_H */

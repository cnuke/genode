/*
 * \brief  Lx_emul support for I/O port access
 * \author Josef Soentgen
 * \date   2022-02-22
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__IO_PORT_H_
#define _LX_EMUL__IO_PORT_H_

#ifdef __cplusplus
extern "C" {
#endif

unsigned char  lx_emul_io_port_inb(unsigned long phys_addr);
unsigned short lx_emul_io_port_inw(unsigned long phys_addr);
unsigned int   lx_emul_io_port_inl(unsigned long phys_addr);

void lx_emul_io_port_outb(unsigned long phys_addr, unsigned char  value);
void lx_emul_io_port_outw(unsigned long phys_addr, unsigned short value);
void lx_emul_io_port_outl(unsigned long phys_addr, unsigned int   value);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__IO_PORT_H_ */

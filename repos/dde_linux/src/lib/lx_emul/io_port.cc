/*
 * \brief  Lx_emul backend for I/O port access
 * \author Josef Soentgen
 * \date   2022-02-22
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/io_port.h>


template<typename T>
T _io_port_in(unsigned long phys_addr)
{
	using namespace Lx_kit;
	using namespace Genode;

	unsigned ret = 0;
	bool valid_ret = false;
	env().devices.for_each([&] (Device & d) {
		if (d.io_port(phys_addr)) {
			valid_ret = true;
			switch (sizeof (T)) {
			case sizeof (unsigned char):  ret = d.io_port_inb(phys_addr); break;
			case sizeof (unsigned short): ret = d.io_port_inw(phys_addr); break;
			case sizeof (unsigned int):   ret = d.io_port_inl(phys_addr); break;
			default:                                   valid_ret = false; break;
			}
		}
	});

	if (!valid_ret)
		error("could not read I/O port resource ", Hex(phys_addr));

	return static_cast<T>(ret);
}


unsigned char lx_emul_io_port_inb(unsigned long phys_addr)
{
	return _io_port_in<unsigned char>(phys_addr);
}


unsigned short lx_emul_io_port_inw(unsigned long phys_addr)
{
	return _io_port_in<unsigned short>(phys_addr);
}


unsigned int lx_emul_io_port_inl(unsigned long phys_addr)
{
	return _io_port_in<unsigned int>(phys_addr);
}

template<typename T>
void _io_port_out(unsigned long phys_addr, T value)
{
	using namespace Lx_kit;
	using namespace Genode;

	bool valid_ret = false;
	env().devices.for_each([&] (Device & d) {
		if (d.io_port(phys_addr)) {
			valid_ret = true;
			switch (sizeof (T)) {
			case sizeof (unsigned char):  d.io_port_outb(phys_addr, (unsigned char)value); break;
			case sizeof (unsigned short): d.io_port_outw(phys_addr, (unsigned short)value); break;
			case sizeof (unsigned int):   d.io_port_outl(phys_addr, (unsigned int)value); break;
			default:                                     valid_ret = false; break;
			}
		}
	});

	if (!valid_ret)
		error("could not write I/O port resource ", Hex(phys_addr));
}


void lx_emul_io_port_outb(unsigned long phys_addr, unsigned char value)
{
	_io_port_out<unsigned char>(phys_addr, value);
}


void lx_emul_io_port_outw(unsigned long phys_addr, unsigned short value)
{
	_io_port_out<unsigned short>(phys_addr, value);
}


void lx_emul_io_port_outl(unsigned long phys_addr, unsigned int value)
{
	_io_port_out<unsigned int>(phys_addr, value);
}

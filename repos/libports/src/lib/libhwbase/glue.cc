/*
 * \brief  Genode HW backend glue implementation
 * \author Josef Soentgen
 * \date   2017-10-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <io_mem_session/connection.h>
#include <io_port_session/connection.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <timer_session/connection.h>

/* local includes */
#include <glue.h>

namespace {

struct Io_memory
{
	Genode::Io_mem_session_client       _mem;
	Genode::Io_mem_dataspace_capability _mem_ds;

	Genode::addr_t                      _vaddr;

	Io_memory(Genode::Region_map &rm,
	          Genode::addr_t base, Genode::Io_mem_session_capability cap)
	:
		_mem(cap),
		_mem_ds(_mem.dataspace())
	{
		if (!_mem_ds.valid()) {
			Genode::error("I/O memory dataspace invalid");
			throw Genode::Exception();
		}

		_vaddr = rm.attach(_mem_ds);
		_vaddr |= base & 0xfff;
	}

	Genode::addr_t vaddr() const { return _vaddr; }
};

struct Io_port
{
	Genode::Io_port_connection _io_port;
	unsigned short             _base;

	Io_port(Genode::Env &env, unsigned short base, unsigned size)
	: _io_port(env, base, size), _base(base) { }

	unsigned short base() const { return _base; }

	template <typename T> void in(T *val)
	{
		switch (sizeof(*val)) {
		case 1:  *val = _io_port.inb(_base);
		case 2:  *val = _io_port.inw(_base);
		default: *val = _io_port.inl(_base);
		}
	}

	template <typename T> void out(T val)
	{
		switch (sizeof(val)) {
		case 1:  _io_port.outb(_base, val);
		case 2:  _io_port.outw(_base, val);
		default: _io_port.outl(_base, val);
		}
	}
};

}


/*
 * Glue code for using Genode services from Ada
 */
struct Glue
{
	Genode::Env &_env;

	Timer::Connection _timer { _env };

	Genode::Constructible<Platform::Connection> _pci;
	Platform::Device_capability                 _pci_dev_cap;

	enum { MAX_PCI_RESOURCES = 6, };
	Genode::Constructible<Io_memory> _pci_io_mem[MAX_PCI_RESOURCES];
	enum { MAX_IO_PORTS = 2, /* arbitrary value */ };
	Genode::Constructible<Io_port> _io_port[MAX_IO_PORTS];

	Io_port *_lookup_io_port(unsigned short port)
	{
		for (Genode::Constructible<Io_port> &io : _io_port) {
			if (io->base() == port) {
				return &*io;
			}
		}
		return nullptr;
	}

	Glue(Genode::Env &env) : _env(env) { }

	/***********
	 ** Timer **
	 ***********/

	unsigned long timer_now() const
	{
		return (unsigned long)_timer.elapsed_ms();
	}

	unsigned long timer_hz() const
	{
		return 1000UL;
	}

	/***************
	 ** I/O Ports **
	 ***************/

	bool handle_io_port(unsigned short port, unsigned int size)
	{
		for (unsigned i = 0; i < MAX_IO_PORTS; i++) {
			if (_io_port[i].constructed()) { continue; }

			try {
				_io_port[i].construct(_env, port, size);
				return true;
			} catch (...) { }
		}

		return false;
	}

	template <typename T>
	void io_port_in(unsigned short port, T *val)
	{
		Io_port *io = _lookup_io_port(port);
		if (!io) {
			*val = 0;
			return;
		}

		io->in(val);
	}

	template <typename T>
	void io_port_out(unsigned short port, T val)
	{
		Io_port *io = _lookup_io_port(port);
		if (!io) { return; }

		io->out(val);
	}

	/*********
	 ** PCI **
	 *********/

	Platform::Device_capability pci_dev_cap() { return _pci_dev_cap; }

	template <typename T>
	Platform::Device::Access_size _access_size(T t)
	{
		switch (sizeof(T)) {
		case 1:  return Platform::Device::ACCESS_8BIT;
		case 2:  return Platform::Device::ACCESS_16BIT;
		default: return Platform::Device::ACCESS_32BIT;
		}
	}

	template <typename T>
	void pci_read(unsigned int devfn, T *val)
	{
		Platform::Device_client client(_pci_dev_cap);
		*val = client.config_read(devfn, _access_size(*val));
	}

	template <typename T>
	void pci_write(unsigned int devfn, T val)
	{
		Platform::Device_client client(_pci_dev_cap);

		_pci->with_upgrade([&] () {
			client.config_write(devfn, val, _access_size(val)); });
	}

	int pci_open(unsigned bus, unsigned dev, unsigned func)
	{
		if (_pci.constructed()) { return 1; }

		try {
			_pci.construct(_env);

			_pci->upgrade_ram(1024 * 1024);

			_pci_dev_cap = _pci->first_device();
			if (!_pci_dev_cap.valid()) {
				_pci.destruct();
				return 0;
			}

			Platform::Device_client device(_pci_dev_cap);

			Genode::uint8_t b, d, f;
			device.bus_address(&b, &d, &f);
			if (b != bus || d != dev || f != func) {
				Genode::log("b: ", b, " d: ", d, " f: ", f);
				_pci->release_device(_pci_dev_cap);
				_pci.destruct();
				return 0;
			}
		} catch (...) {
			using namespace Genode;
			error("could not open PCI device ",
			      Hex(bus, Hex::OMIT_PREFIX), ":",
			      Hex(dev, Hex::OMIT_PREFIX), ":",
			      Hex(func, Hex::OMIT_PREFIX));
			return 0;
		}

		return 1;
	}

	bool _pci_access_ok(int const r)
	{
		if (r < 0 || r > 6) {
			Genode::error("invalid PCI resource");
			return false;
		}

		if (!_pci_dev_cap.valid()) {
			Genode::error("invalid PCI device");
			return false;
		}

		return true;
	}

	unsigned long long pci_map_resource(int r, int wc)
	{
		if (!_pci_access_ok(r)) { return 0; }

		unsigned long long addr = 0;
		try {
			Platform::Device_client device(_pci_dev_cap);
			Platform::Device::Resource res = device.resource(r);

			Genode::Constructible<Io_memory> &mem = _pci_io_mem[r];
			if (!mem.constructed()) {
				Genode::uint8_t const vr = device.phys_bar_to_virt(r);
				mem.construct(_env.rm(), res.base(), device.io_mem(vr));
			} else {
				Genode::warning("attempt to map already mapped PCI resource ", r);
			}

			addr = mem->vaddr();

			Genode::log("map PCI res: ", r, " (", Genode::Hex(res.base()),
			            ") to ", Genode::Hex(addr));
		} catch (...) {
			using namespace Genode;
			error("could not map PCI resource");
			return 0;
		}

		return addr;
	}

	unsigned long long pci_resource_size(int r)
	{
		if (!_pci_access_ok(r)) { return 0; }

		try {
			Platform::Device_client device(_pci_dev_cap);
			Platform::Device::Resource res = device.resource(r);
			return res.size();
		} catch (...) {
			Genode::error("could not query size of resource ", r);
		}

		return 0;
	}
};


static Genode::Constructible<Glue> _glue;


/*************************
 ** Libhwbase interface **
 *************************/

void Libhwbase::init(Genode::Env &env)
{
	_glue.construct(env);
}


Platform::Device_capability Libhwbase::pci_dev_cap()
{
	if (!_glue.constructed()) { throw -1; }

	return _glue->pci_dev_cap();
}


bool Libhwbase::handle_io_port(unsigned short port,
                               unsigned int   size)
{
	if (!_glue.constructed()) { throw -1; }

	return _glue->handle_io_port(port, size);
}


/**********************************
 ** Ada/Genode wrapper functions **
 **********************************/

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

/**
 * HW.PCI.Dev.Read8 backend implementation
 */
extern "C" uint8_t genode_pci_read8(uint32_t reg)
{
	if (!_glue.constructed()) { return 0; }
	uint8_t val = 0;
	_glue->pci_read(reg, &val);
	return val;
}


/**
 * HW.PCI.Dev.Read16 backend implementation
 */
extern "C" uint16_t genode_pci_read16(uint32_t reg)
{
	if (!_glue.constructed()) { return 0; }
	uint16_t val = 0;
	_glue->pci_read(reg, &val);
	return val;
}


/**
 * HW.PCI.Dev.Read32 backend implementation
 */
extern "C" uint32_t genode_pci_read32(uint32_t reg)
{
	if (!_glue.constructed()) { return 0; }
	uint32_t val = 0;
	_glue->pci_read(reg, &val);
	return val;
}


/**
 * HW.PCI.Dev.Write8 backend implementation
 */
extern "C" void genode_pci_write8(uint32_t reg, uint8_t val)
{
	if (!_glue.constructed()) { return; }
	_glue->pci_write(reg, val);
}


/**
 * HW.PCI.Dev.Write16 backend implementation
 */
extern "C" void genode_pci_write16(uint32_t reg, uint16_t val)
{
	if (!_glue.constructed()) { return; }
	_glue->pci_write(reg, val);
}


/**
 * HW.PCI.Dev.Write32 backend implementation
 */
extern "C" void genode_pci_write32(uint32_t reg, uint32_t val)
{
	if (!_glue.constructed()) { return; }
	_glue->pci_write(reg, val);
}


/**
 * HW.Port_IO.InB backend implementation
 */
extern "C" uint8_t genode_inb(unsigned short port)
{
	if (!_glue.constructed()) { return 0; }
	uint8_t val = 0;
	_glue->io_port_in(port, &val);
	return val;
}


/**
 * HW.Port_IO.InW backend implementation
 */
extern "C" uint16_t genode_inw(unsigned short port)
{
	if (!_glue.constructed()) { return 0; }
	uint16_t val = 0;
	_glue->io_port_in(port, &val);
	return val;
}


/**
 * HW.Port_IO.InW backend implementation
 */
extern "C" uint32_t genode_inl(unsigned short port)
{
	if (!_glue.constructed()) { return 0; }
	uint32_t val = 0;
	_glue->io_port_in(port, &val);
	return val;
}


/**
 * HW.Port_IO.OutB backend implementation
 */
extern "C" void genode_outb(unsigned short port, uint8_t val)
{
	if (!_glue.constructed()) { return; }
	_glue->io_port_out(port, val);
}


/**
 * HW.Port_IO.OutW backend implementation
 */
extern "C" void genode_outw(unsigned short port, uint16_t val)
{
	if (!_glue.constructed()) { return; }
	_glue->io_port_out(port, val);
}


/**
 * HW.Port_IO.OutL backend implementation
 */
extern "C" void genode_outl(unsigned short port, uint32_t val)
{
	if (!_glue.constructed()) { return; }
	_glue->io_port_out(port, val);
}


/**
 * HW.PCI.Dev.Initialize backend implementation
 */
extern "C" int genode_open_pci(unsigned bus, unsigned dev, unsigned func)
{
	if (!_glue.constructed()) { return 0; }
	return _glue->pci_open(bus, dev, func);
}


/**
 * HW.PCI.Dev.Map backend implementation
 */
extern "C" unsigned long long genode_map_resource(int res, int wc)
{
	if (!_glue.constructed()) { return 0; }
	return _glue->pci_map_resource(res, wc);
}


/**
 * HW.PCI.Dev.Size backend implementation
 */
extern "C" unsigned long long genode_resource_size(int res)
{
	if (!_glue.constructed()) { return 0; }
	return _glue->pci_resource_size(res);
}


/**
 * HW.Time.Timer.Raw_Value_Min backend implementation
 */
extern "C" unsigned long genode_timer_now()
{
	if (!_glue.constructed()) { return 0; }
	return _glue->timer_now();
}


/**
 * HW.Time.Timer.HZ backend implementation
 */
extern "C" unsigned long genode_timer_hz()
{
	if (!_glue.constructed()) { return 0; }
	return _glue->timer_hz();
}


/**
 * HW.Debug_Sink backend implementation
 */
extern "C" void genode_put(char c)
{
	enum { MAX_BUFFER = 255+1, };
	static char buffer[MAX_BUFFER] = { };
	static unsigned count = 0;

	if (count < MAX_BUFFER - 1) {
		buffer[count] = c;
		count++;
	} else {
		/* force flush */
		c = '\n';
	}

	if (c == '\n') {
		buffer[count-1] = 0;
		Genode::log((char const*)buffer);
		count = 0;
		Genode::memset(buffer, 0, sizeof(buffer));
	}
}

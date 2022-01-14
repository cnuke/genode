/*
 * \brief  Lx_kit device
 * \author Stefan Kalkowski
 * \date   2021-05-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>

using namespace Lx_kit;


/********************
 ** Device::Io_mem **
 ********************/

bool Device::Io_mem::match(addr_t addr, size_t size)
{
	return (this->addr <= addr) &&
	       ((this->addr + this->size) >= (addr + size));
}


/****************
 ** Device::Irq**
 ****************/

void Device::Irq::handle()
{
	env().last_irq = number;
	env().scheduler.unblock_irq_handler();
	env().scheduler.schedule();
}


Device::Irq::Irq(Entrypoint & ep, unsigned idx, unsigned number)
:
	idx{idx},
	number(number),
	handler(ep, *this, &Irq::handle) { }


/************
 ** Device **
 ************/

const char * Device::compatible()
{
	return _type.name.string();
}


const char * Device::name()
{
	return _name.string();
}


clk * Device::clock(const char * name)
{
	clk * ret = nullptr;
	_for_each_clock([&] (Clock & c) {
		if (c.name == name) {
			enable();
			ret = &c.lx_clock;
		}
	});
	return ret;
}


clk * Device::clock(unsigned idx)
{
	clk * ret = nullptr;
	_for_each_clock([&] (Clock & c) {
		if (c.idx == idx) {
			enable();
			ret = &c.lx_clock;
		}
	});
	return ret;
}


bool Device::io_mem(addr_t phys_addr, size_t size)
{
	bool ret = false;
	_for_each_io_mem([&] (Io_mem & io) {
		if (io.match(phys_addr, size))
			ret = true;
	});
	return ret;
}


void * Device::io_mem_local_addr(addr_t phys_addr, size_t size)
{
	void * ret = nullptr;
	_for_each_io_mem([&] (Io_mem & io) {
		if (!io.match(phys_addr, size))
			return;

		enable();

		if (!io.io_mem.constructed())
			io.io_mem.construct(*_pdev, io.idx);

		ret = (void*)((addr_t)io.io_mem->local_addr<void>()
		              + (phys_addr - io.addr));
	});
	return ret;
}


bool Device::irq_unmask(unsigned number)
{
	bool ret = false;

	_for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;

		ret = true;
		enable();

		if (irq.session.constructed())
			return;

		irq.session.construct(*_pdev, irq.idx);
		irq.session->sigh_omit_initial_signal(irq.handler);
		irq.session->ack();
	});

	return ret;
}


void Device::irq_mask(unsigned number)
{
	if (!_pdev.constructed())
		return;

	_for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;
		irq.session.destruct();
	});

}


void Device::irq_ack(unsigned number)
{
	if (!_pdev.constructed())
		return;

	_for_each_irq([&] (Irq & irq) {
		if (irq.number != number || !irq.session.constructed())
			return;
		irq.session->ack();
	});
}


bool Device::matches(unsigned bus, unsigned devfn) const
{
	return _bus == bus && _devfn == devfn;
}


void Device::bus_devfn(unsigned *bus, unsigned *devfn) const
{
	if (bus)
		*bus = _bus;

	if (devfn)
		*devfn = _devfn;
}


static Platform::Device::Config_space::Access_size access_size(unsigned len)
{
	using AS = Platform::Device::Config_space::Access_size;
	AS as = AS::ACCESS_8BIT;
	if (len == 4)      as = AS::ACCESS_32BIT;
	else if (len == 2) as = AS::ACCESS_16BIT;
	else               as = AS::ACCESS_8BIT;

	return as;
}


bool Device::read_config(unsigned reg, unsigned len, unsigned *val)
{
	if (!_pdev.constructed())
		enable();

	if (!val)
		return false;

	using AS = Platform::Device::Config_space::Access_size;
	AS const as = access_size(len);

	*val = Platform::Device::Config_space(*_pdev).read((unsigned char)reg, as);
	return true;
}


bool Device::write_config(unsigned reg, unsigned len, unsigned val)
{
	if (!_pdev.constructed())
		return false;

	using AS = Platform::Device::Config_space::Access_size;
	AS const as = access_size(len);

	Platform::Device::Config_space(*_pdev).write((unsigned char)reg, val, as);
	return true;
}


unsigned Device::vendor_id() const
{
	return _vendor_id;
}


unsigned Device::device_id() const
{
	return _device_id;
}


void Device::enable()
{
	if (_pdev.constructed())
		return;

	_pdev.construct(_platform, _name);

	_platform.update();
	_platform.with_xml([&] (Xml_node & xml) {
		xml.for_each_sub_node("device", [&] (Xml_node node) {
			if (_name != node.attribute_value("name", Device::Name()))
				return;

			node.for_each_sub_node("clock", [&] (Xml_node node) {
				clk * c = clock(node.attribute_value("name", Device::Name()).string());
				if (!c)
					return;
				c->rate = node.attribute_value("rate", 0UL);
			});
		});
	});
}



Device::Device(Entrypoint           & ep,
               Platform::Connection & plat,
               Xml_node             & xml,
               Heap                 & heap)
:
	_platform(plat),
	_name(xml.attribute_value("name", Device::Name())),
	_type{xml.attribute_value("type", Device::Name())}
{
	unsigned i = 0;
	xml.for_each_sub_node("io_mem", [&] (Xml_node node) {
		addr_t addr = node.attribute_value("phys_addr", 0UL);
		size_t size = node.attribute_value("size",      0UL);
		_io_mems.insert(new (heap) Io_mem(i++, addr, size));
	});

	i = 0;
	xml.for_each_sub_node("irq", [&] (Xml_node node) {
		_irqs.insert(new (heap) Irq(ep, i++, node.attribute_value("number", 0U)));
	});

	i = 0;
	xml.for_each_sub_node("clock", [&] (Xml_node node) {
		Device::Name name = node.attribute_value("name", Device::Name());
		_clocks.insert(new (heap) Device::Clock(i++, name));
	});

	/* tunnel PCI informations for now */
	xml.for_each_sub_node("property", [&] (Xml_node node) {
		using Name = Genode::String<16>;
		Name name = node.attribute_value("name", Name());
		if (name == "vendor_id") {
			_vendor_id = node.attribute_value("value", _vendor_id);
		} else
		if (name == "device_id") {
			_device_id = node.attribute_value("value", _device_id);
		} else
		if (name == "class_code") {
			_class_code = node.attribute_value("value", _class_code);
		} else
		if (name == "bus") {
			_bus = node.attribute_value("value", _bus);
		} else
		if (name == "dev") {
			unsigned dev = node.attribute_value("value", 0u);
			_devfn |= dev << 3;
		} else
		if (name == "func") {
			_devfn |= node.attribute_value("value", 0u);
		}
	});

	if (_type.name == "pci") {
		Genode::error(" XXXX ", _bus, ":", Genode::Hex(_devfn));
	}
}


/*****************
 ** Device_list **
 *****************/

Device_list::Device_list(Entrypoint           & ep,
                         Heap                 & heap,
                         Platform::Connection & platform)
:
	_platform(platform)
{
	_platform.with_xml([&] (Xml_node & xml) {
		xml.for_each_sub_node("device", [&] (Xml_node node) {
			insert(new (heap) Device(ep, _platform, node, heap));
		});
	});
}

/*
 * \brief  Legacy platform session wrapper
 * \author Josef Soentgen
 * \date   2022-01-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/ram_allocator.h>
#include <util/xml_generator.h>

/* DDE includes */
#include <lx_kit/env.h>
#include <platform_session/device.h>


template <typename FN>
static void acquire_resources(Legacy_platform::Device &device,
                              FN const &fn)
{
	using R = Legacy_platform::Device::Resource;
	for (unsigned resource_id = 0; resource_id < 6; resource_id++) {

		R const resource = device.resource(resource_id);
		if (resource.type() != R::INVALID)
			fn(resource_id, resource);
	}
}


template<typename T>
static Genode::String<16> to_string(T val)
{
	return Genode::String<16>(Genode::Hex(val));
}


Platform::Connection::Connection(Genode::Env &env)
:
	_env { env }
{
	try {
		_legacy_platform.construct(env);
	} catch (...) {
		Genode::error("could not construct legacy platform connection");
		throw;
	}

	/* empirically determined */
	_legacy_platform->upgrade_ram(32768);
	_legacy_platform->upgrade_caps(8);

	_legacy_platform->with_upgrade([&] () {
		_device_cap =
			_legacy_platform->next_device(_device_cap,
			                              0x0c0300, 0xffff00u);
	});

	if (!_device_cap.valid()) {
		Genode::error("could not find valid PCI device");
		return;
	}

	Legacy_platform::Device_client device { _device_cap };
	enum : size_t { DEVICES_NODE_SIZE = sizeof (_node_buffer), };
	char *buffer = _node_buffer;

	using R = Legacy_platform::Device::Resource;

	unsigned char bus, dev, func;
	device.bus_address(&bus, &dev, &func);

	Genode::Xml_generator xml { buffer, DEVICES_NODE_SIZE,
	                            "devices", [&] () {
		xml.node("device", [&] () {
			xml.attribute("name", "pci");
			xml.attribute("type", "pci");

			xml.node("property", [&] () {
				xml.attribute("name",  "vendor_id");
				xml.attribute("value", to_string(device.vendor_id()));
			});

			xml.node("property", [&] () {
				xml.attribute("name",  "device_id");
				xml.attribute("value", to_string(device.device_id()));
			});

			xml.node("property", [&] () {
				xml.attribute("name",  "class_code");
				xml.attribute("value", to_string(device.class_code()));
			});

			xml.node("property", [&] () {
				xml.attribute("name",  "bus");
				xml.attribute("value", to_string(bus));
			});

			xml.node("property", [&] () {
				xml.attribute("name",  "dev");
				xml.attribute("value", to_string(dev));
			});

			xml.node("property", [&] () {
				xml.attribute("name",  "func");
				xml.attribute("value", to_string(func));
			});

			acquire_resources(device, [&] (unsigned id, R const &r) {

				xml.node(r.type() == R::MEMORY ? "io_mem" : "io_port", [&] () {
					xml.attribute("phys_addr", to_string(r.base()));
					xml.attribute("size",      r.size());
					xml.attribute("bar",       id);
				});
			});
		});
	} };

	_devices_node.construct(buffer, DEVICES_NODE_SIZE);
	Genode::log(*_devices_node);
}


void Platform::Connection::update()
{
	Genode::error(__func__, ": not implemented");
}


Genode::Ram_dataspace_capability
Platform::Connection::alloc_dma_buffer(size_t size, Cache cache)
{
	return _legacy_platform->with_upgrade([&] () {
		return _legacy_platform->alloc_dma_buffer(size, cache);
	});
}


void Platform::Connection::free_dma_buffer(Ram_dataspace_capability)
{
	Genode::error(__func__, ": not implemented");
}


Genode::addr_t Platform::Connection::dma_addr(Ram_dataspace_capability ds_cap)
{
	return _legacy_platform->dma_addr(ds_cap);
}


static Legacy_platform::Device::Access_size convert(Platform::Device::Config_space::Access_size size)
{
	using PAS = Platform::Device::Config_space::Access_size;
	using LAS = Legacy_platform::Device::Access_size;
	switch (size) {
	case PAS::ACCESS_8BIT:
		return LAS::ACCESS_8BIT;
	case PAS::ACCESS_16BIT:
		return LAS::ACCESS_16BIT;
	case PAS::ACCESS_32BIT:
		return LAS::ACCESS_32BIT;
	}

	return LAS::ACCESS_8BIT;
}


static int bar_checked_for_size[6];


static unsigned bar_size(Genode::Xml_node const &devices, unsigned bar)
{
	using namespace Genode;

	unsigned val = 0;
	devices.for_each_sub_node("device", [&] (Xml_node device) {
		device.for_each_sub_node("io_mem", [&] (Xml_node node) {
			if (node.attribute_value("bar", 6u) == bar) {
				val = node.attribute_value("size", 0u);
			}
		});
	});

	return val;
}


unsigned Platform::Device::Config_space::read(unsigned char address,
                                              Access_size size)
{
	Legacy_platform::Device_client device {
		_device._platform._device_cap };

	// 32bit BARs only for now
	if (address >= 0x10 && address <= 0x24) {
		unsigned const bar = (address - 0x10) / 4;
		Genode::log(__func__, ": check bar: ", bar);
		if (bar_checked_for_size[bar]) {
			bar_checked_for_size[bar] = 0;
			return bar_size(*_device._platform._devices_node, bar);
		}
	}

	Legacy_platform::Device::Access_size const as = convert(size);
	return device.config_read(address, as);
}


void Platform::Device::Config_space::write(unsigned char address,
                                           unsigned value,
                                           Access_size size)
{
	Legacy_platform::Device_client device {
		_device._platform._device_cap };

	// 32bit BARs only for now
	if (address >= 0x10 && address <= 0x24) {
		unsigned const bar = (address - 0x10) / 4;
		Genode::log(__func__, ": check bar: ", bar);
		if (value == 0xffffffffu)
			bar_checked_for_size[bar] = 1;
		return;
	}

	Legacy_platform::Device::Access_size const as = convert(size);
	device.config_write(address, value, as);
}


Genode::size_t Platform::Device::Mmio::size() const
{
	size_t const size = _attached_ds.constructed() ? _attached_ds->size() : 0;

	Genode::log(__func__, ": size: ", size);

	return size;
}


void *Platform::Device::Mmio::_local_addr()
{
	Genode::log(__func__, ": index: ", _index.value);

	if (!_attached_ds.constructed()) {
		Legacy_platform::Device_client device {
			_device._platform._device_cap };

		Genode::uint8_t const id =
			device.phys_bar_to_virt((Genode::uint8_t)_index.value);

		Genode::Io_mem_session_capability io_mem_cap =
			device.io_mem(id);

		Io_mem_session_client io_mem_client(io_mem_cap);

		_attached_ds.construct(Lx_kit::env().env.rm(),
		                       io_mem_client.dataspace());
	}

	return _attached_ds->local_addr<void*>();
}

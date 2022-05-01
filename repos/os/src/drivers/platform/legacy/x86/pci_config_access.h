/*
 * \brief  PCI configuration access for the platform driver
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _X86_PCI_CONFIG_ACCESS_H_
#define _X86_PCI_CONFIG_ACCESS_H_

#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <io_port_session/connection.h>
#include <legacy/x86/platform_device/platform_device.h>
#include <util/bit_array.h>
#include <util/mmio.h>

namespace Platform { namespace Pci { struct Bdf; struct Config; } }


struct Platform::Pci::Bdf
{
	unsigned bus, device, function;

	static Bdf from_value(uint16_t const bdf)
	{
		return Bdf { .bus      = (bdf >> 8) & 0xffu,
		             .device   = (bdf >> 3) & 0x1fu,
		             .function =  bdf       & 0x07u };
	}

	uint16_t value() const {
		return ((bus & 0xff) << 8) | ((device & 0x1f) << 3) | (function & 7); }

	bool operator == (Bdf const &other) const {
		return value() == other.value(); }

	void print(Output &out) const
	{
		using Genode::print;
		print(out, Hex(bus, Hex::Prefix::OMIT_PREFIX, Hex::Pad::PAD),
		      ":", Hex(device, Hex::Prefix::OMIT_PREFIX, Hex::Pad::PAD),
		      ".", Hex(function, Hex::Prefix::OMIT_PREFIX));
	}
};


namespace Platform {

	class Config_access
	{
		private:

			Genode::Env &_env;

			enum { REG_ADDR = 0xcf8, REG_DATA = 0xcfc, REG_SIZE = 4 };

			/**
			 * Request interface to access an I/O port
			 */
			template <unsigned port>
			Genode::Io_port_session *_io_port()
			{
				/*
				 * Open I/O-port session when first called.
				 * The number of instances of the io_port_session_client
				 * variable depends on the number of instances of the
				 * template function.
				 *
				 * Thanks to this mechanism, the sessions to the PCI
				 * ports are created lazily when PCI service is first
				 * used. If the PCI bus driver is just started but not
				 * used yet, other processes are able to access the PCI
				 * config space.
				 *
				 * Once created, each I/O-port session persists until
				 * the PCI driver gets killed by its parent.
				 */
				static Genode::Io_port_connection io_port(_env, port, REG_SIZE);
				return &io_port;
			}

			/**
			 * Generate configuration address
			 *
			 * \param bus       target PCI bus ID  (0..255)
			 * \param device    target device ID   (0..31)
			 * \param function  target function ID (0..7)
			 * \param addr      target byte within targeted PCI config space (0..255)
			 *
			 * \return configuration address (written to REG_ADDR register)
			 */
			unsigned _cfg_addr(Pci::Bdf const bdf, int addr)
			{
				return ( (1u           << 31) |
				         (bdf.bus      << 16) |
				         (bdf.device   << 11) |
				         (bdf.function <<  8) |
				         (addr          & ~3) );
			}

			Bit_array<256> _used { };

			void _use_register(unsigned char addr, unsigned short width)
			{
				for (unsigned i = 0; i < width; i++)
					if (!_used.get(addr + i, 1))
						_used.set(addr + i, 1);
			}

		public:

			class Invalid_mmio_access : Exception { };

			Config_access(Genode::Env &env) : _env { env } { }

			/**
			 * Read value from config space of specified device/function
			 *
			 * \param bdf       target PCI bus, device & function ID
			 * \param addr      target byte within targeted PCI config space
			 * \param size      bit width of read access
			 *
			 * \return          value read from specified config-space address
			 *
			 * There is no range check for the input values.
			 */
			unsigned read(Pci::Bdf const bdf, unsigned char const addr,
			              Device::Access_size const size, bool const track = true)
			{
				/* write target address */
				_io_port<REG_ADDR>()->outl(REG_ADDR, _cfg_addr(bdf, addr));

				/* return read value */
				switch (size) {

				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);
					return _io_port<REG_DATA>()->inb(REG_DATA + (addr & 3));

				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);
					return _io_port<REG_DATA>()->inw(REG_DATA + (addr & 2));

				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);
					return _io_port<REG_DATA>()->inl(REG_DATA);
				}
				return ~0U;
			}

			/**
			 * Write to config space of specified device/function
			 *
			 * \param bdf       target PCI bus, device & function ID
			 * \param addr      target byte within targeted PCI config space
			 * \param value     value to be written
			 * \param size      bit width of write access
			 *
			 * There is no range check for the input values.
			 */
			void write(Pci::Bdf const bdf, unsigned char const addr,
			           unsigned const value, Device::Access_size const size,
			           bool const track = true)
			{
				/* write target address */
				_io_port<REG_ADDR>()->outl(REG_ADDR, _cfg_addr(bdf, addr));

				/* write value to targeted address */
				switch (size) {

				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);
					 _io_port<REG_DATA>()->outb(REG_DATA + (addr & 3), value);
					break;

				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);
					_io_port<REG_DATA>()->outw(REG_DATA + (addr & 2), value);
					break;

				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);
					_io_port<REG_DATA>()->outl(REG_DATA, value);
					break;
				}
			}

			bool reg_in_use(unsigned char addr, Device::Access_size size)
			{
				switch (size) {
				case Device::ACCESS_8BIT:
					return _used.get(addr, 1);
				case Device::ACCESS_16BIT:
					return _used.get(addr, 2);
				case Device::ACCESS_32BIT:
					return _used.get(addr, 4);
				default:
					return true;
				}
			}
	};

#if 0
	class Config_access
	{
		private:

			Attached_io_mem_dataspace &_pciconf;

			size_t const _pciconf_size;

			/**
			 * Calculate device offset from BDF
			 *
			 * \return device base address
			 */
			unsigned _dev_base(Pci::Bdf const bdf)
			{
				return unsigned(bdf.value()) << 12;
			}

			Bit_array<256> _used { };

			void _use_register(unsigned char addr, unsigned short width)
			{
				for (unsigned i = 0; i < width; i++)
					if (!_used.get(addr + i, 1))
						_used.set(addr + i, 1);
			}

		public:

			class Invalid_mmio_access : Exception { };

			Config_access(Attached_io_mem_dataspace &pciconf)
			:
				_pciconf(pciconf),
				_pciconf_size(Dataspace_client(_pciconf.cap()).size())
			{ }

			Config_access(Config_access &c)
			: _pciconf(c._pciconf), _pciconf_size(c._pciconf_size) { }

			/**
			 * Read value from config space of specified device/function
			 *
			 * \param bdf       target PCI bus, device & function ID
			 * \param addr      target byte within targeted PCI config space
			 * \param size      bit width of read access
			 *
			 * \return          value read from specified config-space address
			 *
			 * There is no range check for the input values.
			 */
			unsigned read(Pci::Bdf const bdf, unsigned char const addr,
			              Device::Access_size const size, bool const track = true)
			{
				unsigned     const offset    = _dev_base(bdf) + addr;
				char const * const field_ptr = _pciconf.local_addr<char>() + offset;

				if (offset >= _pciconf_size)
					throw Invalid_mmio_access();

				switch (size) {

				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);
					return *(uint8_t const *)field_ptr;

				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);
					return *(uint16_t const *)field_ptr;

				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);
					return *(uint32_t const *)field_ptr;
				}
				return ~0U;
			}

			/**
			 * Write to config space of specified device/function
			 *
			 * \param bdf       target PCI bus, device & function ID
			 * \param addr      target byte within targeted PCI config space
			 * \param value     value to be written
			 * \param size      bit width of write access
			 *
			 * There is no range check for the input values.
			 */
			void write(Pci::Bdf const bdf, unsigned char const addr,
			           unsigned const value, Device::Access_size const size,
			           bool const track = true)
			{
				unsigned const offset    = _dev_base(bdf) + addr;
				char *   const field_ptr = _pciconf.local_addr<char>() + offset;

				if (offset >= _pciconf_size)
					throw Invalid_mmio_access();

				/*
				 * Write value to targeted address, see read() comment above
				 * for an explanation of the assembly templates
				 */
				switch (size) {

				case Device::ACCESS_8BIT:
					if (track)
						_use_register(addr, 1);
					*(uint8_t volatile *)field_ptr = value;
					break;

				case Device::ACCESS_16BIT:
					if (track)
						_use_register(addr, 2);
					*(uint16_t volatile *)field_ptr = value;
					break;

				case Device::ACCESS_32BIT:
					if (track)
						_use_register(addr, 4);
					*(uint32_t volatile *)field_ptr = value;
					break;
				}
			}

			bool reg_in_use(unsigned char addr, Device::Access_size size)
			{
				switch (size) {
				case Device::ACCESS_8BIT:
					return _used.get(addr, 1);
				case Device::ACCESS_16BIT:
					return _used.get(addr, 2);
				case Device::ACCESS_32BIT:
					return _used.get(addr, 4);
				default:
					return true;
				}
			}
	};
#endif
}

/**
 * Type-safe, fine-grained access to a PCI config space of a device
 *
 * It is similar to Genode::Mmio but uses Config_access as backend.
 */
struct Platform::Pci::Config: Register_set<Platform::Pci::Config>
{
	private:

		friend Register_set_plain_access;

		Config_access &_config;
		Pci::Bdf       _bdf;
		uint16_t       _cap;

		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const
		{
			addr_t const cap = _cap + offset;

			if (sizeof(ACCESS_T) == 1)
				return _config.read(_bdf, cap, Device::ACCESS_8BIT);
			if (sizeof(ACCESS_T) == 2)
				return _config.read(_bdf, cap, Device::ACCESS_16BIT);
			if (sizeof(ACCESS_T) == 4)
				return _config.read(_bdf, cap, Device::ACCESS_32BIT);

			warning("unsupported read ", sizeof(ACCESS_T));
			return 0;
		}

		template <typename ACCESS_T>
		inline void _write(off_t const offset, ACCESS_T const value)
		{
			addr_t const cap = _cap + offset;

			switch (sizeof(ACCESS_T)) {
			case 1 :
				_config.write(_bdf, cap, value, Device::ACCESS_8BIT);
				break;
			case 2 :
				_config.write(_bdf, cap, value, Device::ACCESS_16BIT);
				break;
			case 4 :
				_config.write(_bdf, cap, value, Device::ACCESS_32BIT);
				break;
			default:
				warning("unsupported write ", sizeof(ACCESS_T));
			}
		}

	public:

		Config(Config_access &config, Pci::Bdf const &bdf, uint16_t cap)
		:
			Register_set<Config>(*this), _config(config), _bdf(bdf), _cap(cap)
		{ }

};

#endif /* _X86_PCI_CONFIG_ACCESS_H_ */

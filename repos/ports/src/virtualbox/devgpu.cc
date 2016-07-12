/*
 * \brief  PCI VGA GPU pass-through device frontend
 * \author Josef Soentgen
 * \date   2016-07-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/log.h>
#include <util/list.h>
#include <io_port_session/connection.h>
#include <io_mem_session/connection.h>
#include <irq_session/connection.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/retry.h>

/* Virtualbox includes */
/* XXX hijack DEV_VGA */
#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/pci.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#ifdef IN_RING3
# include <iprt/alloca.h>
# include <iprt/mem.h>
# include <iprt/thread.h>
# include <iprt/uuid.h>
#endif
#include <VBoxDD.h>


static bool const verbose_gttmmadr = false;
static bool const verbose_io       = false;
static bool const verbose_vga_io   = false;
static bool const verbose_vga_mmio = false;
static bool const verbose_pci_cfg  = false;


/************************
 ** GPU device struct **
 ************************/

struct Gpu;
struct Io_memory;
struct Io_port;

struct GPU
{
	/** The PCI device. */
	PCIDEVICE    PciDev;

	/** Pointer to default PCI config read function. */
	R3PTRTYPE(PFNPCICONFIGREAD) pfnConfigRead;

	/** Pointer to default PCI config write function. */
	R3PTRTYPE(PFNPCICONFIGWRITE) pfnConfigWrite;

	/** Pointer to the device instance - R3 ptr. */
	PPDMDEVINSR3 pDevInsR3;

	/** Pointer to the device instance - R0 ptr */
	PPDMDEVINSR0 pDevInsR0;

	/** Receiver thread that handles all IRQ signals. */
	PPDMTHREAD   pThread;

	Io_memory  *gttmmadr;
	RTGCPHYS32  gttmmadr_base; /* local guest address */

	Io_memory  *gmadr;
	RTGCPHYS32  gmadr_base;

	Io_port    *iobar;
	RTGCPHYS32  IOBase4;
	uint32_t    iobar_index;

	Gpu *gpu;

	Io_port   *vga_port_io;
	Io_memory *vga_buffer;

	uint8_t *vbios_rom;
	uint32_t vbios_rom_size;
	bool     vbios_rom_installed;
};


/** Pointer to GPU device data. */
typedef struct GPU *PGPU;
/** Read-only pointer to the GPU device data. */
typedef struct GPU const *PCGPU;


/*
 * used by DevVGA
 *
 * (see IHD-OS-CHV-BSW-Vol 2e p. 10)
 */
enum {
	IGD_ASLS = 0xfc,
	IGD_BDSM = 0x5c,
	IGD_GMCH = 0x50,
	IGD_BGSM = 0x70, /* only in BSW -> check for BDW */

	IGD_GEN8_MASTER_IRQ        = 0x44200,
	IGD_GEN8_MASTER_IRQ_ENABLE = 0x80000000,

	IGD_BDW_DEVICE_ID = 0x1600,

	IGD_OPREGION_SIZE = 0x2000,
};


struct Io_port
{
	Genode::Io_port_session_client _io;
	Genode::addr_t                 _base;

	Io_port(Genode::addr_t base, Genode::Io_port_session_capability cap)
		: _io(cap), _base(base) { }

	Genode::addr_t base() { return _base; }

	unsigned read_1(unsigned long address) {
		return _io.inb(_base + address); }

	unsigned read_2(unsigned long address) {
		return _io.inw(_base + address); }

	unsigned read_4(unsigned long address) {
		return _io.inl(_base + address); }

	void write_1(unsigned long address, unsigned char value) {
		_io.outb(_base + address, value); }

	void write_2(unsigned long address, unsigned short value) {
		_io.outw(_base + address, value); }

	void write_4(unsigned long address, unsigned int value) {
		_io.outl(_base + address, value); }
};


struct Io_memory
{
	Genode::Io_mem_session_client       _mem;
	Genode::Io_mem_dataspace_capability _mem_ds;
	Genode::addr_t                      _vaddr;

	Io_memory(Genode::Region_map                &rm,
	          Genode::addr_t                     base,
	          Genode::Io_mem_session_capability  cap)
	: _mem(cap)
	{
		try { _mem_ds = _mem.dataspace(); }
		catch (...) { Genode::error("could not get dataspace"); }

		if (!_mem_ds.valid()) {
			Genode::error("mem dataspace not valid");
			throw Genode::Exception();
		}

		try         { _vaddr = rm.attach(_mem_ds); }
		catch (...) { Genode::error("could not attach mem dataspace"); }

		_vaddr |= base & 0xfff;
	}

	Genode::Io_mem_dataspace_capability cap() { return _mem_ds; }

	Genode::addr_t vaddr() { return _vaddr; }

	unsigned read_1(unsigned long address) {
		return *(volatile unsigned char*)(_vaddr + address); }

	unsigned read_2(unsigned long address) {
		return *(volatile unsigned short*)(_vaddr + address); }

	unsigned read_4(unsigned long address) {
		return *(volatile unsigned int*)(_vaddr + address); }

	void write_1(unsigned long address, unsigned char value) {
		*(volatile unsigned char*)(_vaddr + address) = value; }

	void write_2(unsigned long address, unsigned short value) {
		*(volatile unsigned short*)(_vaddr + address) = value; }

	void write_4(unsigned long address, unsigned int value) {
		*(volatile unsigned int*)(_vaddr + address) = value; }
};


Genode::addr_t _dma_addr;
Genode::size_t _dma_size;


class Gpu
{
	private:

		Platform::Connection        _platform;
		Platform::Device_capability _device_cap;
		Platform::Device_client     _device;

		uint16_t _lpc_device_id { 0 };

		void query_isabridge()
		{
			enum { ISA_BRIDGE = 0x060100, ISA_BRIDGE_MASK = 0xffff00, };

			Platform::Device_capability isa_cap = _platform.first_device(ISA_BRIDGE,
			                                                             ISA_BRIDGE_MASK);
			if (!isa_cap.valid()) {
				Genode::error("could not access LPC/ISA bridge");
				return;
			}

			/**
			 * LPC/ISA device id is needed in the PIIX3 PCI-to-ISA bridge
			 * for guest drivers to work
			 */

			Platform::Device_client device(isa_cap);
			_lpc_device_id = device.device_id();

			_platform.release_device(isa_cap);
		}

		Platform::Device_capability _find_gpu_card()
		{
			Platform::Device_capability prev_device_cap, device_cap;
			Genode::env()->parent()->upgrade(_platform.cap(), "ram_quota=4096");
			for (device_cap = _platform.first_device();
				 device_cap.valid();
				 device_cap = _platform.next_device(prev_device_cap)) {

				Platform::Device_client device(device_cap);

				if (prev_device_cap.valid())
					_platform.release_device(prev_device_cap);

				if ((device.class_code() >> 8) == 0x0300
				    && (device.device_id() & 0xff00) == IGD_BDW_DEVICE_ID)
					break;

				prev_device_cap = device_cap;
			}

			if (!device_cap.valid()) {
				Genode::error("No IGD (BDW) found");
				return Platform::Device_capability();
			}

			return device_cap;
		}

		Genode::Irq_session_client _irq;
		Genode::Signal_receiver    _sig_rec;

		PPDMDEVINS _vbox_pci_dev;

		Genode::Lock _irq_lock;
		bool _irq_pending = false;

		Genode::Signal_dispatcher<Gpu> _irq_dispatcher {
			_sig_rec, *this, &Gpu::_handle_irq };

		void _handle_irq(unsigned)
		{
			Genode::Lock::Guard g(_irq_lock);

			_irq_pending = true;
			PDMDevHlpPCISetIrqNoWait(_vbox_pci_dev, 0, 1);
		}

		void     *_igd_opregion { nullptr };
		uint32_t  _igd_gmch_ctl { 0U };
		uint32_t  _igd_gtt_max  { 0U };

		Genode::addr_t _igd_bdsm     { 0U };
		uint32_t       _igd_dsm_size { 0U };

	public:

	Gpu(PPDMDEVINS pci_dev)
	:
		_device_cap(_find_gpu_card()),
		_device(_device_cap),
		_irq(_device.irq(0)),
		_vbox_pci_dev(pci_dev)
	{
		if (!_device.valid()) { return; }

		PVM pVM = PDMDevHlpGetVM(_vbox_pci_dev);

		unsigned char bus = 0, dev = 0, fn = 0;
		_device.bus_address(&bus, &dev, &fn);

		/*
		 * MGGC0_0_2_0_PCI
		 *
		 * (see intel-gfx-prm-osrc-bdw-vol02c-commandreference-registers_4.pdf)
		 */

		/* GMCH */
		_igd_gmch_ctl = _device.config_read(IGD_GMCH, Platform::Device::ACCESS_32BIT);
		Genode::info("orig igd_gmch_ctl: ", Genode::Hex(_igd_gmch_ctl));

		/* GGMS (GTT Graphics Memory size) */
		unsigned ggms = (1 << ((_igd_gmch_ctl >> 6) & 0x3)) << 20;
		enum { PAGE_SIZE_LOG2 = 12, };
		_igd_gtt_max = (ggms >> PAGE_SIZE_LOG2) * 8;
		Genode::info("ggms: ", ggms, " gtt_max: ", _igd_gtt_max);

		_igd_gmch_ctl &= 0xff; /* disable GMS pre-allocated memory */
		_igd_gmch_ctl |= (1U<<8);

		Genode::info("new  igd_gmch_ctl: ", Genode::Hex(_igd_gmch_ctl));

		/* BDSM_0_2_0_PCI */
		Genode::addr_t bdsm_addr = _device.config_read(IGD_BDSM, Platform::Device::ACCESS_32BIT);
		Genode::info("orig bdsm_addr: ", Genode::Hex(bdsm_addr));

		_igd_dsm_size = ggms + (32<<20); /* XXX fixme */

		char quota[32];
		Genode::snprintf(quota, sizeof(quota), "ram_quota=%zd", _igd_dsm_size);

		Genode::env()->parent()->upgrade(_platform.cap(), quota);

		Genode::Ram_dataspace_capability ram_cap;
		
		try {
			// ram_cap = _platform.alloc_dma_buffer(_igd_dsm_size);
			ram_cap = Genode::env()->ram_session()->alloc(_igd_dsm_size);
		} catch (...) {
			Genode::error("could not allocate DMA buffer");
			throw;
		}

		_igd_bdsm = (Genode::addr_t)Genode::env()->rm_session()->attach(ram_cap);
		if (_igd_bdsm & 0xfffff) {
			Genode::error("BDSM: ", Genode::Hex(_igd_bdsm), " not 1 MiB aligned");
		}

		Genode::info("new  bdsm_addr: ", Genode::Hex(_igd_bdsm));

		/* trigger mapping */
		Genode::memset((void*)_igd_bdsm, 0, _igd_dsm_size);

		_dma_addr = _igd_bdsm;
		_dma_size = _igd_dsm_size;

		int rc = 0;

		/* OpRegion */
		uint32_t const opregion_addr = _device.config_read(IGD_ASLS,
		                                                   Platform::Device::ACCESS_32BIT);

		Genode::Io_mem_connection iom(opregion_addr, IGD_OPREGION_SIZE);
		Genode::Io_mem_dataspace_capability iom_cap;
		try { iom_cap = iom.dataspace(); }
		catch (...) {
			Genode::error("could not get dataspace");
			throw Genode::Exception();
		}

		if (!iom_cap.valid()) {
		// Genode::snprintf(quota, sizeof(quota), "ram_quota=%zd", _igd_dsm_size + (33<<20));
			Genode::error("mem dataspace not valid");
			throw Genode::Exception();
		}

		Genode::addr_t opregion_vaddr = 0UL;

		try         { opregion_vaddr = Genode::env()->rm_session()->attach(iom_cap); }
		catch (...) { Genode::error("could not attach iomem dataspace"); }

		opregion_vaddr |= opregion_addr & 0xfff;

		if (Genode::memcmp((void*)opregion_vaddr, "IntelGraphicsMem", 16) != 0) {
			Genode::error("OpRegion signature mismatch");
			throw Genode::Exception();
		}

		enum { OPREGION_SIZE_OFFSET = 0x10, };
		uint32_t const opregion_size =
			*((Genode::addr_t*)(opregion_vaddr + OPREGION_SIZE_OFFSET)) * 1024;

		if (opregion_size != IGD_OPREGION_SIZE) {
			Genode::error("OpRegion size mismatch");
			throw Genode::Exception();
		}

		rc = MMR3HyperAllocOnceNoRel(pVM, IGD_OPREGION_SIZE, 4096,
		                             MM_TAG_PDM_DEVICE_USER, (void **)&_igd_opregion);
		if (RT_FAILURE(rc)) {
			Genode::error("could not allocate OpRegion memory");
			throw Genode::Exception();
		}

		Genode::memcpy(_igd_opregion, (void*)opregion_vaddr, IGD_OPREGION_SIZE);

		Genode::info("igd_opregion: ", _igd_opregion);

		rc = PDMDevHlpROMRegister(_vbox_pci_dev, (uint32_t)(uint64_t)_igd_opregion,
		                          IGD_OPREGION_SIZE, _igd_opregion, IGD_OPREGION_SIZE,
		                          PGMPHYS_ROM_FLAGS_PERMANENT_BINARY, "igd_opregion");
		if (RT_FAILURE(rc)) {
			Genode::error("Could not register OpRegion ROM mapping");
			throw Genode::Exception();
		}

		query_isabridge();

		Genode::info("GPU found at ", bus, ":", dev, ".", fn, " DSM size: ", _igd_dsm_size);

		Genode::info("XxXxXxXxXxXxXxXxXxX BDSM addr:     ", Genode::Hex((Genode::addr_t)_igd_bdsm));
		Genode::info("XxXxXxXxXxXxXxXxXxX OpRegion addr: ", Genode::Hex((Genode::addr_t)_igd_opregion));
	}

	Platform::Device_client &device()  { return _device; }

	Platform::Device_capability &device_cap()  { return _device_cap; }

	Platform::Connection &platform() { return _platform; }

	uint32_t config_read(uint32_t offset, unsigned len)
	{
		Platform::Device::Access_size sz = Platform::Device::ACCESS_32BIT;

		switch (len) {
		case 2: sz = Platform::Device::ACCESS_16BIT; break;
		case 4: sz = Platform::Device::ACCESS_32BIT; break;
		}

		return _device.config_read(offset, sz);
	}

	void config_write(uint32_t offset, unsigned val, unsigned len)
	{
		Platform::Device::Access_size sz = Platform::Device::ACCESS_32BIT;

		switch (len) {
		case 1: sz = Platform::Device::ACCESS_8BIT;  break;
		case 2: sz = Platform::Device::ACCESS_16BIT; break;
		case 4: sz = Platform::Device::ACCESS_32BIT; break;
		}

		_device.config_write(offset, val, sz);
	}

	Genode::Signal_receiver &sig_rec() { return _sig_rec; }

	void enable_interrupts()
	{
		_irq.sigh(_irq_dispatcher);
		_irq.ack_irq();
	}

	void ack_irq()
	{
		Genode::Lock::Guard g(_irq_lock);

		if (_irq_pending) {
			_irq_pending = false;

			_irq.ack_irq();

			PDMDevHlpPCISetIrqNoWait(_vbox_pci_dev, 0, 0);
		}
	}

	Genode::addr_t igd_opregion() { return (Genode::addr_t)_igd_opregion; }

	Genode::addr_t igd_bdsm() { return (Genode::addr_t)_igd_bdsm + (8<<20); }
	Genode::addr_t igd_bgsm() { return (Genode::addr_t)_igd_bdsm; }

	uint32_t igd_gtt_max() { return _igd_gtt_max; }
	uint32_t igd_gmch_ctl() { return _igd_gmch_ctl; }

	uint16_t lpc_device_id() { return _lpc_device_id; }
};


/***********************************************
 ** Virtualbox Device function implementation **
 ***********************************************/

/*
 * GTTMMADR_0_2_0_PCI
 */

/**
 * @callback_method_impl{FNIOMMMIOREAD}
 */
PDMBOTHCBDECL(int) gpuReadGttmmaddr(PPDMDEVINS pDevIns, void *pvUser,
                                    RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_memory &iom = *pThis->gttmmadr;

	Genode::off_t const offset = GCPhysAddr - pThis->gttmmadr_base;

	unsigned *upv = (unsigned*)pv;

	switch (cb) {
	case 1: *upv = iom.read_1(offset); break;
	case 2: *upv = iom.read_2(offset); break;
	case 4: *upv = iom.read_4(offset); break;
	}

	if (verbose_gttmmadr) {
		Genode::log(__func__,
		            ":  base: ",  Genode::Hex(pThis->gttmmadr_base),
		            " offset: ", Genode::Hex(offset),
		            " pv: ",     Genode::Hex(*upv),
		            " cb: ",     Genode::Hex(cb));
	}

	return 0;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
PDMBOTHCBDECL(int) gpuWriteGttmmaddr(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr,
                                 void const *pv, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_memory &iom = *pThis->gttmmadr;

	Genode::off_t const offset = GCPhysAddr - pThis->gttmmadr_base;

	if (verbose_gttmmadr) {
		Genode::log(__func__,
		            ": base: ",  Genode::Hex(pThis->gttmmadr_base),
		            " offset: ", Genode::Hex(offset),
		            " pv: ",     Genode::Hex(*(unsigned int*)pv),
		            " cb: ",     Genode::Hex(cb));
	}

	switch (cb) {
	case 1: iom.write_1(offset, *(unsigned char*)pv);  break;
	case 2: iom.write_2(offset, *(unsigned short*)pv); break;
	case 4: iom.write_4(offset, *(unsigned int*)pv);   break;
	}

	/* ack injected IRQ */
	if (offset == IGD_GEN8_MASTER_IRQ) {
		pThis->gpu->ack_irq();
	}

	return 0;
}


extern void guest_memory_dump();


/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) gpuMapGttmmaddr(PPCIDEVICE pPciDev, int iRegion,
                                         RTGCPHYS GCPhysAddress, uint32_t cb,
                                         PCIADDRESSSPACE enmType)
{
	PGPU pThis = (PGPU)pPciDev;
	int rc = PDMDevHlpMMIORegister(pThis->CTX_SUFF(pDevIns), GCPhysAddress, cb, 0 /*pvUser*/,
	                               IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED | IOMMMIO_FLAGS_DBGSTOP_ON_COMPLICATED_WRITE,
	                               gpuWriteGttmmaddr, gpuReadGttmmaddr, "GTTMMADR");
	if (RT_FAILURE(rc))
		return rc;

	pThis->gttmmadr_base = GCPhysAddress;
	Genode::info(__func__,
	            ": gttmmadr_base: ", Genode::Hex(pThis->gttmmadr_base),
	            " cb: ",             Genode::Hex(cb));

	guest_memory_dump();
	return VINF_SUCCESS;
}


/*
 * GMADR_0_2_0_PCI
 */

/**
 * @callback_method_impl{FNPGMR3PHYSHANDLER, HC access handler for the LFB.}
 */
static DECLCALLBACK(int) gpuHandleGmadr(PVM pVM, RTGCPHYS GCPhys, void *pvPhys,
                                        void *pvBuf, size_t cbBuf,
                                        PGMACCESSTYPE enmAccessType, void *pvUser)
{
	PGPU pThis = (PGPU)pvUser;

	return VINF_PGM_HANDLER_DO_DEFAULT;
}


extern void vmm_alloc_mmio(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPHYS base, size_t size, uint32_t iRegion);


static DECLCALLBACK(int) gpuMapGmadr(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress,
                                       uint32_t cb, PCIADDRESSSPACE enmType)
{
	PGPU pThis = (PGPU)pPciDev;

	int rc = -1;

	PPDMDEVINS pDevIns = pPciDev->pDevIns;
	PVM            pVM = PDMDevHlpGetVM(pDevIns);

	/* unmap */
	if (GCPhysAddress == NIL_RTGCPHYS) {
		rc = PGMHandlerPhysicalDeregister(pVM, pThis->gmadr_base);
		AssertRC(rc);
		pThis->gmadr_base = 0;
		return rc;
	}

	/*
	 * We have to add the MMIO region to the VMM memory map so that
	 * may later register an handler for that region.
	 */
	if (!PGMR3PhysMMIO2IsBase(pVM, pDevIns, GCPhysAddress)) {
		Genode::error(__func__, ": could not lookup GCPhysAddress: ", Genode::Hex(GCPhysAddress));
		vmm_alloc_mmio(pDevIns, GCPhysAddress, pThis->gmadr->vaddr(), cb, iRegion);
	}

	/* map */
	rc = PGMR3PhysMMIO2Map(pVM, pDevIns, iRegion, GCPhysAddress);
	if (RT_FAILURE(rc)) {
		return rc;
	}

	rc = PGMR3HandlerPhysicalRegister(pVM,
	                                  PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
	                                  GCPhysAddress, GCPhysAddress + (cb - 1),
	                                  gpuHandleGmadr, pThis,
	                                  /* arguments below are not required by our backend */
	                                  0 /* pszR0Mod */,     0 /* pszHandlerR0 */,
	                                  0 /* pvUserR0 */,     0 /* pszRCMod */,
	                                  0 /* pszHandlerRC */, 0 /* pvUserRC */,
	                                  0 /* pszDesc */);
	if (RT_FAILURE(rc)) {
		return rc;
	}

	pThis->gmadr_base = GCPhysAddress;
	Genode::info(__func__,": gmadr_base: ", Genode::Hex(pThis->gmadr_base),
	            " cb: ", Genode::Hex(cb));
	return VINF_SUCCESS;
}


/*
 * IOBAR_0_2_0_PCI
 */

/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
PDMBOTHCBDECL(int) gpuReadIobar(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port,
                              uint32_t *pu32, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_port *io = pThis->iobar;

	Genode::off_t offset = Port - pThis->IOBase4;

	switch (cb) {
	case 1: *pu32 = io->read_1(offset); break;
	case 2: *pu32 = io->read_2(offset); break;
	case 4: *pu32 = io->read_4(offset); break;
	}

	if (verbose_io) {
		Genode::log(__func__,": base: ", Genode::Hex(pThis->IOBase4),
		            " offset: ", Genode::Hex(offset),
		            " cb: ",     Genode::Hex(cb),
		            " pu32: ",   Genode::Hex(*pu32),
		            " (",        Genode::Binary(*pu32), ")");
	}

	pThis->iobar_index = ~0;

	return 0;
}


/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) gpuWriteIobar(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port,
                               uint32_t u32, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);
	Gpu &gpu   = *pThis->gpu;

	Io_port *io = pThis->iobar;

	Genode::off_t offset = Port - pThis->IOBase4;

	if (verbose_io) {
		Genode::log(__func__,": base: ", Genode::Hex(pThis->IOBase4),
		            " offset: ", Genode::Hex(offset),
		            " cb: ",     Genode::Hex(cb),
		            " u32: ",    Genode::Hex(u32),
		            " (",        Genode::Binary(u32), ")");
	}

	enum { IO_INDEX = 0, IO_DATA = 4, };
	if (offset == IO_INDEX) {
		pThis->iobar_index = u32;
	} else if (offset == IO_DATA) {

		uint32_t const index   = pThis->iobar_index;
		uint32_t const gtt_max = gpu.igd_gtt_max();

		if (index % 4 == 1 && index < gtt_max) {
			Genode::addr_t const bdsm_addr = gpu.igd_bgsm();
			uint32_t v = (index % 8 == 1) ? bdsm_addr | (u32 & ((1U<<20) - 1)) : 0;

			// Genode::info(__func__, ": override offset: ", Genode::Hex(index),
			// 			" u32: ", Genode::Hex(u32),
			// 			" with: ", Genode::Hex(v));
			u32 = v;
		}

		pThis->iobar_index = ~0;
	}

	switch (cb) {
	case 1: io->write_1(offset, (uint8_t) u32); break;
	case 2: io->write_2(offset, (uint16_t)u32); break;
	case 4: io->write_4(offset,           u32); break;
	}

	return 0;
}


/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) gpuMapIobar(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress,
                                   uint32_t cb, PCIADDRESSSPACE enmType)
{
	PGPU pThis = (PGPU)pPciDev;

	Genode::warning("GCPhysAddress: ", Genode::Hex(GCPhysAddress));

	/* x250 = 0x3000, shuttle = 0xf000 */
	RTIOPORT port = pThis->iobar->base();

	int rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns, port, 8,
	                                 (RTGCPTR)NIL_RTRCPTR, gpuWriteIobar, gpuReadIobar,
	                                 NULL, NULL, "IOBAR");
	if (RT_FAILURE(rc))
		return rc;

	pThis->IOBase4 = port;
	Genode::info(__func__, ": IOBase4: ", Genode::Hex(pThis->IOBase4), " cb: ", Genode::Hex(cb));
	return VINF_SUCCESS;
}


/******************
 ** VGA Port I/O **
 ******************/

enum {
	VGA_PORT_IO_START = 0x3b0,
	VGA_PORT_IO_END   = 0x3df,
	VGA_PORT_IO_SIZE  = VGA_PORT_IO_END - VGA_PORT_IO_START
};

PDMBOTHCBDECL(int) vgaPortIoRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port,
                                 uint32_t *pu32, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_port * const io = pThis->vga_port_io;

	Genode::off_t offset = Port - VGA_PORT_IO_START;

	switch (cb) {
	case 1:
		*pu32 = io->read_1(offset);
		break;
	case 2:
		*pu32 = io->read_2(offset);
		break;
	case 4:
		Genode::warning("writing ", cb, " bytes not supported");
		return -1;
	}

	if (verbose_vga_io) {
		Genode::log(__func__,": port: ", Genode::Hex(Port),
		            " offset: ", Genode::Hex(offset),
		            " cb: ",     Genode::Hex(cb),
		            " pu32: ",   Genode::Hex(*pu32));
	}

	return 0;
}


PDMBOTHCBDECL(int) vgaPortIoWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port,
                                  uint32_t u32, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);
	Io_port * const io = pThis->vga_port_io;

	Genode::off_t offset = Port - VGA_PORT_IO_START;

	if (verbose_vga_io) {
		Genode::log(__func__,": base: ", Genode::Hex(Port),
		            " offset: ", Genode::Hex(offset), " cb: ", Genode::Hex(cb),
		            " u32: ", Genode::Hex(u32));
	}

	switch (cb) {
	case 1:
		io->write_1(offset, (unsigned char)u32);
		break;
	case 2:
		io->write_2(offset, (unsigned short)u32);
		break;
	default:
		Genode::warning("writing ", cb, " bytes not supported");
		return -1;
	}

	return 0;
}


/*********************
 ** VGA BUFFER MMIO **
 *********************/

enum {
	VGA_BUFFER_START = 0xa0000,
	VGA_BUFFER_END   = 0xbffff,
	VGA_BUFFER_SIZE  = VGA_BUFFER_END - VGA_BUFFER_START,
};


PDMBOTHCBDECL(int) vgaMmioFill(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr,
                               uint32_t u32Item, unsigned cbItem, unsigned cItems)
{
	Genode::error(__func__);
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_memory *io = pThis->vga_buffer;
}


PDMBOTHCBDECL(int) vgaMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr,
                               void *pv, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_memory *iom = pThis->vga_buffer;

	Genode::off_t offset = GCPhysAddr - VGA_BUFFER_START;

	unsigned *upv = (unsigned*)pv;

	switch (cb) {
	case 1: *upv = iom->read_1(offset); break;
	case 2: *upv = iom->read_2(offset); break;
	case 4: *upv = iom->read_4(offset); break;
	}

	if (verbose_vga_mmio) {
		Genode::log(__func__,": base: ", Genode::Hex(VGA_BUFFER_START),
		            " offset: ", Genode::Hex(offset), " cb: ", Genode::Hex(cb),
		            " pv: ", Genode::Hex(*upv));
	}

	return 0;
}


PDMBOTHCBDECL(int) vgaMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr,
                                void const *pv, unsigned cb)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);

	Io_memory *iom = pThis->vga_buffer;

	Genode::off_t offset = GCPhysAddr - VGA_BUFFER_START;

	if (verbose_vga_mmio) {
		Genode::log(__func__,": base: ", Genode::Hex(VGA_BUFFER_START),
		            " offset: ",     Genode::Hex(offset),
		            " GCPhysAddr: ", Genode::Hex(GCPhysAddr),
		            " cb: ",         Genode::Hex(cb));
	}

	switch (cb) {
	case 1: iom->write_1(offset, *(unsigned char*)pv);  break;
	case 2: iom->write_2(offset, *(unsigned short*)pv); break;
	case 4: iom->write_4(offset, *(unsigned int*)pv);   break;
	}

	return 0;
}


static void install_vbios(PPDMDEVINS pDevIns)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);


	if (pThis->vbios_rom_installed) { return; }

	Genode::info("Map external (IGD) VGA BIOS");
	int rc = PDMDevHlpROMRegister(pDevIns, 0xc0000, pThis->vbios_rom_size,
	                              pThis->vbios_rom, pThis->vbios_rom_size, 0, "VGA BIOS");
	if (RT_FAILURE(rc)) {
		Genode::error("could not map external VGA BIOS ROM");
	}

	pThis->vbios_rom_installed = true;
}


/**********************
 ** PCI config space **
 **********************/


static DECLCALLBACK(uint32_t) pci_read_config(PCIDevice *d, uint32_t address, unsigned len)
{
	PGPU pThis = PDMINS_2_DATA(d->pDevIns, PGPU);
	Gpu &gpu   = *pThis->gpu;

	if (verbose_pci_cfg)
		Genode::log("         ", __func__, ": pThis: ", pThis, " address: ",
		            Genode::Hex(address, Genode::Hex::PREFIX, Genode::Hex::PAD),
		            " len: ", len);

	/*
	 * In case gpuR3Construct did not take care of installing the
	 * vBIOS do it now on the first config space read
	 */
	install_vbios(d->pDevIns);

	/* get BAR# from emulated config space */
	switch (address) {
	case 0x00:
	case 0x02:
	case 0x04:
	case 0x0e:
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1C:
	case 0x20:
	case 0x24:
	case 0x3c:
	case 0x3d:
		return pThis->pfnConfigRead(d, address, len);
	case IGD_ASLS:
		Genode::warning("disable OpRegion by overriding IGD_ASLS with 0");
		return 0;
		return (uint32_t)gpu.igd_opregion();
	case IGD_GMCH:
		return gpu.igd_gmch_ctl();
	case IGD_BDSM:
		return (uint32_t)gpu.igd_bdsm();
	case IGD_BGSM:
		return (uint32_t)gpu.igd_bgsm();
	}

	/* forward to device */
	return gpu.config_read(address, len);
}


static DECLCALLBACK(void) pci_write_config(PCIDevice *d, uint32_t address, uint32_t val, unsigned len)
{
	PGPU pThis = PDMINS_2_DATA(d->pDevIns, PGPU);
	Gpu &gpu   = *pThis->gpu;

	if (verbose_pci_cfg)
		Genode::log("         ", __func__, ": pThis: ", pThis, " address: ",
		            Genode::Hex(address, Genode::Hex::PREFIX, Genode::Hex::PAD),
		            " val: ", Genode::Hex(val), " len: ", len);

	/* set BAR# to emulated config space */
	switch (address) {
	case 0x00:
	case 0x02:
	case 0x04: /* PCI_CMD */
		try {
			gpu.config_write(address, val, len); }
		catch (...) {
			Genode::error(__func__, ": pThis: ", pThis,
			              " could not write address: ", Genode::Hex(address),
			              " val: ", Genode::Hex(val), " len: ", len);
		}
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1C:
	case 0x20:
	case 0x24:
	case 0x3C:
		pThis->pfnConfigWrite(d, address, val, len);
		return;
	}

	/* forward to device */
	try { gpu.config_write(address, val, len); }
	catch (...) {
		Genode::error(__func__, ": pThis: ", pThis,
		              " could not write address: ", Genode::Hex(address),
		              " val: ", Genode::Hex(val), " len: ", len);
	}
}


static DECLCALLBACK(int) irq_inject_thread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);
	if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
		return VINF_SUCCESS;

	Gpu &gpu   = *pThis->gpu;
	while (pThread->enmState == PDMTHREADSTATE_RUNNING)
	{
		Genode::Signal sig = gpu.sig_rec().wait_for_signal();
		int num            = sig.num();

		Genode::Signal_dispatcher_base *dispatcher;
		dispatcher = dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context());
		if (dispatcher) {
			dispatcher->dispatch(num);
		}
	}

	return VINF_SUCCESS;
}


extern void lpc_set_device_id(uint16_t);
extern bool vcpu_assign_pci(unsigned int cpu_id, Genode::addr_t pci_config_memory, uint16_t bdf);

Genode::addr_t _vga_buffer_addr;


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct,GPU constructor}
 */
static DECLCALLBACK(int) gpuR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
	PGPU pThis = PDMINS_2_DATA(pDevIns, PGPU);
	PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

	static Gpu gpu(pDevIns);

	if (!gpu.device().valid()) {
		Genode::error("could not construct PCI GPU-pass-through device");
		return -1;
	}

	pThis->gpu = &gpu;

	int rc = 0;

	enum { VIDEO_ROM_BASE = 0xc0000, VIDEO_ROM_SIZE = 0x20000 };
	static Genode::Attached_io_mem_dataspace vrom(VIDEO_ROM_BASE, VIDEO_ROM_SIZE);
	pThis->vbios_rom      = vrom.local_addr<uint8_t>();
	pThis->vbios_rom_size = VIDEO_ROM_SIZE;

	rc = PDMDevHlpThreadCreate(pDevIns, &pThis->pThread, pThis,
	                               irq_inject_thread, nullptr,
	                               2*1024*sizeof(long) , RTTHREADTYPE_IO, "irq_inject");
	if (RT_FAILURE(rc))
		return rc;

	/*
	 * Override LPC/ISA bridge device id
	 */
	if (gpu.lpc_device_id() == 0) {
		Genode::error("could not override LPC/ISA bridge device id");
		return -1;
	}

	lpc_set_device_id(gpu.lpc_device_id());
	Genode::info("Override LPC/ISA bridge device id with ", Genode::Hex(gpu.lpc_device_id()));

	static Genode::Io_port_connection vga_port_io(VGA_PORT_IO_START, VGA_PORT_IO_SIZE);
	pThis->vga_port_io = new (Genode::env()->heap()) Io_port(VGA_PORT_IO_START, vga_port_io.cap());

	/*
	 * Map the IGD vBIOS
	 */
	install_vbios(pDevIns);

	/* finally enable device interrupts */
	gpu.enable_interrupts();

	/*
	 * Init instance data.
	 */
	pThis->pDevInsR3 = pDevIns;
	pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);

	PCIDevSetVendorId      (&pThis->PciDev, gpu.device().vendor_id());
	PCIDevSetDeviceId      (&pThis->PciDev, gpu.device().device_id());
	PCIDevSetClassProg     (&pThis->PciDev, 0x00);
	PCIDevSetClassSub      (&pThis->PciDev, gpu.device().sub_class());
	PCIDevSetClassBase     (&pThis->PciDev, gpu.device().base_class());
	PCIDevSetInterruptPin  (&pThis->PciDev, 0x01);
	PCIDevSetHeaderType    (&pThis->PciDev, 0x80); /* XXX read from gpu */
#ifdef VBOX_WITH_MSI_DEVICES
	PCIDevSetStatus        (&pThis->PciDev, VBOX_PCI_STATUS_CAP_LIST);
	PCIDevSetCapabilityList(&pThis->PciDev, 0x80);
#endif

	PCIDevSetDWord         (&pThis->PciDev, IGD_ASLS, (uint32_t)gpu.igd_opregion());
	PCIDevSetDWord         (&pThis->PciDev, IGD_BDSM, (uint32_t)gpu.igd_bdsm());
	PCIDevSetDWord         (&pThis->PciDev, IGD_BGSM, (uint32_t)gpu.igd_bgsm());

	/*
	 * Register PCI device and I/O region.
	 */
	rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
	if (RT_FAILURE(rc))
		return rc;

#ifdef VBOX_WITH_MSI_DEVICES
	PDMMSIREG MsiReg;
	RT_ZERO(MsiReg);
	MsiReg.cMsiVectors    = 1;
	MsiReg.iMsiCapOffset  = 0x80;
	MsiReg.iMsiNextOffset = 0x00;
	rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
	if (RT_FAILURE(rc))
	{
		/* That's OK, we can work without MSI */
		PCIDevSetCapabilityList(&pThis->PciDev, 0x0);
	}
#endif

	/*
	 * Map VGA MMIO buffer
	 */
	static Genode::Io_mem_connection vga_buffer(VGA_BUFFER_START, VGA_BUFFER_SIZE);
	pThis->vga_buffer = new (Genode::env()->heap())
		Io_memory(*Genode::env()->rm_session(), VGA_BUFFER_START, vga_buffer.cap());

	_vga_buffer_addr = pThis->vga_buffer->vaddr();

	rc = PDMDevHlpMMIORegisterEx(pDevIns, 0xa0000, 0x20000, (RTHCPTR)_vga_buffer_addr,
	                             IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
	                             vgaMmioWrite, vgaMmioRead, vgaMmioFill, "VGA Buffer");
	if (RT_FAILURE(rc)) {
		Genode::error("could not map VGA buffer");
		return rc;
	}

	/*
	 * Map VGA I/O Ports (see vol 3)
	 */
	// enum {
	// 	VGA_CRTC_IDX_MONO     = 0x3b4,
	// 	VGA_STATUS_REG        = 0x3ba,
	// 	VGA_ATTR_CTL_IDX      = 0x3c0,
	// 	VGA_ATTR_CTL_DATA     = 0x3c1,
	// 	VGA_COLOR_PAL_WM      = 0x3c8,
	// 	VGA_COLOR_PAL_DATA    = 0x3c9,
	// 	VGA_GRAPHIC_CTL_IDX   = 0x3ce,
	// 	VGA_CRTC_IDX          = 0x3d4,
	// 	GFX_2D_CONF_EXT_IDX   = 0x3d6,
	// 	VGA_STATUS_REG_2      = 0x3da,
	// };

	rc = PDMDevHlpIOPortRegister(pDevIns, VGA_PORT_IO_START, VGA_PORT_IO_SIZE, NULL,
	                             vgaPortIoWrite, vgaPortIoRead, NULL, NULL, "VGA - 0x3b0-0x3df");
	if (RT_FAILURE(rc)) {
		Genode::error("could not map VGA I/O ports");
		return rc;
	}

	/*
	 * Map PCI resources
	 */
	try {
		/* BAR0 GTTMMADR_0_2_0_PCI (vol 2c) */
		uint8_t const              vgttmaddr = gpu.device().phys_bar_to_virt(0);
		Platform::Device::Resource  gttmmadr = gpu.device().resource(0);

		pThis->gttmmadr = new (Genode::env()->heap())
			Io_memory(*Genode::env()->rm_session(), gttmmadr.base(),
			          gpu.device().io_mem(vgttmaddr, Genode::Cache_attribute::UNCACHED));

		Genode::info(__func__,
		            ": gttmmadr: ", Genode::Hex(gttmmadr.base()),
		            " size: ",      Genode::Hex(gttmmadr.size()),
		            " vaddr: ",     Genode::Hex(pThis->gttmmadr->vaddr()));

		rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, gttmmadr.size(),
		                                  PCI_ADDRESS_SPACE_MEM, gpuMapGttmmaddr);
		if (RT_FAILURE(rc))
			return rc;

		/*
		 * Clear GTT
		 *
		 * Note: the vBIOS will programm the GTT later on via BAR4.
		 */
		Genode::addr_t bgsm_addr = gpu.igd_bgsm();

		uint32_t const gtt_offset = gttmmadr.size() / 2;
		Genode::addr_t ggtt = pThis->gttmmadr->vaddr() + gtt_offset;
		uint32_t const gtt_max = gpu.igd_gtt_max();

		Genode::info("************************************************ gtt_max: ", gtt_max, " entries");
		// for (int i = 0; i < gtt_max*8; i+=8) {
		// 	volatile uint64_t *pte = (volatile uint64_t*)(ggtt+i);
		// 	uint64_t old_pte = *pte;

		// 	*pte = bgsm_addr | (*pte & ((1U<<20) - 1));

		// 	// Genode::info("i: ", (i/8), " old: ", Genode::Hex(old_pte),
		// 	//             " new: ", Genode::Hex(*pte), " ", Genode::Hex(*pte & 0x7ffffff000));
		// }

		/* BAR2 GMADR_0_2_0_PCI (vol 2c) */
		uint8_t const              vgmadr = gpu.device().phys_bar_to_virt(2);
		Platform::Device::Resource  gmadr = gpu.device().resource(2);

		pThis->gmadr = new (Genode::env()->heap())
			Io_memory(*Genode::env()->rm_session(), gmadr.base(),
			          gpu.device().io_mem(vgmadr, Genode::Cache_attribute::CACHED));

		Genode::info(__func__,
		            ": gmadr: ", Genode::Hex(gmadr.base()),
		            " size: ",   Genode::Hex(gmadr.size()),
		            " vaddr: ",  Genode::Hex(pThis->gmadr->vaddr()));

		/* GMADR BAR is actually 256MiB */
		rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, gmadr.size() / 2,
		                                  PCI_ADDRESS_SPACE_MEM, gpuMapGmadr);
		if (RT_FAILURE(rc))
			return rc;


		/* BAR4 IOBAR_0_2_0_PCI (vol 2c) */
		uint8_t const              viobar = gpu.device().phys_bar_to_virt(4);
		Platform::Device::Resource  iobar = gpu.device().resource(4);

		pThis->iobar = new (Genode::env()->heap())
			Io_port(iobar.base(), gpu.device().io_port(viobar));

		Genode::info(__func__, ": iobar: ", Genode::Hex(iobar.base()),
		            " size: ", Genode::Hex(iobar.size()));

		for (uint32_t i = 0; i < gtt_max; i++) {
			pThis->iobar->write_4(0, i);
			pThis->iobar->write_4(4, 0);
		}

		rc = PDMDevHlpPCIIORegionRegister(pDevIns, 4, iobar.size(),
		                                  PCI_ADDRESS_SPACE_IO, gpuMapIobar);
		if (RT_FAILURE(rc))
			return rc;

	} catch(...) {
		Genode::error("could not map I/O resources");
		return -1;
	}

	PDMDevHlpPCISetConfigCallbacks(pDevIns, &pThis->PciDev,
	                               pci_read_config, &pThis->pfnConfigRead,
	                               pci_write_config, &pThis->pfnConfigWrite);

	/* assign device to PD */
	{
		unsigned char bus, device, function;
		gpu.device().bus_address(&bus, &device, &function);
		uint16_t const bdf = (bus << 8) | (device << 3) | (function & 0x7);

		Genode::Io_mem_dataspace_capability cap = gpu.platform().config_extended(gpu.device_cap());
		if (!cap.valid()) {
			Genode::error("could not get GPU config_space");
			return -1;
		}

		Genode::addr_t page = Genode::env()->rm_session()->attach(cap);

		/* trigger mapping, needed for assign_pci() */
		*(volatile unsigned*)page;

		if (!vcpu_assign_pci(0, page, bdf)) {
			Genode:: Dataspace_client ds_client(cap);
			Genode::error("could not assign GPU to VMM, phys: ",
			              Genode::Hex(ds_client.phys_addr()),
			              " virt: ", Genode::Hex(page));
			return -1;
		}

		Genode::env()->rm_session()->detach(page);
	}

	Genode::info("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX VINF_SUCCESS");
	return VINF_SUCCESS;
}

const PDMDEVREG g_DeviceGPU =
{
	/* u32version */
	PDM_DEVREG_VERSION,
	/* szName */
	"vga",
	/* szRCMod */
	"VBoxDDGC.gc",
	/* szR0Mod */
	"VBoxDDR0.r0",
	/* pszDescription */
	"GPU pass-through device.\n",
	/* fFlags */
	PDM_DEVREG_FLAGS_DEFAULT_BITS,
	/* fClass */
	PDM_DEVREG_CLASS_BUS_USB,
	/* cMaxInstances */
	1,
	/* cbInstance */
	sizeof(GPU),
	/* pfnConstruct */
	gpuR3Construct,
	/* pfnDestruct */
	NULL,
	/* pfnRelocate */
	NULL,
	/* pfnMemSetup */
	NULL,
	/* pfnPowerOn */
	NULL,
	/* pfnReset */
	NULL,
	/* pfnSuspend */
	NULL,
	/* pfnResume */
	NULL,
	/* pfnAttach */
	NULL,
	/* pfnDetach */
	NULL,
	/* pfnQueryInterface */
	NULL,
	/* pfnInitComplete */
	NULL,
	/* pfnPowerOff */
	NULL,
	/* pfnSoftReset */
	NULL,
	/* u32VersionEnd */
	PDM_DEVREG_VERSION
};

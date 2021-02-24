#include <base/component.h>
#include <util/mmio.h>

using namespace Genode;

struct Descriptor : Genode::Mmio
{
	enum Type {
		DEVICE = 0x01,
		CONFIG = 0x02,
		STRING = 0x03,
		IFACE  = 0x04,
		ENDPT  = 0x05,
		DEVQ   = 0x06,
		OSC    = 0x07,
		IFPWR  = 0x08,
		OTG    = 0x09,
		DBG    = 0x0a,
		IFA    = 0x0b,
	};
	struct bLength            : Register< 0,  8> { };
	struct bDescriptorType    : Register< 1,  8> { };

	Descriptor(addr_t base) : Mmio { base } { }

	void print(Genode::Output &out) const
	{
		using namespace Genode;

		Genode::print(out, "bLength: ",          read<bLength>(), "\n");
		Genode::print(out, "bDescriptorType: ",  Hex(read<bDescriptorType>()), "\n");
	}
};


struct Device_descriptor : Descriptor
{
	struct bcdUSB             : Register< 2, 16> { };
	struct bDeviceClass       : Register< 4,  8> { };
	struct bDeviceSubClass    : Register< 5,  8> { };
	struct bDeviceProtocol    : Register< 6,  8> { };
	struct bMaxPacketSize0    : Register< 7,  8> { };
	struct idVendor           : Register< 8, 16> { };
	struct idProduct          : Register<10, 16> { };
	struct bcdDevice          : Register<12, 16> { };
	struct iManufacturer      : Register<14, 16> { };
	struct iProduct           : Register<15,  8> { };
	struct iSerial            : Register<16,  8> { };
	struct bNumConfigurations : Register<17,  8> { };

	Device_descriptor(addr_t base) : Descriptor { base } { }

	void print(Genode::Output &out) const
	{
		using namespace Genode;

		Genode::print(out, "bLength: ",          read<bLength>(), "\n");
		Genode::print(out, "bDescriptorType: ",  Hex(read<bDescriptorType>()), "\n");
		Genode::print(out, "bcdUSB: ",           Hex(read<bcdUSB>()), "\n");
		Genode::print(out, "bDeviceClass: ",     Hex(read<bDeviceClass>()), "\n");
	}
};


struct Device_qualifier : Genode::Mmio
{
	/* TODO */
};


struct Configuration_descriptor : Genode::Mmio
{
	struct bLength             : Register<0,  8> { };
	struct bDescriptorType     : Register<1,  8> { };
	struct wTotalLength        : Register<2, 16> { };
	struct bNumInterfaces      : Register<4,  8> { };
	struct bConfigurationValue : Register<5,  8> { };
	struct iConfiguration      : Register<6,  8> { };
	struct bMAttributes        : Register<7,  8> { };
	struct MaxPower            : Register<8,  8> { };

	Configuration_descriptor(addr_t base) : Mmio { base } { }

	void print(Genode::Output &out) const
	{
		using namespace Genode;

		Genode::print(out, "bLength: ",             read<bLength>(), "\n");
		Genode::print(out, "bDescriptorType: ",     Hex(read<bDescriptorType>()), "\n");
		Genode::print(out, "wTotalLength: ",        read<wTotalLength>(), "\n");
		Genode::print(out, "bNumInterfaces: ",      read<bNumInterfaces>(), "\n");
		Genode::print(out, "bConfigurationValue: ", read<bConfigurationValue>(), "\n");
	}
};


struct Other_speed_configuration_descriptor : Genode::Mmio
{
	/* TODO (same as Configuration_descriptor, Type is 7) */
};


struct Interface_descriptor : Genode::Mmio
{
	struct bLength             : Register<0, 8> { };
	struct bDescriptorType     : Register<1, 8> { };
	struct bInterfaceNumber    : Register<2, 8> { };
	struct bAlternateSetting   : Register<3, 8> { };
	struct bNumEndpoints       : Register<4, 8> { };
	struct bInterfaceClass     : Register<5, 8> { };
	struct bInterfaceSubClass  : Register<6, 8> { };
	struct bInterfaceProtocol  : Register<7, 8> { };
	struct iInterface          : Register<8, 8> { };

	Interface_descriptor(addr_t base) : Mmio { base } { }
};


struct Audio_control_interface_descriptor : Interface_descriptor
{
	enum {
		INTERFACE_CLASS     = 1,
		INTERFACE_SUB_CLASS = 1,
	};

	Audio_control_interface_descriptor(addr_t base)
	: Interface_descriptor { base } { }
};


struct Audio_streaming_interface_descriptor : Interface_descriptor
{
	enum {
		INTERFACE_CLASS     = 1,
		INTERFACE_SUB_CLASS = 2,
	};

	Audio_streaming_interface_descriptor(addr_t base)
	: Interface_descriptor { base } { }
};


struct Hid_interface_descriptor : Interface_descriptor
{
	enum {
		INTERFACE_CLASS     = 3,
		INTERFACE_SUB_CLASS = 0,
	};

	Hid_interface_descriptor(addr_t base)
	: Interface_descriptor { base } { }
};


struct Endpoint_descriptor : Genode::Mmio
{
	struct bLength          : Register<0,  8> { };
	struct bDescriptorType  : Register<1,  8> { };
	struct bEndpointAddress : Register<2,  8>
	{
		struct Address   : Bitfield<0, 4> { };
		struct Direction : Bitfield<4, 4> { };
	};
	struct bmAttributes    : Register< 3,  8>
	{
		struct Transfer_type : Bitfield<0, 2>
		{
			enum {
				ISOCH = 0b01,
				INTR  = 0b11,
			};
		};
		struct Sync_type : Bitfield<2, 2>
		{
			enum {
				ASYNC = 0b01,
				ADAPT = 0b10,
				SYNC  = 0b11,
			};
		};
	};
	struct wMaxPacketSize   : Register<4, 16> { };
	struct bInterval        : Register<6,  8> { };

	Endpoint_descriptor(addr_t base) : Mmio { base } { }
};


struct String_descriptor : Genode::Mmio
{
	struct bLength          : Register<0,  8> { };
	struct bDescriptorType  : Register<1,  8> { };

	struct wLangID0 : Register<2, 16> { };
	struct wLangID1 : Register<4, 16> { };
	/* struct wLangIDn : Register<n, 16> { }; */

	/* bString : N */

	String_descriptor(addr_t base) : Mmio { base } { }
};


struct Audio_streaming_general : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  1, /* AS_GENERAL */
	};
	struct bLength            : Register<0,  8> { };
	struct bDescriptorType    : Register<1,  8> { };
	struct bDescriptorSubtype : Register<2,  8> { };
	struct bTerminalLink      : Register<3,  8> { };
	struct bDelay             : Register<4,  8> { };
	struct wFormatTag         : Register<5, 16> { };

	Audio_streaming_general(addr_t base)
	: Mmio { base } { }
};


struct Audio_streaming_format_type : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  2, /* FORMAT_TYPE */

		FORMAT_TYPE_I = 1,
	};
	struct bLength            : Register< 0,  8> { };
	struct bDescriptorType    : Register< 1,  8> { };
	struct bDescriptorSubtype : Register< 2,  8> { };
	struct bFormatType        : Register< 3,  8> { };
	struct bNrChannels        : Register< 4,  8> { };
	struct bSubframesize      : Register< 5,  8> { };
	struct bBitResolution     : Register< 6,  8> { };
	struct bSamFreqType       : Register< 7,  8> { };
	struct tSamFreq0          : Register< 8, 32>
	{
		struct Value : Bitfield<0, 32> { };
	};
	struct tSamFreq1          : Register<11, 32>
	{
		struct Value : Bitfield<0, 32> { };
	};

	Audio_streaming_format_type(addr_t base)
	: Mmio { base } { }
};


struct Audio_streaming_endpoint : Endpoint_descriptor
{
	enum {
		DESCRIPTOR_TYPE     = 5, /* ENDPOINT */
	};
	struct bRefresh       : Register<7,  8> { };
	struct bSyncAddress   : Register<8,  8> { };

	Audio_streaming_endpoint(addr_t base)
	: Endpoint_descriptor { base } { }
};


struct Audio_streaming_cs_endpoint : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 37, /* CS_ENDPOINT */
		DESCRIPTOR_SUB_TYPE = 1,  /* EP_GENERAL */
	};
	struct bLength            : Register< 0,  8> { };
	struct bDescriptorType    : Register< 1,  8> { };
	struct bDescriptorSubType : Register< 2,  8> { };
	struct bmAttributes       : Register< 3,  8>
	{
		enum {
			SAMPLING_FREQ    = 1u << 0,
			PITCH            = 1u << 1,
			MAX_PACKETS_ONLY = 1u << 7,
		};
	};
	struct bLockDelayUnits    : Register< 4,  8>
	{
		enum {
			UNDEF = 0,
			MS    = 1,
			PCM   = 2,
		};
	};
	struct wLockDelay         : Register< 5,  8> { };

	Audio_streaming_cs_endpoint(addr_t base)
	: Mmio { base } { }
};


struct Audio_control_header : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  1, /* HEADER */
	};
	struct bLength            : Register<0,  8> { };
	struct bDescriptorType    : Register<1,  8> { };
	struct bDescriptorSubtype : Register<2,  8> { };
	struct bcdADC             : Register<3, 16> { };
	struct wTotalLength       : Register<5, 16> { };
	struct bInCollection      : Register<7,  8> { };
	struct baInterfaceNr0     : Register<8,  8> { };

	/* struct baInterfaceNrn     : Register<n,  8> { }; */

	Audio_control_header(addr_t base)
	: Mmio { base } { }
};


struct Audio_control_input_terminal : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  2, /* INPUT_TERMINAL */
	};
	struct bLength            : Register< 0,  8> { };
	struct bDescriptorType    : Register< 1,  8> { };
	struct bDescriptorSubtype : Register< 2,  8> { };
	struct bTerminalID        : Register< 3,  8> { };
	struct wTerminalType      : Register< 4, 16>
	{
		enum { MICROPHONE = 0x0201, };
		/* TODO */
	};
	struct bAssocTerminal     : Register< 6,  8> { };
	struct bNrChannels        : Register< 7,  8> { };
	struct wChannelConfig     : Register< 8, 16> { };
	struct iChannelNames      : Register<10,  8> { };
	struct iTerminal          : Register<11,  8> { };

	Audio_control_input_terminal(addr_t base)
	: Mmio { base } { }
};


struct Audio_control_output_terminal : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  3, /* OUTPUT_TERMINAL */
	};
	struct bLength            : Register<0,  8> { };
	struct bDescriptorType    : Register<1,  8> { };
	struct bDescriptorSubtype : Register<2,  8> { };
	struct bTerminalID        : Register<3,  8> { };
	struct wTerminalType      : Register<4, 16>
	{
		enum {
			SPEAKER       = 0x0301,
			USB_STREAMING = 0x0101,
		};
		/* TODO */
	};
	struct bAssocTerminal     : Register<6,  8> { };
	struct bSourceID          : Register<7,  8> { };
	struct iTerminal          : Register<8,  8> { };

	Audio_control_output_terminal(addr_t base)
	: Mmio { base } { }
};


struct Audio_control_selector_unit : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  5, /* SELECTOR_UNIT */
	};
	struct bLength            : Register<0, 8> { };
	struct bDescriptorType    : Register<1, 8> { };
	struct bDescriptorSubtype : Register<2, 8> { };
	struct bUnitID            : Register<3, 8> { };
	struct bNrPins            : Register<4, 8> { };
	struct baSourceID1        : Register<5, 8> { };
	/* struct baSourceIDn        : Register<n, 8> { }; */
	struct iSelector          : Register<6, 8> { };
	/* struct iSelector        : Register<5+n, 8> { }; */
	Audio_control_selector_unit(addr_t base)
	: Mmio { base } { }
};


struct Audio_control_feature_unit : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  6, /* FEATURE_UNIT */
	};
	/* bLength = 7 + (ch+1)*3 */
	struct bLength            : Register<0, 8> { };
	struct bDescriptorType    : Register<1, 8> { };
	struct bDescriptorSubtype : Register<2, 8> { };
	struct bUnitID            : Register<3, 8> { };
	struct bSourceID          : Register<4, 8> { };
	struct bControlSize       : Register<5, 8> { };
	enum {
		CTL_MUTE   = 1u << 0,
		CTL_VOLUME = 1u << 1,
		CTL_BASS   = 1u << 2,
		CTL_MID    = 1u << 3,
		CTL_TREBLE = 1u << 4,
		CTL_EQ     = 1u << 5,
		CTL_AUGAIN = 1u << 6,
		CTL_DELAY  = 1u << 7,
		CTL_BBOOST = 1u << 8,
		CTL_LDNSS  = 1u << 9,
	};
	/* width depends on bControlSize */
	struct bmaControls0       : Register<6, 8> { };
	struct bmaControls1       : Register<7, 8> { };
	struct bmaControls2       : Register<8, 8> { };
	/* iFeature 6 + (ch+1)*n */
	struct iFeature           : Register<9, 8> { };

	Audio_control_feature_unit(addr_t base)
	: Mmio { base } { }
};


struct Audio_control_mixer_unit : Genode::Mmio
{
	enum {
		DESCRIPTOR_TYPE     = 36, /* CS_INTERFACE */
		DESCRIPTOR_SUB_TYPE =  4, /* MIXER_UNIT */
	};
	/* bLength = 10 + p + N */
	struct bLength            : Register< 0,  8> { };
	struct bDescriptorType    : Register< 1,  8> { };
	struct bDescriptorSubtype : Register< 2,  8> { };
	struct bUnitID            : Register< 3,  8> { };
	struct bNrInPins          : Register< 4,  8> { };
	struct baSourceID0        : Register< 5,  8> { };
	struct baSourceID1        : Register< 6,  8> { };
	struct bNrChannels        : Register< 7,  8> { };
	struct wChannelConfig     : Register< 8, 16> { };
	struct iChannelNames      : Register<10,  8> { };
	struct bmControls         : Register<11,  8> { };
	struct iMixer             : Register<12,  8> { };

	Audio_control_mixer_unit(addr_t base)
	: Mmio { base } { }
};


static uint8_t const sabrent_cfg_descr[] = {
	0x9, 0x2, 0xfd, 0x0, 0x4, 0x1, 0x0, 0x80, 0x32, 0x9, 0x4, 0x0, 0x0, 0x0, 0x1, 0x1,
	0x0, 0x0, 0xa, 0x24, 0x1, 0x0, 0x1, 0x64, 0x0, 0x2, 0x1, 0x2, 0xc, 0x24, 0x2, 0x1,
	0x1, 0x1, 0x0, 0x2, 0x3, 0x0, 0x0, 0x0, 0xc, 0x24, 0x2, 0x2, 0x1, 0x2, 0x0, 0x1,
	0x1, 0x0, 0x0, 0x0, 0x9, 0x24, 0x3, 0x6, 0x1, 0x3, 0x0, 0x9, 0x0, 0x9, 0x24, 0x3,
	0x7, 0x1, 0x1, 0x0, 0x8, 0x0, 0x7, 0x24, 0x5, 0x8, 0x1, 0xa, 0x0, 0xa, 0x24, 0x6,
	0x9, 0xf, 0x1, 0x1, 0x2, 0x2, 0x0, 0x9, 0x24, 0x6, 0xa, 0x2, 0x1, 0x43, 0x0, 0x0,
	0x9, 0x24, 0x6, 0xd, 0x2, 0x1, 0x3, 0x0, 0x0, 0xd, 0x24, 0x4, 0xf, 0x2, 0x1, 0xd,
	0x2, 0x3, 0x0, 0x0, 0x0, 0x0, 0x9, 0x4, 0x1, 0x0, 0x0, 0x1, 0x2, 0x0, 0x0, 0x9,
	0x4, 0x1, 0x1, 0x1, 0x1, 0x2, 0x0, 0x0, 0x7, 0x24, 0x1, 0x1, 0x1, 0x1, 0x0, 0xe,
	0x24, 0x2, 0x1, 0x2, 0x2, 0x10, 0x2, 0x80, 0xbb, 0x0, 0x44, 0xac, 0x0, 0x9, 0x5, 0x1,
	0x9, 0xc8, 0x0, 0x1, 0x0, 0x0, 0x7, 0x25, 0x1, 0x1, 0x1, 0x1, 0x0, 0x9, 0x4, 0x2,
	0x0, 0x0, 0x1, 0x2, 0x0, 0x0, 0x9, 0x4, 0x2, 0x1, 0x1, 0x1, 0x2, 0x0, 0x0, 0x7,
	0x24, 0x1, 0x7, 0x1, 0x1, 0x0, 0xe, 0x24, 0x2, 0x1, 0x1, 0x2, 0x10, 0x2, 0x80, 0xbb,
	0x0, 0x44, 0xac, 0x0, 0x9, 0x5, 0x82, 0xd, 0x64, 0x0, 0x1, 0x0, 0x0, 0x7, 0x25, 0x1,
	0x1, 0x0, 0x0, 0x0, 0x9, 0x4, 0x3, 0x0, 0x1, 0x3, 0x0, 0x0, 0x0, 0x9, 0x21, 0x0,
	0x1, 0x0, 0x1, 0x22, 0x3c, 0x0, 0x7, 0x5, 0x87, 0x3, 0x4, 0x0, 0x2
};


void Component::construct(Genode::Env &env)
{
	Genode::error("sizeof (sabrent_cfg_descr): ", sizeof (sabrent_cfg_descr));

	Descriptor descr { (addr_t)sabrent_cfg_descr };
	Genode::log(descr);

	switch (descr.read<Descriptor::bDescriptorType>()) {
	case Descriptor::Type::DEVICE: {
		Device_descriptor dev_descr { (addr_t)sabrent_cfg_descr };
		Genode::log(dev_descr);
		break;
	}
	case Descriptor::Type::CONFIG: {
		Configuration_descriptor cfg_descr { (addr_t)sabrent_cfg_descr };
		Genode::log(cfg_descr);
		break;
	}
	case Descriptor::Type::STRING:
	case Descriptor::Type::IFACE:
	case Descriptor::Type::ENDPT:
	default:
		break;
	}


	env.parent().exit(0);
}

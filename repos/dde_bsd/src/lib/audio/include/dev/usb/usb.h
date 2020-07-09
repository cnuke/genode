/*
 * \brief  Emulation of the OpenBSD kernel USB subsystem API
 * \author Josef Soentgen
 * \date   2020-06-19
 *
 * The content of this file, in particular data structures, is partially
 * derived from OpenBSD-internal headers.
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEV_USB_USB_H_
#define _DEV_USB_USB_H_

#include <extern_c_begin.h>


/*******************
 ** dev/usb/usb.h **
 *******************/

typedef u_int8_t uByte;
typedef u_int8_t uWord[2];
typedef u_int8_t uDWord[4];

// XXX consider unaligned access architectures
#define UGETW(w) (*(u_int16_t *)(w))
#define USETW(w,v) (*(u_int16_t *)(w) = (v))

struct usb_device_request
{
	uByte bmRequestType;
	uByte bRequest;
	uWord wValue;
	uWord wIndex;
	uWord wLength;
} __packed;
typedef struct usb_device_request usb_device_request_t;

#define UT_WRITE     0x00
#define UT_READ      0x80
#define UT_CLASS     0x20
#define UT_INTERFACE 0x01
#define UT_ENDPOINT  0x02

#define UT_READ_CLASS_INTERFACE  (UT_READ  | UT_CLASS | UT_INTERFACE)
#define UT_WRITE_CLASS_INTERFACE (UT_WRITE | UT_CLASS | UT_INTERFACE)
#define UT_WRITE_CLASS_ENDPOINT  (UT_WRITE | UT_CLASS | UT_ENDPOINT)

#define  UDESC_INTERFACE    0x04
#define  UDESC_ENDPOINT     0x05
#define  UDESC_CS_INTERFACE 0x24
#define  UDESC_CS_ENDPOINT  0x25

#define UE_GET_DIR(a) ((a) & 0x80)
#define UE_DIR_IN   0x80

#define UE_XFERTYPE 0x03
#define  UE_ISOCHRONOUS 0x01
#define UE_GET_XFERTYPE(a) ((a) & UE_XFERTYPE)
#define UE_ISO_TYPE 0x0c
#define  UE_ISO_ASYNC   0x04
#define  UE_ISO_ADAPT   0x08
#define  UE_ISO_SYNC    0x0c
#define UE_GET_ISO_TYPE(a) ((a) & UE_ISO_TYPE)
#define UE_GET_SIZE(a) ((a) & 0x7ff)

#define UICLASS_AUDIO         0x01
#define  UISUBCLASS_AUDIOCONTROL    1
#define  UISUBCLASS_AUDIOSTREAM     2
#define  UISUBCLASS_MIDISTREAM      3

struct usb_config_descriptor
{
	uByte bLength;
	uByte bDescriptorType;
	uWord wTotalLength;
	uByte bNumInterface;
	uByte bConfigurationValue;
	uByte iConfiguration;
	uByte bmAttributes;
	uByte bMaxPower;
} __packed;
typedef struct usb_config_descriptor usb_config_descriptor_t;

#define UC_BUS_POWERED          0x80
#define UC_SELF_POWERED         0x40
#define UC_REMOTE_WAKEUP        0x20
#define UC_POWER_FACTOR 2

struct usb_interface_descriptor
{
	// uByte bLength;
	// uByte bDescriptorType;
	// uByte bInterfaceNumber;
	// uByte bAlternateSetting;
	// uByte bNumEndpoints;
	uByte bInterfaceClass;
	uByte bInterfaceSubClass;
	// uByte bInterfaceProtocol;
	// uByte iInterface;
} __packed;
typedef struct usb_interface_descriptor usb_interface_descriptor_t;

#define USB_SPEED_LOW   1
#define USB_SPEED_FULL  2
#define USB_SPEED_HIGH  3
#define USB_SPEED_SUPER 4


/*********************
 ** dev/usb/usbdi.h **
 *********************/

#define USBD_NO_COPY        0x01    /* do not copy data to DMA buffer */
#define USBD_SHORT_XFER_OK  0x04    /* allow short reads */

struct usbd_device;
struct usbd_interface;
struct usbd_pipe;
struct usbd_xfer;

typedef enum
{
	USBD_NORMAL_COMPLETION = 0, /* must be 0 */
	USBD_IN_PROGRESS,       /* 1 */
	// /* errors */
	// USBD_PENDING_REQUESTS,  /* 2 */
	// USBD_NOT_STARTED,       /* 3 */
	// USBD_INVAL,             /* 4 */
	// USBD_NOMEM,             /* 5 */
	// USBD_CANCELLED,         /* 6 */
	// USBD_BAD_ADDRESS,       /* 7 */
	// USBD_IN_USE,            /* 8 */
	// USBD_NO_ADDR,           /* 9 */
	// USBD_SET_ADDR_FAILED,   /* 10 */
	// USBD_NO_POWER,          /* 11 */
	// USBD_TOO_DEEP,          /* 12 */
	USBD_IOERROR,           /* 13 */
	// USBD_NOT_CONFIGURED,    /* 14 */
	// USBD_TIMEOUT,           /* 15 */
	// USBD_SHORT_XFER,        /* 16 */
	// USBD_STALLED,           /* 17 */
	// USBD_INTERRUPTED,       /* 18 */

	// USBD_ERROR_MAX          /* must be last */
} usbd_status;

typedef void (*usbd_callback)(struct usbd_xfer *, void *, usbd_status);

struct usbd_xfer *usbd_alloc_xfer(struct usbd_device *);
void              usbd_free_xfer(struct usbd_xfer *xfer);
usbd_status       usbd_close_pipe(struct usbd_pipe *pipe);
usbd_status       usbd_do_request(struct usbd_device *, usb_device_request_t *,
                                  void *);
void              usbd_claim_iface(struct usbd_device *, int);
void             *usbd_alloc_buffer(struct usbd_xfer *xfer, u_int32_t size);
usbd_status       usbd_device2interface_handle(struct usbd_device *dev,
                                               u_int8_t ifaceno,
                                               struct usbd_interface **iface);
usbd_status       usbd_set_interface(struct usbd_interface *, int);
usbd_status       usbd_open_pipe(struct usbd_interface *iface, u_int8_t address,
                                 u_int8_t flags, struct usbd_pipe **pipe);
void              usbd_setup_isoc_xfer(struct usbd_xfer *xfer,
                                       struct usbd_pipe *pipe,
                                       void *priv, u_int16_t *frlengths,
                                       u_int32_t nframes, u_int16_t flags,
                                       usbd_callback);
usbd_status       usbd_transfer(struct usbd_xfer *req);
void              usbd_get_xfer_status(struct usbd_xfer *xfer, void **priv,
                                       void **buffer, u_int32_t *count,
                                       usbd_status *status);
int               usbd_is_dying(struct usbd_device *);
usb_interface_descriptor_t *usbd_get_interface_descriptor(struct usbd_interface *iface);
usb_config_descriptor_t    *usbd_get_config_descriptor(struct usbd_device *dev);

char const *usbd_errstr(usbd_status);

struct usb_attach_arg
{
	struct usbd_device    *device;
	struct usbd_interface *iface;
};

#define UMATCH_VENDOR_PRODUCT_CONF_IFACE 8
#define UMATCH_NONE                      0

#define splusb() 0xdeadc0de /* splraise(IPL_SOFTUSB) */
#define splx(x)

/************************
 ** dev/usb/usbdivar.h **
 ************************/

struct usbd_device
{
	void *genode_usb_device;

	u_int8_t speed;         /* low/full/high speed */
};


struct usbd_interface
{
	void *genode_usb_device;
};


struct usbd_pipe
{
	struct usbd_interface *iface;
};


struct usbd_xfer
{
	struct usbd_pipe *pipe;
};


#include <extern_c_end.h>

#endif /* _DEV_USB_USB_H_ */

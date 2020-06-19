/*
 * \brief  OpenBSD USB subsystem API emulation
 * \author Josef Soentgen
 * \date   2020-06-19
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator.h>
#include <base/log.h>

/* local includes */
#include <bsd.h>
#include <bsd_emul.h>
#include <dev/usb/usb.h>


extern "C" int probe_cfdata(void *);


struct usbd_xfer *usbd_alloc_xfer(struct usbd_device *)
{
	Genode::error(__func__, ": not implemented");
	return NULL;
}


void usbd_free_xfer(struct usbd_xfer *xfer)
{
	Genode::error(__func__, ": not implemented");
}


usbd_status usbd_close_pipe(struct usbd_pipe *)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}

usbd_status usbd_do_request(struct usbd_device *, usb_device_request_t *,
                            void *)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}

void usbd_claim_iface(struct usbd_device *, int)
{
	Genode::error(__func__, ": not implemented");
}


void *usbd_alloc_buffer(struct usbd_xfer *, u_int32_t)
{
	Genode::error(__func__, ": not implemented");
	return NULL;
}


usbd_status usbd_device2interface_handle(struct usbd_device *,
                                         u_int8_t,
                                         struct usbd_interface **)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


usbd_status usbd_set_interface(struct usbd_interface *, int)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


usbd_status usbd_open_pipe(struct usbd_interface *iface, u_int8_t address,
                                 u_int8_t flags, struct usbd_pipe **pipe)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


void usbd_setup_isoc_xfer(struct usbd_xfer *, struct usbd_pipe *,
                          void *, u_int16_t *, u_int32_t, u_int16_t,
                          usbd_callback)
{
	Genode::error(__func__, ": not implemented");
}


usbd_status usbd_transfer(struct usbd_xfer *req)
{
	Genode::error(__func__, ": not implemented");
	return USBD_IOERROR;
}


void usbd_get_xfer_status(struct usbd_xfer *, void **,
                          void **, u_int32_t *, usbd_status *)
{
	Genode::error(__func__, ": not implemented");
}


int usbd_is_dying(struct usbd_device *)
{
	Genode::error(__func__, ": not implemented");
	return 1;
}


usb_interface_descriptor_t *usbd_get_interface_descriptor(struct usbd_interface *)
{
	Genode::error(__func__, ": not implemented");
	return NULL;
}


usb_config_descriptor_t *usbd_get_config_descriptor(struct usbd_device *)
{
	Genode::error(__func__, ": not implemented");
	return NULL;
}


int Bsd::probe_drivers(Genode::Env &env, Genode::Allocator &alloc)
{
	Genode::log("--- probe drivers ---");
	return probe_cfdata(nullptr);
}

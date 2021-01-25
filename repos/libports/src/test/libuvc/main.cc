/*
 * \brief  VESA frame buffer driver back end
 * \author Josef Soentgen
 * \date   2021-01-25
 *
 * The test component is based on the original 'example.c'.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <gui_session/connection.h>
#include <libc/component.h>
#include <os/pixel_rgb888.h>


/* libuvc stuff */
#include "libuvc/libuvc.h"
#include <stdio.h>
#include <unistd.h>


static bool verbose = false;


struct Viewer
{
	Viewer(const Viewer &) = delete;
	const Viewer& operator=(const Viewer&) = delete;

	using PT = Genode::Pixel_rgb888;

	Genode::Env               &_env;
	Gui::Connection            _gui;
	Gui::Session::View_handle  _view;
	Framebuffer::Mode const    _mode;
	PT                        *_pixels;

	Genode::Constructible<Genode::Attached_dataspace> _fb_ds { };

	Viewer(Genode::Env &env, Framebuffer::Mode mode)
	:
		_env    { env },
		_gui    { _env, "webcam_viewer" },
		_view   { _gui.create_view() },
		_mode   { mode },
		_pixels { nullptr }
	{
		_gui.buffer(mode, false);

		_fb_ds.construct(_env.rm(), _gui.framebuffer()->dataspace());
		_pixels = _fb_ds->local_addr<PT>();

		using Command = Gui::Session::Command;
		using namespace Gui;

		_gui.enqueue<Command::Geometry>(_view, Rect(Point(0, 0), _mode.area));
		_gui.enqueue<Command::To_front>(_view, Gui::Session::View_handle());
		_gui.enqueue<Command::Title>(_view, "webcam");
		_gui.execute();
	}

	void fill(char const *data, int width, int height)
	{
		char const * p = data;

		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				unsigned char r = *p++;
				unsigned char g = *p++;
				unsigned char b = *p++;

				_pixels[i * width + j] = PT(r, b, g);
			}
		}

		_gui.framebuffer()->refresh(0, 0, _mode.area.w(), _mode.area.h());
	}
};


void cb(uvc_frame_t *frame, void *ptr)
{
	Viewer *viewer = ptr ? reinterpret_cast<Viewer*>(ptr) : nullptr;

	uvc_frame_t *out = uvc_allocate_frame(frame->width * frame->height * 3);
	if (!out) {
		Genode::warning("unable to allocate out frame");
		return;
	}

	if (verbose) {
		Genode::log(__func__, ":", " "
		            "format: ", (unsigned)frame->frame_format, " "
		            "width: ",  frame->width,        " "
		            "height: ", frame->height,       " "
		            "length: ", frame->data_bytes);
	}

	uvc_error_t ret;
	switch (frame->frame_format) {
		case UVC_FRAME_FORMAT_H264:
			break;
		case UVC_COLOR_FORMAT_MJPEG:
			ret = uvc_mjpeg2rgb(frame, out);
			if (ret) {
				uvc_perror(ret, "uvc_mpjeg2rgb");
				uvc_free_frame(out);
				return;
			}
			break;
		case UVC_COLOR_FORMAT_YUYV:
			ret = uvc_any2rgb(frame, out);
			if (ret) {
				uvc_perror(ret, "uvc_any2rgb");
				uvc_free_frame(out);
				return;
			}
			break;
		default:
			break;
	}

	if (viewer) {
		viewer->fill((char const*)out->data, out->width, out->height);
	}

	uvc_free_frame(out);
}


void Libc::Component::construct(Libc::Env &env)
{
	Genode::Attached_rom_dataspace config_rom { env, "config" };
	config_rom.update();

	bool     use_viewer = false;
	unsigned duration   = 10u;
	if (config_rom.valid()) {
		Genode::Xml_node config = config_rom.xml();

		use_viewer = config.attribute_value("viewer",   use_viewer);
		verbose    = config.attribute_value("verbose",  verbose);
		duration   = config.attribute_value("duration", duration);
	}

	Libc::with_libc([&] () {

		uvc_context_t *ctx = nullptr;
		uvc_error_t    res = uvc_init(&ctx, NULL);

		if (res < 0) {
			uvc_perror(res, "uvc_init");
			env.parent().exit(res);
			return;
		}

		Genode::log("UVC initialized");

		uvc_device_t *dev;
		res = uvc_find_device(ctx, &dev, 0, 0, NULL);
		if (res < 0) {
			uvc_perror(res, "uvc_find_device");
		} else {
			Genode::log("Device found");

			uvc_device_handle_t *devh;
			res = uvc_open(dev, &devh);

			if (res < 0) {
				uvc_perror(res, "uvc_open");
			} else {
				Genode::log("Device opened");

				uvc_print_diag(devh, stderr);

				uvc_format_desc_t const *format_desc = uvc_get_format_descs(devh);
				uvc_frame_desc_t  const *frame_desc  = format_desc->frame_descs;
				enum uvc_frame_format frame_format;
				int width  = 640;
				int height = 480;
				int fps    = 30;

				switch (format_desc->bDescriptorSubtype) {
					case UVC_VS_FORMAT_MJPEG:
						frame_format = UVC_COLOR_FORMAT_MJPEG;
						break;
					case UVC_VS_FORMAT_FRAME_BASED:
						frame_format = UVC_FRAME_FORMAT_H264;
						break;
					default:
						frame_format = UVC_FRAME_FORMAT_YUYV;
						break;
				}

				if (frame_desc) {
					width = frame_desc->wWidth;
					height = frame_desc->wHeight;
					fps = 10000000 / frame_desc->dwDefaultFrameInterval;
				}

				Genode::log("Use first format: ", format_desc->fourccFormat,
				            " ", width, "x", height, "@", fps);

				uvc_stream_ctrl_t ctrl;
				res = uvc_get_stream_ctrl_format_size(devh, &ctrl, frame_format,
				                                      width, height, fps);

				uvc_print_stream_ctrl(&ctrl, stderr);

				if (res < 0) {
					uvc_perror(res, "get_mode");
				} else {
					Genode::Constructible<Viewer> viewer { };

					if (use_viewer) {
						viewer.construct(env, Framebuffer::Mode {
							.area = { (unsigned)width, (unsigned)height } });
					}

					res = uvc_start_streaming(devh, &ctrl, cb,
					                          viewer.constructed() ? &*viewer
					                                               : (void *) 0, 0);

					if (res < 0) {
						uvc_perror(res, "start_streaming");
					} else {
						Genode::log("Streaming for ", duration, " seconds...");

						/* e.g., turn on auto exposure */
						uvc_set_ae_mode(devh, 1);

						sleep(duration);

						uvc_stop_streaming(devh);
						Genode::log("Done streaming.");
					}
				}

				/* Release our handle on the device */
				uvc_close(devh);
				Genode::log("Device closed");
			}

			/* Release the device descriptor */
			uvc_unref_device(dev);
		}

		uvc_exit(ctx);

		Genode::log("UVC exited");
	});

	env.parent().exit(0);
}

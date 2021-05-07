/**
 * \brief  etnaviv (Vivante) EGL-DRI2 back end
 * \author Sebastian Sumpf
 * \date   2021-04-30
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
/*
 * Mesa
 */
#include <egl_dri2.h>
#include <drivers/dri/common/utils.h>
/*
 * Libc
 */
#include <string.h>
#include <dlfcn.h>

/*
 * Local
 */
#include <platform.h>


static int stride(int value)
{
	/* 32-bit RGB888 */
	return value * 4;
}


static EGLBoolean
dri2_genode_etnaviv_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
	return EGL_FALSE;
}


static struct dri2_egl_display_vtbl dri2_genode_display_vtbl = {
	.authenticate = NULL,
	.create_window_surface = dri2_genode_create_window_surface,
	.create_pixmap_surface = dri2_genode_create_pixmap_surface,
	.destroy_surface = dri2_genode_destroy_surface,
	.swap_interval = dri2_genode_swap_interval,
	.swap_buffers = dri2_genode_etnaviv_swap_buffers,
	.get_dri_drawable = dri2_surface_get_dri_drawable,
};


static void
dri2_genode_etnaviv_get_image(__DRIdrawable * read,
                             int x, int y, int w, int h,
                             char *data, void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf  = loaderPrivate;
	struct Genode_egl_window  *window   = dri2_surf->g_win;
	unsigned char * src                 = window->addr;

	int src_stride = stride(dri2_surf->base.Width);
	int copy_width = stride(w);
	int x_offset = stride(x);
	int dst_stride = copy_width;

	assert(data != (char *)src);

	src += x_offset;
	src += y * src_stride;

	/* copy width over stride boundary */
	if (copy_width > src_stride - x_offset)
		copy_width = src_stride - x_offset;

	/* limit height */
	if (h > dri2_surf->base.Height - y)
		h = dri2_surf->base.Height - y;

	/* copy to surface */
	genode_blit(src, src_stride, data, dst_stride, copy_width, h);
}


static void
dri2_genode_etnaviv_put_image(__DRIdrawable * draw, int op,
                             int x, int y, int w, int h,
                             char *data, void *loaderPrivate)
{
}


static void
dri2_genode_etnaviv_get_drawable_info(__DRIdrawable * draw,
                                 int *x, int *y, int *w, int *h,
                                 void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf = loaderPrivate;

	struct Genode_egl_window  *window   = dri2_surf->g_win;

	*x = 0;
	*y = 0;
	*w = window->width;
	*h = window->height;

	dri2_surf->base.Width  = window->width;
	dri2_surf->base.Height = window->height;

}


static __DRIbuffer *
dri2_genode_get_buffers(__DRIdrawable * driDrawable,
                        int *width, int *height,
                        unsigned int *attachments, int count,
                        int *out_count, void *loaderPrivate)
{
	_eglError(EGL_BAD_PARAMETER, "dri2_genode_get_buffers not implemented");
	*out_count = 0;
	return NULL;
}


static void
dri2_genode_flush_front_buffer(__DRIdrawable * driDrawable, void *loaderPrivate)
{
	_eglError(EGL_BAD_PARAMETER, "dri2_genode_flush_front_buffer not implemented");
}


static void
back_bo_to_dri_buffer(struct dri2_egl_surface *dri2_surf, __DRIbuffer *buffer)
{
	struct dri2_egl_display *dri2_dpy = dri2_egl_display(dri2_surf->base.Resource.Display);
	__DRIimage *image;
	int name, pitch;

	image = dri2_surf->back_image;

	/* use dmabuf-fd as render nodes may not use GEM_FLINK */
	dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_FD, &name);
	dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &pitch);

	buffer->attachment = __DRI_BUFFER_BACK_LEFT;
	buffer->name = name;
	buffer->pitch = pitch;
	buffer->cpp = 4;
	buffer->flags = 0;
}


static __DRIbuffer *
dri2_genode_get_buffers_with_format(__DRIdrawable * driDrawable,
                                    int *width, int *height,
                                    unsigned int *attachments, int count,
                                    int *out_count, void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf = loaderPrivate;
	int i, j;

	for (i = 0, j = 0; i < 2 * count; i += 2, j++) {
		switch (attachments[i]) {
		case __DRI_BUFFER_BACK_LEFT:
			back_bo_to_dri_buffer(dri2_surf, &dri2_surf->buffers[j]);
			break;
		default:
//			if (get_aux_bo(dri2_surf, attachments[i], attachments[i + 1],
//			               &dri2_surf->buffers[j]) < 0) {
//				_eglError(EGL_BAD_ALLOC, "failed to allocate aux buffer");
//				return NULL;
//			}
			printf("ERROR: not implemented\n");
			while (1);
			break;
		}
	}

	*out_count = j;
	if (j == 0)
		return NULL;

	*width = dri2_surf->base.Width;
	*height = dri2_surf->base.Height;

	return dri2_surf->buffers;
}




static const __DRIdri2LoaderExtension dri2_loader_extension = {
	.base = { __DRI_DRI2_LOADER, 3 },

	.getBuffers           = dri2_genode_get_buffers,
	.flushFrontBuffer     = dri2_genode_flush_front_buffer,
	.getBuffersWithFormat = dri2_genode_get_buffers_with_format,
};


static const __DRIextension *dri2_loader_extensions[] = {
	&dri2_loader_extension.base,
	&image_lookup_extension.base,
	&background_callable_extension.base,
	NULL,
};


extern  const __DRIextension **__driDriverGetExtensions_etnaviv(void);


static EGLBoolean dri2_initialize_genode_etnaviv(_EGLDisplay *disp)
{
	// XXX pull in frontends/dri2.c b/c: MESA-LOADER: driver does not expose __driDriverGetExtensions_etnaviv():

	struct dri2_egl_display *dri2_dpy;
	static int      rgb888_shifts[4] = { 16, 8, 0, 24 };
	static unsigned rgb888_sizes[4]  = {  8, 8, 8, 8 };
	int i;

	printf("%s:%d\n", __func__, __LINE__);

	/* initialize DRM back end */
	genode_drm_init();

	dri2_dpy = calloc(1, sizeof *dri2_dpy);
	if (!dri2_dpy)
		return _eglError(EGL_BAD_ALLOC, "eglInitialize");

	printf("%s:%d\n", __func__, __LINE__);
	dri2_dpy->fd          = 42;
	dri2_dpy->driver_name = strdup("etnaviv");

	disp->DriverData = (void *)dri2_dpy;
	dri2_dpy->driver_extensions = __driDriverGetExtensions_etnaviv();

	printf("%s:%d\n", __func__, __LINE__);
	if (!dri2_load_driver_dri3(disp))
		goto close_driver;
	// printf("%s:%d\n", __func__, __LINE__);
	// if (!dri2_load_driver(disp))
	// 	goto close_driver;

	dri2_dpy->dri2_major = 2;
	dri2_dpy->dri2_minor = __DRI_DRI2_VERSION;

	dri2_dpy->loader_extensions = dri2_loader_extensions;

	printf("%s:%d\n", __func__, __LINE__);
	if (!dri2_create_screen(disp))
		goto close_screen;

	printf("%s:%d\n", __func__, __LINE__);
	if (!dri2_setup_extensions(disp))
		goto close_screen;

	printf("%s:%d\n", __func__, __LINE__);
	dri2_setup_screen(disp);

	EGLint attrs[] = {
		EGL_DEPTH_SIZE, 0, /* set in loop below (from DRI config) */
		EGL_NATIVE_VISUAL_TYPE, 0,
		EGL_NATIVE_VISUAL_ID, 0,
		EGL_NONE };

	for (i = 0; dri2_dpy->driver_configs[i]; i++) {
		/* set depth size in attrs */
		attrs[1] = dri2_dpy->driver_configs[i]->modes.depthBits;
		dri2_add_config(disp, dri2_dpy->driver_configs[i], i,
		                EGL_WINDOW_BIT | EGL_PBUFFER_BIT, attrs,
		                rgb888_shifts, rgb888_sizes);
	}

	dri2_dpy->vtbl   = &dri2_genode_display_vtbl;

	printf("%s:%d dri2_dpy: %p dri2_dpy->dri2: %p\n", __func__, __LINE__, dri2_dpy, dri2_dpy->dri2);
	return EGL_TRUE;

close_screen:
	printf("%s:%d\n", __func__, __LINE__);
	dlclose(dri2_dpy->driver);
close_driver:
	printf("%s:%d\n", __func__, __LINE__);
	free(dri2_dpy);

	return EGL_FALSE;
}


EGLBoolean dri2_initialize_genode_backend(_EGLDisplay *disp)
{
	return dri2_initialize_genode_etnaviv(disp);
}

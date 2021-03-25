/**
 * \brief  Backend implementation for Linux
 * \author Josef Soentgen
 * \date   2021-08-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_DRM_H_
#define _LX_DRM_H_

#ifdef __cplusplus
extern "C" {
#endif

struct Handle_id
{
	unsigned long long id;
	unsigned int       handle;
};

struct Handle_id_array
{
	unsigned int count;
	struct Handle_id *items;
};


void *lx_drm_open(void);
void  lx_drm_close(void *);

int lx_drm_ioctl_etnaviv_gem_param(void *, unsigned char, unsigned long long*);
int lx_drm_ioctl_etnaviv_gem_submit(void *, unsigned long, unsigned int*, struct Handle_id_array const*);
int lx_drm_ioctl_etnaviv_gem_new(void *, unsigned long, unsigned int *);
int lx_drm_ioctl_etnaviv_gem_info(void *, unsigned int, unsigned long long *);
int lx_drm_ioctl_etnaviv_cpu_prep(void *, unsigned int, int);
int lx_drm_ioctl_etnaviv_cpu_fini(void *, unsigned int);
int lx_drm_ioctl_gem_close(void *, unsigned int);
int lx_drm_ioctl_etnaviv_wait_fence(void *, unsigned int);


void genode_completion_signal(unsigned long long);

#ifdef __cplusplus
}
#endif

#endif /* _LX_DRM_H_ */

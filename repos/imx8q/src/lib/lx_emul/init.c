/*
 * \brief  Linux Kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

#include <lx_emul/init.h>
#include <lx_emul/printf.h>

#include <drm/etnaviv_drm.h>

void probe_platform_bus(void);
void register_gpu_platform_device(void);
int lx_drm_open(void);
int lx_drm_ioctl(unsigned int, unsigned long);
void lx_emul_announce_drm_session(void);


/* functions marked with __init */
void radix_tree_init(void);

int lx_emul_init_kernel()
{
	radix_tree_init();

	return 0;
}


int lx_emul_start_kernel()
{
	int err = 0;

	register_gpu_platform_device();
	probe_platform_bus();

	err = lx_drm_open();
	if (err) {
		return err;
	}

	{
		struct drm_etnaviv_param param = {
			.pipe  = 0,
			.param = ETNAVIV_PARAM_GPU_MODEL,
			.value = 0,
		};

		lx_emul_printf("%s: query gpu model param: %px\n", __func__, &param);
		err = lx_drm_ioctl(DRM_IOCTL_ETNAVIV_GET_PARAM,
		                   (unsigned long)&param);
		if (err) {
			return err;
		}

		lx_emul_printf("%s: model: 0x%llx\n", __func__, param.value);
	}

	lx_emul_announce_drm_session();
	return 0;
}

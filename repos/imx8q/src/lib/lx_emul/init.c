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
void lx_emul_announce_gpu_session(void);


/* functions marked with __init */
void radix_tree_init(void);

int lx_emul_init_kernel()
{
	radix_tree_init();

	return 0;
}


int lx_emul_start_kernel()
{
	register_gpu_platform_device();
	probe_platform_bus();

	lx_emul_announce_gpu_session();
	return 0;
}

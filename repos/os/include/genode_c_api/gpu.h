/*
 * \brief  C-API Genode gpu backend
 * \author Josef Soentgen
 * \date   2021-10-29
 */

/*
 * Copyright (C) 2006-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENODE_C_API__GPU_H_
#define _GENODE_C_API__GPU_H_

#include <genode_c_api/base.h>


struct genode_gpu_session; /* definition is private to the implementation */


#ifdef __cplusplus
extern "C" {
#endif


/********************
 ** Initialization **
 ********************/

/**
 * Callback that returns the Info dataspace
 */
typedef struct genode_dataspace * (*genode_gpu_rpc_info_dataspace_t)
	(unsigned long size);

/**
 * Callback for enqueing a buffer for execution
 */
typedef unsigned long (*genode_gpu_rpc_exec_buffer_t)
	(unsigned long id, unsigned long size);

/**
 * Callback for checking if the given request has beend completed
 */
typedef int (*genode_gpu_rpc_complete_t)
	(unsigned int seqno);

/**
 * Callback for allocation a new buffer
 */
typedef struct genode_dataspace * (*genode_gpu_rpc_alloc_buffer_t)
	(unsigned long id, unsigned long size);

/**
 * Callback for freeing a buffer
 */
typedef void (*genode_gpu_rpc_free_buffer_t)
	(unsigned long id);

/**
 * Callback for mapping a buffer for access by the CPU
 */
typedef struct genode_dataspace * (*genode_gpu_rpc_map_buffer_t)
	(unsigned long id, int, int attrs);

/**
 * Callback for unmapping a buffer accessed by the CPU
 */
typedef void (*genode_gpu_rpc_unmap_buffer_t)
	(unsigned long id);

struct genode_gpu_rpc_callbacks
{
	genode_gpu_rpc_info_dataspace_t info_dataspace_fn;
	genode_gpu_rpc_exec_buffer_t    exec_buffer_fn;
	genode_gpu_rpc_complete_t       complete_fn;
	genode_gpu_rpc_alloc_buffer_t   alloc_buffer_fn;
	genode_gpu_rpc_free_buffer_t    free_buffer_fn;
	genode_gpu_rpc_map_buffer_t     map_buffer_fn;
	genode_gpu_rpc_unmap_buffer_t   unmap_buffer_fn;
};


/**
 * Initialize gpu root component
 *
 * \param handler  signal handler to be installed at each gpu session
 */
void genode_gpu_init(struct genode_env               *env,
                     struct genode_allocator         *alloc,
                     struct genode_signal_handler    *handler,
                     struct genode_gpu_rpc_callbacks *callbacks);


/**************************************
 ** Gpu session lifetime management **
 **************************************/

void genode_gpu_announce_service();


/************************************
 ** Gpu session request handling **
 ************************************/

enum Operation
{
	GENODE_GPU_ALLOC,
	GENODE_GPU_FREE,
	GENODE_GPU_MAP,
	GENODE_GPU_UNMAP,
	GENODE_GPU_EXEC,
	GENODE_GPU_WAIT,
};

enum Attributes
{
	GENODE_GPU_ATTR_READ  = 1,
	GENODE_GPU_ATTR_WRITE = 2,
};


struct genode_gpu_request
{
	int op;

	unsigned long size;
	unsigned int  handle;
	unsigned int  fence_id;
	int           attrs;

	void *exec_buffer;
	void *device;

	int success;
};

struct genode_gpu_session * genode_gpu_session_by_name(const char * name);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__GPU_H_ */

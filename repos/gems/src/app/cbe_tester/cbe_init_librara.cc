/*
 * \brief  Temporary module compliant wrapper for the CBE library
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <cbe_init_librara.h>
#include <trust_anchor.h>
#include <block_io.h>
#include <block_allocator.h>
#include <vbd_initializer.h>

using namespace Genode;
using namespace Cbe;


void Cbe_init::Librara::_drop_generated_request(Module_request &mod_req)
{
	switch (mod_req.dst_module_id()) {
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &req {
			*dynamic_cast<Trust_anchor_request *>(&mod_req) };

		_lib.librara__drop_generated_request(req.prim_ptr());
		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &req {
			*dynamic_cast<Block_io_request *>(&mod_req) };

		_lib.librara__drop_generated_request(req.prim_ptr());
		break;
	}
	case BLOCK_ALLOCATOR:
	{
		Block_allocator_request &req {
			*dynamic_cast<Block_allocator_request *>(&mod_req) };

		_lib.librara__drop_generated_request(req.prim_ptr());
		break;
	}
	case VBD_INITIALIZER:
	{
		Vbd_initializer_request &req {
			*dynamic_cast<Vbd_initializer_request *>(&mod_req) };

		_lib.librara__drop_generated_request(req.prim_ptr());
		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Cbe_init::Librara::generated_request_complete(Module_request &mod_req)
{
	switch (mod_req.dst_module_id()) {
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &req {
			*dynamic_cast<Trust_anchor_request *>(&mod_req) };

		_lib.librara__generated_request_complete(
			req.prim_ptr(), req.key_plaintext_ptr(), req.key_ciphertext_ptr(),
			0, nullptr, req.success());

		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &req {
			*dynamic_cast<Block_io_request *>(&mod_req) };

		_lib.librara__generated_request_complete(
			req.prim_ptr(), nullptr, nullptr, 0, nullptr, req.success());

		break;
	}
	case BLOCK_ALLOCATOR:
	{
		Block_allocator_request &req {
			*dynamic_cast<Block_allocator_request *>(&mod_req) };

		_lib.librara__generated_request_complete(
			req.prim_ptr(), nullptr, nullptr, req.blk_nr(), nullptr, req.success());

		break;
	}
	case VBD_INITIALIZER:
	{
		Vbd_initializer_request &req {
			*dynamic_cast<Vbd_initializer_request *>(&mod_req) };

		_lib.librara__generated_request_complete(
			req.prim_ptr(), nullptr, nullptr, 0, req.root_node(), req.success());

		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}

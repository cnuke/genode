/*
 * \brief  Module for cached access to physical blocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* cbe tester includes */
#include <cache.h>

using namespace Genode;
using namespace Cbe;


/**********************
 ** Cache_request **
 **********************/

void Cache_request::create(void       *buf_ptr,
                           size_t      buf_size,
                           uint64_t    src_module_id,
                           uint64_t    src_request_id,
                           size_t      req_type,
                           void       *prim_ptr,
                           size_t      prim_size,
                           uint64_t    pba,
                           void       *blk_ptr)
{
	Cache_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;
	req._pba = pba;
	if (prim_ptr != nullptr) {
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Exception_1 { };
			throw Exception_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
	}
	req._blk_ptr = (addr_t)blk_ptr;

	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Cache_request::Cache_request(unsigned long src_module_id,
                             unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, CACHE }
{ }


char const *Cache_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case READ: return "read";
	case WRITE: return "write";
	case SYNC: return "sync";
	}
	return "?";
}

/*
      --
      --  Cache -> Block_IO
      --
      declare
         Prim : Primitive.Object_Type;
         Slot_Idx : Cache.Slots_Index_Type;
      begin
         Cache.Peek_Generated_Primitive (Obj.Cache_Obj, Prim, Slot_Idx);
         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_Cache_Blk_IO =>

               case Primitive.Operation (Prim) is
               when Read =>

                  Create_Block_IO_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 1,
                     CBE_Req_Offset => 0,
                     CBE_Req_Tag    => 0,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         => 0,
                     PBA            =>
                        CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                     VBA            => 0,
                     Blk_Count      => 1,
                     Blk_Ptr        => Obj.Cache_Slots_Data (Slot_Idx)'Address
                  );
                  return 1;

               when Write =>

                  Create_Block_IO_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 2,
                     CBE_Req_Offset => 0,
                     CBE_Req_Tag    => 0,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         => 0,
                     PBA            =>
                        CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                     VBA            => 0,
                     Blk_Count      => 1,
                     Blk_Ptr        => Obj.Cache_Slots_Data (Slot_Idx)'Address
                  );
                  return 1;

               when Sync =>

                  Create_Block_IO_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 3,
                     CBE_Req_Offset => 0,
                     CBE_Req_Tag    => 0,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         => 0,
                     PBA            =>
                        CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                     VBA            => 0,
                     Blk_Count      => 1,
                     Blk_Ptr        => System.Null_Address
                  );
                  return 1;

               end case;

            when others =>

               null;

            end case;
         end if;
      end;
*/

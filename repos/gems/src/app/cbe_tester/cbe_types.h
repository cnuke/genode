/*
 * \brief  Basic types regarding the CBE
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOCAL_CBE_TYPES_H_
#define _LOCAL_CBE_TYPES_H_

/* base includes */
#include <base/stdint.h>

/* gems includes */
#include <cbe/types.h>

namespace Cbe {

	enum {
		TYPE_1_NODE_STORAGE_SIZE = 64,
		TYPE_2_NODE_STORAGE_SIZE = 64,
		NR_OF_TYPE_2_NODES_PER_BLK =
			BLOCK_SIZE / TYPE_2_NODE_STORAGE_SIZE,

		NR_OF_TYPE_1_NODES_PER_BLK =
			BLOCK_SIZE / TYPE_1_NODE_STORAGE_SIZE,

		TREE_LEVEL_MAX = 6,
	};


	struct Type_1_node
	{
		Genode::uint64_t pba      { 0 };
		Genode::uint64_t gen      { 0 };
		Genode::uint8_t  hash[32] { 0 };

		Genode::uint8_t  reserved[16] { 0 };

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			Genode::print(out, "pba: ", pba, " gen: ", gen, " hash: ");
			for (unsigned i = 0; i < sizeof(hash); i++)
				Genode::print(out, Hex(hash[i], Hex::OMIT_PREFIX, Hex::PAD));
		}
	}
	__attribute__((packed));
	static_assert(sizeof(Type_1_node) == TYPE_1_NODE_STORAGE_SIZE);


	struct Type_1_node_block
	{
		Type_1_node value[NR_OF_TYPE_1_NODES_PER_BLK] { };
	} __attribute__((packed));
	static_assert(sizeof(Type_1_node_block) ==
		(TYPE_1_NODE_STORAGE_SIZE * NR_OF_TYPE_1_NODES_PER_BLK));

	struct Type_2_node
	{
		Genode::uint64_t pba          { 0 };
		Genode::uint64_t last_vba     { 0 };
		Genode::uint64_t alloc_gen    { 0 };
		Genode::uint64_t free_gen     { 0 };
		Genode::uint32_t last_key_id  { 0 };
		Genode::uint8_t  reserved[28] { 0 };

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			Genode::print(out, "pba: ",         pba, " "
			                   "last_vba: ",    last_vba, " "
			                   "alloc_gen: ",   alloc_gen, " "
			                   "free_gen: ",    free_gen, " "
			                   "last_key_id: ", last_key_id);
		}

	} __attribute__((packed));
	static_assert(sizeof(Type_2_node) == TYPE_2_NODE_STORAGE_SIZE);

	struct Type_2_node_block
	{
		Type_2_node value[NR_OF_TYPE_2_NODES_PER_BLK] { };
	} __attribute__((packed));
	static_assert(sizeof(Type_2_node_block) ==
		(TYPE_2_NODE_STORAGE_SIZE * NR_OF_TYPE_2_NODES_PER_BLK));
}

#endif /* _LOCAL_CBE_TYPES_H_ */

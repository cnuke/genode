/*
 * \brief  Module for file tree resizing
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FT_RESIZING_H_
#define _FT_RESIZING_H_

/* cbe tester includes */
#include <module.h>
#include <cbe/types.h>
#include <vfs_utilities.h>

namespace Cbe
{
	class Ft_resizing;
	class Ft_resizing_request;
	class Ft_resizing_channel;
}

class Cbe::Ft_resizing_request : public Module_request
{
	public:

		enum Type { INVALID = 0, FT_EXTENSION_STEP = 1 };

	private:

		friend class Ft_resizing;
		friend class Ft_resizing_channel;

		Type _type { INVALID };
};

class Cbe::Ft_resizing_channel
{
	private:

		friend class Ft_resizing;

		enum State {
			SUBMITTED,

			READ_ROOT_NODE_PENDING,
			READ_ROOT_NODE_IN_PROGRESS,
			READ_ROOT_NODE_COMPLETED,

			READ_INNER_NODE_PENDING,
			READ_INNER_NODE_IN_PROGRESS,
			READ_INNER_NODE_COMPLETED,

			ALLOC_PBA_PENDING,
			ALLOC_PBA_IN_PROGRESS,
			ALLOC_PBA_COMPLETED,

			EXTEND_MT_BY_ONE_LEAF_PENDING,
			EXTEND_MT_BY_ONE_LEAF_IN_PROGRESS,
			EXTEND_MT_BY_ONE_LEAF_COMPLETED,

			WRITE_INNER_NODE_PENDING,
			WRITE_INNER_NODE_IN_PROGRESS,
			WRITE_INNER_NODE_COMPLETED,

			WRITE_ROOT_NODE_PENDING,
			WRITE_ROOT_NODE_IN_PROGRESS,
			WRITE_ROOT_NODE_COMPLETED,

			COMPLETED
		};

		enum Tag_type
		{
			TAG_INVALID,
			TAG_FT_RSZG_CACHE,
			TAG_FT_RSZG_MT_ALLOC,
		};

		struct Generated_prim
		{
			enum Type { READ, WRITE };

			Type     op     { READ };
			bool     succ   { false };
			Tag_type tg     { TAG_INVALID };
			uint64_t blk_nr { 0 };
			uint64_t idx    { 0 };
		};

		struct Submitted_prim
		{
			bool succ { false };
		};

		/* same as in virtual_block_device.h ?! XXX */
		struct Type_1_node_blocks
		{
			Type_1_node_block items[TREE_MAX_LEVEL] { };
		};

/* Martin: please check: XXX
   Tree_Max_Max_Level : constant := 6;
   type Tree_Level_Index_Type is range 0 .. Tree_Max_Max_Level;
   type Tree_Level_Generations_Type
   is array (Tree_Level_Index_Type) of Generation_Type;
   Old_Generations  : Tree_Level_Generations_Type;
*/
		enum { TREE_MAX_MAX_LEVEL = 6 };

		struct Generations
		{
			Generation values[TREE_MAX_MAX_LEVEL + 1] { }; /* +1 right ? see above XXX */
		};

		Ft_resizing_request    _request         { };
		State                  _state           { SUBMITTED };

		Submitted_prim         _submitted_prim  { };
		Generated_prim         _generated_prim  { };

		Type_1_node            _ft_root         { };
		Tree_level_index       _ft_max_lvl_idx  { 0 }; /* XXX min value ? -> 0 */
		Tree_number_of_leaves  _ft_nr_of_leaves { 0 };
		Tree_degree            _ft_degree       { TREE_MIN_DEGREE }; /* XXX */
		Type_1_node_blocks     _t1_blks         { };
		Type_2_node_block      _t2_blk          { };
		Tree_level_index       _lvl_idx         { 0 }; /* XXX ? */
		Tree_level_index       _alloc_lvl_idx   { 0 }; /* XXX ? */
		Virtual_block_address  _vba             { };
		Tree_walk_pbas         _old_pbas        { };
		Generations            _old_generations { };
		Tree_walk_pbas         _new_pbas        { };
		Physical_block_address _pba             { };
		Number_of_blocks       _nr_of_pbas      { };
		Tree_number_of_leaves  _nr_of_leaves    { };
		Generation             _curr_gen        { };
};

class Cbe::Ft_resizing : public Module
{
	private:

		using Tree_child_index = Genode::uint32_t;

		using Request = Ft_resizing_request;
		using Channel = Ft_resizing_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _set_args_for_write_back_of_inner_lvl(Tree_level_index const,
		                                           Tree_level_index const,
		                                           Physical_block_address const,
		                                           unsigned const prim_idx,
		                                           Channel::State &,
		                                           bool &progress,
		                                           Channel::Generated_prim &);

		void _add_new_root_lvl_to_ft_using_pba_contingent(Type_1_node &,
		                                                  Tree_level_index &,
		                                                  Tree_number_of_leaves const,
		                                                  Generation const,
		                                                  Channel::Type_1_node_blocks &,
		                                                  Tree_walk_pbas &,
		                                                  Physical_block_address &,
		                                                  Number_of_blocks &);

		void _add_new_branch_to_ft_using_pba_contingent(Tree_level_index const,
		                                                Tree_child_index const,
		                                                Tree_degree const,
		                                                Generation const,
		                                                Physical_block_address &,
		                                                Number_of_blocks &,
		                                                Channel::Type_1_node_blocks &,
		                                                Type_2_node_block &,
		                                                Tree_walk_pbas &,
		                                                Tree_level_index &,
		                                                Tree_number_of_leaves &);

		void _execute_ft_extension_step(Channel &, unsigned const idx, bool &);

		void _execute_ft_ext_step_read_inner_node_completed(Channel &,
		                                                    unsigned const job_idx,
		                                                    bool &progress);

		/************
		 ** Module **
		 ************/

		void execute(bool &) override;

};

#endif /* _FT_RESIZING_H_ */

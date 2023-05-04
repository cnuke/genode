/*
 * \brief  Module for file tree resizing
 * \author Martin Stein
 * \date   2023-05-09
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
#include <ft_resizing.h>
#include <sha256_4k_hash.h>

using namespace Genode;
using namespace Cbe;


static Virtual_block_address tree_max_max_vba(Tree_degree      const degree,
                                              Tree_level_index const max_lvl_idx)
{
	return (Virtual_block_address(degree) ^ max_lvl_idx) - 1;
}


static Physical_block_address alloc_pba_from_resizing_contingent(Physical_block_address &first_pba,
                                                                 Number_of_blocks       &nr_of_pbas)
{
	if (nr_of_pbas == 0) {
		class Program_error_ft_resizing_nr_of_pbas_zero { };
		throw Program_error_ft_resizing_nr_of_pbas_zero { };
	}

	auto const allocated_pba = first_pba;

	first_pba  = first_pba  + 1;
	nr_of_pbas = nr_of_pbas - 1;

	return allocated_pba;
}


static uint64_t log_2(uint64_t const value)
{
	class Log_2_error { };

	if (value == 0)
		throw Log_2_error { };

	uint64_t result = log2(value);
	if (result >= sizeof(value) * 8)
		throw Log_2_error { };

	return result;
}


static Node_index t1_child_idx_for_vba(Virtual_block_address const vba,
                                       Tree_level_index      const lvl,
                                       Tree_degree           const degr)
{
	uint64_t const degree_log_2 { log_2(degr) };
	uint64_t const degree_mask  { (1ull << degree_log_2) - 1 };
	uint64_t const vba_rshift   { degree_log_2 * (lvl - 1) };
	return degree_mask & (vba >> vba_rshift);
}


static Node_index t2_child_idx_for_vba(Virtual_block_address const vba,
                                       Tree_degree           const degr)
{
	uint64_t const degree_log_2 { log_2(degr) };
	uint64_t const degree_mask  { (1ull << degree_log_2) - 1 };

	return degree_mask & vba;
}


void Ft_resizing::_execute_ft_ext_step_read_inner_node_completed(Channel        &channel,
                                                                 unsigned const  job_idx,
                                                                 bool           &progress)
{
	if (not channel._generated_prim.succ) {
		class Primitive_not_successfull_ft_resizing { };
		throw Primitive_not_successfull_ft_resizing { };
	}

	if (channel._lvl_idx > 1) {

		if (channel._lvl_idx == channel._ft_max_lvl_idx) {

			if (not check_sha256_4k_hash(&channel._t1_blks.items[channel._lvl_idx],
                                         &channel._ft_root.hash)) {
				class Program_error_ft_resizing_hash_mismatch { };
				throw Program_error_ft_resizing_hash_mismatch { };
			}

		} else {

			auto const parent_lvl_idx = channel._lvl_idx + 1;
			auto const child_idx = t1_child_idx_for_vba(channel._vba,
			                                            parent_lvl_idx,
			                                            channel._ft_degree);
			auto const &child = channel._t1_blks.items[parent_lvl_idx].nodes[child_idx];

			if (not check_sha256_4k_hash(&channel._t1_blks.items[channel._lvl_idx],
			                             &child.hash)) {
				class Program_error_ft_resizing_hash_mismatch_2 { };
				throw Program_error_ft_resizing_hash_mismatch_2 { };
			}

		}

		auto const parent_lvl_idx = channel._lvl_idx;
		auto const child_lvl_idx = channel._lvl_idx - 1;
		auto const child_idx = t1_child_idx_for_vba(channel._vba,
		                                            parent_lvl_idx,
		                                            channel._ft_degree);
		auto const &child = channel._t1_blks.items[parent_lvl_idx].nodes[child_idx];

		if (child.valid()) {

			channel._lvl_idx                               = child_lvl_idx;
			channel._old_pbas.pbas         [child_lvl_idx] = child.pba;
			channel._old_generations.values[child_lvl_idx] = child.gen;

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_FT_RSZG_CACHE,
				.blk_nr = child.pba,
				.idx    = job_idx
			};

			channel._state = Channel::State::READ_INNER_NODE_PENDING;
			progress       = true;

/*
               pragma Debug (Debug.Print_String (
                  "   READ LVL " &
                  Debug.To_String (Debug.Uint64_Type (Child_Lvl_Idx)) &
                  " PARENT LVL " &
                  Debug.To_String (Debug.Uint64_Type (Parent_Lvl_Idx)) &
                  " CHILD " &
                  Debug.To_String (Debug.Uint64_Type (Child_Idx)) &
                  " PBA " &
                  Debug.To_String (Debug.Uint64_Type (Child.PBA)) &
                  " GEN " &
                  Debug.To_String (Debug.Uint64_Type (Child.Gen)) &
                  " " &
                  Debug.To_String (Child.Hash) &
                  " "));
*/
		} else {

			_add_new_branch_to_ft_using_pba_contingent(parent_lvl_idx,
			                                           child_idx,
			                                           channel._ft_degree,
			                                           channel._curr_gen,
			                                           channel._pba,
			                                           channel._nr_of_pbas,
			                                           channel._t1_blks,
			                                           channel._t2_blk,
			                                           channel._new_pbas,
			                                           channel._lvl_idx,
			                                           channel._nr_of_leaves);

			channel._alloc_lvl_idx = parent_lvl_idx;

			if (channel._old_generations.values[channel._alloc_lvl_idx] == channel._curr_gen) {

				channel._new_pbas.pbas[channel._alloc_lvl_idx] =
				   channel._old_pbas.pbas[channel._alloc_lvl_idx];

				channel._state = Channel::State::ALLOC_PBA_COMPLETED;
				progress       = true;

			} else {

				channel._generated_prim = {
					.op     = Channel::Generated_prim::Type::READ,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_FT_RSZG_MT_ALLOC,
					.blk_nr = 0,
					.idx    = job_idx
				};

				channel._state = Channel::State::ALLOC_PBA_PENDING;
				progress       = true;

			}
		}
	} else {

		{
			auto const parent_lvl_idx = channel._lvl_idx + 1;
			auto const child_idx = t1_child_idx_for_vba(channel._vba,
			                                            parent_lvl_idx,
			                                            channel._ft_degree);

			if (not check_sha256_4k_hash(&channel._t2_blk,
			                             &channel._t1_blks.items[parent_lvl_idx].nodes[child_idx].hash)) {
				class Program_error_ft_resizing_hash_mismatch_3 { };
				throw Program_error_ft_resizing_hash_mismatch_3 { };
			}
		}

		{
			auto const parent_lvl_idx = channel._lvl_idx;
			auto const child_idx = t2_child_idx_for_vba(channel._vba,
			                                            channel._ft_degree);

			auto const &child = channel._t2_blk.nodes[child_idx];

			if (not child.valid()) {
				class Program_error_ft_resizing_t2_invalid { };
				throw Program_error_ft_resizing_t2_invalid { };
			}

			_add_new_branch_to_ft_using_pba_contingent(parent_lvl_idx,
			                                           child_idx,
			                                           channel._ft_degree,
			                                           channel._curr_gen,
			                                           channel._pba,
			                                           channel._nr_of_pbas,
			                                           channel._t1_blks,
			                                           channel._t2_blk,
			                                           channel._new_pbas,
			                                           channel._lvl_idx,
			                                           channel._nr_of_leaves);

			channel._alloc_lvl_idx = parent_lvl_idx;
/*
            pragma Debug (Debug.Print_String (
               "   ALLOC LVL " &
               Debug.To_String (Debug.Uint64_Type (Job.Alloc_Lvl_Idx))));
*/
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_FT_RSZG_MT_ALLOC,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::ALLOC_PBA_PENDING;
			progress       = true;
		}
	}
}


void Ft_resizing::_set_args_for_write_back_of_inner_lvl(Tree_level_index       const  max_lvl_idx,
                                                        Tree_level_index       const  lvl_idx,
                                                        Physical_block_address const  pba,
                                                        unsigned               const  prim_idx,
                                                        Channel::State               &job_state,
                                                        bool                         &progress,
                                                        Channel::Generated_prim      &prim)
{
	if (lvl_idx == 0) {
		class Program_error_ft_resizing_lvl_idx_zero { };
		throw Program_error_ft_resizing_lvl_idx_zero { };
	}

	if (lvl_idx > max_lvl_idx) {
		class Program_error_ft_resizing_lvl_idx_large { };
		throw Program_error_ft_resizing_lvl_idx_large { };
	}

	prim = {
		.op     = Channel::Generated_prim::Type::WRITE,
		.succ   = false,
		.tg     = Channel::Tag_type::TAG_FT_RSZG_CACHE,
		.blk_nr = pba,
		.idx    = prim_idx
	};

/*
      pragma Debug (Debug.Print_String (
         "   WRITE LVL " &
         Debug.To_String (Debug.Uint64_Type (Lvl_Idx)) &
         " PBA " &
         Debug.To_String (Debug.Uint64_Type (PBA)) &
         " "));
*/

	if (lvl_idx < max_lvl_idx) {
		job_state = Channel::State::WRITE_INNER_NODE_PENDING;
		progress  = true;
	} else {
		job_state = Channel::State::WRITE_ROOT_NODE_PENDING;
		progress  = true;
	}
}


void Ft_resizing::_add_new_root_lvl_to_ft_using_pba_contingent(Type_1_node                 &ft_root,
                                                               Tree_level_index            &ft_max_lvl_idx,
                                                               Tree_number_of_leaves const  ft_nr_of_leaves,
                                                               Generation            const  curr_gen,
                                                               Channel::Type_1_node_blocks &t1_blks,
                                                               Tree_walk_pbas              &new_pbas,
                                                               Physical_block_address      &first_pba,
                                                               Number_of_blocks            &nr_of_pbas)
{
	if (ft_max_lvl_idx >= TREE_MAX_LEVEL) {
		class Program_error_ft_resizing_max_level { };
		throw Program_error_ft_resizing_max_level { };
	}

	ft_max_lvl_idx += 1;

	t1_blks.items[ft_max_lvl_idx] = { };
	t1_blks.items[ft_max_lvl_idx].nodes[0] = ft_root;

	new_pbas.pbas[ft_max_lvl_idx] = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);

	ft_root = {
		.pba     = new_pbas.pbas[ft_max_lvl_idx],
		.gen     = curr_gen,
		.hash    = { },
	};
/*
      pragma Debug (Debug.Print_String (
         "   SET FT_ROOT PBA " &
         Debug.To_String (Debug.Uint64_Type (FT_Root.PBA)) &
         " GEN " &
         Debug.To_String (Debug.Uint64_Type (FT_Root.Gen)) &
         " LEAVES " &
         Debug.To_String (Debug.Uint64_Type (FT_Nr_Of_Leaves)) &
         " MAX_LVL " &
         Debug.To_String (Debug.Uint64_Type (FT_Max_Lvl_Idx)) &
         " " &
         Debug.To_String (FT_Root.Hash) &
         " "));

      pragma Debug (Debug.Print_String (
         "   SET LVL " &
         Debug.To_String (Debug.Uint64_Type (FT_Max_Lvl_Idx)) &
         " CHILD 0 PBA " &
         Debug.To_String (Debug.Uint64_Type (
            T1_Blks (FT_Max_Lvl_Idx) (0).PBA)) &
         " GEN " &
         Debug.To_String (Debug.Uint64_Type (
            T1_Blks (FT_Max_Lvl_Idx) (0).Gen)) &
         " " &
         Debug.To_String (
            T1_Blks (FT_Max_Lvl_Idx) (0).Hash) &
         " "));
      pragma Unreferenced (FT_Nr_Of_Leaves);
*/
	(void)ft_nr_of_leaves;
}


void Ft_resizing::_add_new_branch_to_ft_using_pba_contingent(Tree_level_index      const  mount_point_lvl_idx,
                                                             Tree_child_index      const  mount_point_child_idx,
                                                             Tree_degree           const  ft_degree,
                                                             Generation            const  curr_gen,
                                                             Physical_block_address      &first_pba,
                                                             Number_of_blocks            &nr_of_pbas,
                                                             Channel::Type_1_node_blocks &t1_blks,
                                                             Type_2_node_block           &t2_blk,
                                                             Tree_walk_pbas              &new_pbas,
                                                             Tree_level_index            &stopped_at_lvl_idx,
                                                             Tree_number_of_leaves       &nr_of_leaves)
{
	nr_of_leaves       = 0;
	stopped_at_lvl_idx = mount_point_lvl_idx;

	if (mount_point_lvl_idx > 1) {
		for (unsigned lvl_idx = 1; lvl_idx < mount_point_lvl_idx - 1; lvl_idx++) {
			if (lvl_idx > 1)
				t1_blks.items[lvl_idx] = { };
			else
				t2_blk = { };
/*
            pragma Debug (Debug.Print_String (
               "   RESET LVL " &
               Debug.To_String (Debug.Uint64_Type (Lvl_Idx))));
*/
		}
	}

	if (nr_of_pbas > 0) {

		for (unsigned lvl_idx = mount_point_lvl_idx; lvl_idx >= 1; lvl_idx --) {
			stopped_at_lvl_idx = lvl_idx;

			if (lvl_idx > 1) {

				if (nr_of_pbas == 0)
					break;

				auto const child_idx = (lvl_idx == mount_point_lvl_idx)
				                     ? mount_point_child_idx : 0;
				auto const child_lvl_idx = lvl_idx - 1;

				new_pbas.pbas[child_lvl_idx] = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);

				t1_blks.items[lvl_idx].nodes[child_idx] = {
					.pba  = new_pbas.pbas[child_lvl_idx],
					.gen  = curr_gen,
					.hash = { }
				};

/*
                  pragma Debug (Debug.Print_String (
                     "   SET LVL " &
                     Debug.To_String (Debug.Uint64_Type (Lvl_Idx)) &
                     " CHILD " &
                     Debug.To_String (Debug.Uint64_Type (Child_Idx)) &
                     " PBA " &
                     Debug.To_String (Debug.Uint64_Type (
                        T1_Blks (Lvl_Idx) (Child_Idx).PBA)) &
                     " GEN " &
                     Debug.To_String (Debug.Uint64_Type (
                        T1_Blks (Lvl_Idx) (Child_Idx).Gen)) &
                     " " &
                     Debug.To_String (
                        T1_Blks (Lvl_Idx) (Child_Idx).Hash) &
                     " "));
*/
			} else {
				auto const first_child_idx = (lvl_idx == mount_point_lvl_idx)
				                           ? mount_point_child_idx : 0;

				for (auto child_idx = first_child_idx; child_idx < ft_degree - 1; child_idx++) {

					if (nr_of_pbas == 0)
						break;

					auto child_pba = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);

					t2_blk.nodes[child_idx] = {
						.pba         = child_pba,
						.last_vba    = { }, /* vba invalid XXX ? */
						.alloc_gen   = { INITIAL_GENERATION },
						.free_gen    = { INITIAL_GENERATION },
						.last_key_id = { }, /* key_id_invalid XXX ? */
						.reserved    = false
					};

/*
                     pragma Debug (Debug.Print_String (
                        "   SET LVL " &
                        Debug.To_String (Debug.Uint64_Type (Lvl_Idx)) &
                        " CHILD " &
                        Debug.To_String (Debug.Uint64_Type (Child_Idx)) &
                        " PBA " &
                        Debug.To_String (Debug.Uint64_Type (
                           T2_Blk (Child_Idx).PBA)) &
                        " AGEN " &
                        Debug.To_String (Debug.Uint64_Type (
                           T2_Blk (Child_Idx).Alloc_Gen)) &
                        " FGEN " &
                        Debug.To_String (Debug.Uint64_Type (
                           T2_Blk (Child_Idx).Free_Gen)) &
                        " KEY " &
                        Debug.To_String (Debug.Uint64_Type (
                           T2_Blk (Child_Idx).Last_Key_ID)) &
                        " VBA " &
                        Debug.To_String (Debug.Uint64_Type (
                           T2_Blk (Child_Idx).Last_VBA)) &
                        " "));
*/
					nr_of_leaves = nr_of_leaves + 1;
				}
			}
		}
	}
}


void Ft_resizing::_execute_ft_extension_step(Channel        &channel,
                                             unsigned const  job_idx,
                                             bool           &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._nr_of_leaves = 0;
		channel._vba          = channel._ft_nr_of_leaves;

		channel._old_pbas        = { };
		channel._old_generations = { };
		channel._new_pbas        = { };

		channel._lvl_idx                                  = channel._ft_max_lvl_idx;
		channel._old_pbas.pbas[channel._lvl_idx]          = channel._ft_root.pba;
		channel._old_generations.values[channel._lvl_idx] = channel._ft_root.gen;

		if (channel._vba <= tree_max_max_vba(channel._ft_degree, channel._ft_max_lvl_idx)) {

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_FT_RSZG_CACHE,
				.blk_nr = channel._ft_root.pba,
				.idx    = job_idx
			};
/*
            pragma Debug (Debug.Print_String (
               "   READ LVL " &
               Debug.To_String (Debug.Uint64_Type (Job.Lvl_Idx)) &
               " PARENT FT_ROOT PBA " &
               Debug.To_String (Debug.Uint64_Type (
                  Job.FT_Root.PBA)) &
               " GEN " &
               Debug.To_String (Debug.Uint64_Type (
                  Job.FT_Root.Gen)) &
               " LEAVES " &
               Debug.To_String (Debug.Uint64_Type (
                  Job.FT_Nr_Of_Leaves)) &
               " MAX_LVL " &
               Debug.To_String (Debug.Uint64_Type (
                  Job.FT_Max_Lvl_Idx)) &
               " " &
               Debug.To_String (Job.FT_Root.Hash) &
               " "));
*/
			channel._state = Channel::State::READ_ROOT_NODE_PENDING;
			progress       = true;

		} else {

			_add_new_root_lvl_to_ft_using_pba_contingent(channel._ft_root,
			                                             channel._ft_max_lvl_idx,
			                                             channel._ft_nr_of_leaves,
			                                             channel._curr_gen,
			                                             channel._t1_blks,
			                                             channel._new_pbas,
			                                             channel._pba,
			                                             channel._nr_of_pbas);

			_add_new_branch_to_ft_using_pba_contingent(channel._ft_max_lvl_idx,
			                                           1,
			                                           channel._ft_degree,
			                                           channel._curr_gen,
			                                           channel._pba,
			                                           channel._nr_of_pbas,
			                                           channel._t1_blks,
			                                           channel._t2_blk,
			                                           channel._new_pbas,
			                                           channel._lvl_idx,
			                                           channel._nr_of_leaves);

/*
            pragma Debug (Debug.Print_String (
               "   PBAS ALLOCATED CURR_GEN " &
               Debug.To_String (Debug.Uint64_Type (Job.Curr_Gen)) &
               " "));
*/

			_set_args_for_write_back_of_inner_lvl(channel._ft_max_lvl_idx,
			                                      channel._lvl_idx,
			                                      channel._new_pbas.pbas[channel._lvl_idx],
			                                      job_idx,
			                                      channel._state,
			                                      progress,
			                                      channel._generated_prim);

		}

		break;
	case Channel::State::READ_ROOT_NODE_COMPLETED:
		_execute_ft_ext_step_read_inner_node_completed(channel, job_idx, progress);
		break;
	case Channel::State::READ_INNER_NODE_COMPLETED:
		_execute_ft_ext_step_read_inner_node_completed(channel, job_idx, progress);
		break;
	case Channel::State::ALLOC_PBA_COMPLETED:
		if (channel._alloc_lvl_idx < channel._ft_max_lvl_idx) {

			channel._alloc_lvl_idx = channel._alloc_lvl_idx + 1;

			if (channel._old_generations.values[channel._alloc_lvl_idx] == channel._curr_gen) {

				channel._new_pbas.pbas[channel._alloc_lvl_idx] = channel._old_pbas.pbas[channel._alloc_lvl_idx];

				channel._state = Channel::State::ALLOC_PBA_COMPLETED;
				progress = true;

			} else {

				channel._generated_prim = {
					.op     = Channel::Generated_prim::Type::READ,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_FT_RSZG_MT_ALLOC,
					.blk_nr = 0,
					.idx    = job_idx
				};

				channel._state = Channel::State::ALLOC_PBA_PENDING;
				progress       = true;

			}

		} else {
/*
            pragma Debug (Debug.Print_String (
               "   PBAS ALLOCATED CURR_GEN " &
               Debug.To_String (Debug.Uint64_Type (channel._Curr_Gen)) &
               " "));
*/

			_set_args_for_write_back_of_inner_lvl(channel._ft_max_lvl_idx,
			                                      channel._lvl_idx,
			                                      channel._new_pbas.pbas[channel._lvl_idx],
			                                      job_idx,
			                                      channel._state,
			                                      progress,
			                                      channel._generated_prim);

		}
		break;
	case Channel::State::WRITE_INNER_NODE_COMPLETED:

		if (not channel._generated_prim.succ) {
			class Primitive_not_successfull_ft_resizing_write_inner { };
			throw Primitive_not_successfull_ft_resizing_write_inner { };
		}

		if (channel._lvl_idx > 1) {

			auto const parent_lvl_idx = channel._lvl_idx + 1;
			auto const child_lvl_idx  = channel._lvl_idx;
			auto const child_idx = t1_child_idx_for_vba(channel._vba,
			                                            parent_lvl_idx,
			                                            channel._ft_degree);

			auto &child = channel._t1_blks.items[parent_lvl_idx].nodes[child_idx];

			child = {
				.pba     = channel._new_pbas.pbas[child_lvl_idx],
				.gen     = channel._curr_gen,
				.padding = {}
			};

			calc_sha256_4k_hash(&channel._t1_blks.items[child_lvl_idx],
			                    &child.hash);

/*
               pragma Debug (Debug.Print_String (
                  "   SET LVL " &
                  Debug.To_String (Debug.Uint64_Type (Parent_Lvl_Idx)) &
                  " CHILD " &
                  Debug.To_String (Debug.Uint64_Type (Child_Idx)) &
                  " PBA " &
                  Debug.To_String (Debug.Uint64_Type (
                     Job.T1_Blks (Parent_Lvl_Idx) (Child_Idx).PBA)) &
                  " GEN " &
                  Debug.To_String (Debug.Uint64_Type (
                     Job.T1_Blks (Parent_Lvl_Idx) (Child_Idx).Gen)) &
                  " " &
                  Debug.To_String (
                     Job.T1_Blks (Parent_Lvl_Idx) (Child_Idx).Hash) &
                  " "));
*/

			_set_args_for_write_back_of_inner_lvl(channel._ft_max_lvl_idx,
			                                      parent_lvl_idx,
			                                      channel._new_pbas.pbas[parent_lvl_idx],
			                                      job_idx,
			                                      channel._state,
			                                      progress,
			                                      channel._generated_prim);

			channel._lvl_idx += 1;

		} else {

			auto const parent_lvl_idx = channel._lvl_idx + 1;
			auto const child_lvl_idx  = channel._lvl_idx;
			auto const child_idx      = t1_child_idx_for_vba(channel._vba,
			                                                 parent_lvl_idx,
			                                                 channel._ft_degree);

			auto &child = channel._t1_blks.items[parent_lvl_idx].nodes[child_idx];
			child = {
				.pba = channel._new_pbas.pbas[child_lvl_idx],
				.gen = channel._curr_gen,
				.padding = {}
			};

			calc_sha256_4k_hash(&channel._t2_blk, &child.hash);

/*
               pragma Debug (Debug.Print_String (
                  "   SET LVL " &
                  Debug.To_String (Debug.Uint64_Type (
                     Parent_Lvl_Idx)) &
                  " CHILD " &
                  Debug.To_String (Debug.Uint64_Type (
                     Child_Idx)) &
                  " PBA " &
                  Debug.To_String (Debug.Uint64_Type (
                     channel._T1_Blks (Parent_Lvl_Idx) (Child_Idx).PBA)) &
                  " GEN " &
                  Debug.To_String (Debug.Uint64_Type (
                     channel._T1_Blks (Parent_Lvl_Idx) (Child_Idx).Gen)) &
                  " " &
                  Debug.To_String (
                     channel._T1_Blks (Parent_Lvl_Idx) (Child_Idx).Hash) &
                  " "));
*/

			_set_args_for_write_back_of_inner_lvl(channel._ft_max_lvl_idx,
			                                      parent_lvl_idx,
			                                      channel._new_pbas.pbas[parent_lvl_idx],
			                                      job_idx,
			                                      channel._state,
			                                      progress,
			                                      channel._generated_prim);

			channel._lvl_idx += 1;

		}
		break;
	case Channel::State::WRITE_ROOT_NODE_COMPLETED: {

		if (not channel._generated_prim.succ) {
			class Primitive_not_successfull_ft_resizing_write_root { };
			throw Primitive_not_successfull_ft_resizing_write_root { };
		}

		auto const child_lvl_idx = channel._lvl_idx;
		auto const child_pba     = channel._new_pbas.pbas[child_lvl_idx];

		channel._ft_root = {
			.pba = child_pba,
			.gen = channel._curr_gen,
			.padding = {}
		};

		calc_sha256_4k_hash(&channel._t1_blks.items[child_lvl_idx], &channel._ft_root);

		channel._ft_nr_of_leaves = channel._ft_nr_of_leaves + channel._nr_of_leaves;

		channel._submitted_prim.succ = true;

		channel._state = Channel::State::COMPLETED;
		progress       = true;

		break;
	}
	default:
		break;
	}
}


void Ft_resizing::execute(bool &progress)
{
	for (unsigned idx = 0; idx < NR_OF_CHANNELS; idx++) {

		Channel &channel = _channels[idx];
		Request &request { channel._request };

		switch (request._type) {
		case Request::INVALID:
			_execute_ft_extension_step(channel, idx, progress);
			break;
		case Request::FT_EXTENSION_STEP:
			break;
		}
	}
}

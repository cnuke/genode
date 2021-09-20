/*
 * \brief  Gpu Information for IRIS driver
 * \author Josef Soentgen
 * \date   2021-09-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_INFO_IRIS_H_
#define _INCLUDE__GPU_INFO_IRIS_H_

#include <base/output.h>
#include <gpu/request.h>

namespace Gpu {

	struct Info_iris;
}


/*
 * Gpu information
 *
 * Used to query information in the DRM backend
 */
struct Gpu::Info_iris
{
	using Chip_id    = Genode::uint16_t;
	using Features   = Genode::uint32_t;
	using size_t     = Genode::size_t;
	using Context_id = Genode::uint32_t;

	Chip_id    chip_id;
	Features   features;
	size_t     aperture_size;
	Context_id ctx_id;

	struct Revision      { Genode::uint8_t value; } revision;
	struct Slice_mask    { unsigned value; }        slice_mask;
	struct Subslice_mask { unsigned value; }        subslice_mask;
	struct Eu_total      { unsigned value; }        eus;
	struct Subslices     { unsigned value; }        subslices;

	Info_iris(Chip_id chip_id, Features features, size_t aperture_size,
	     Context_id ctx_id,
	     Revision rev, Slice_mask s_mask, Subslice_mask ss_mask,
	     Eu_total eu, Subslices subslice)
	:
		chip_id(chip_id), features(features),
		aperture_size(aperture_size), ctx_id(ctx_id),
		revision(rev),
		slice_mask(s_mask),
		subslice_mask(ss_mask),
		eus(eu),
		subslices(subslice)
	{ }
};

#endif /* _INCLUDE__GPU_INFO_IRIS_H_ */

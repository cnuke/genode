/*
 * \brief  Calculate SHA256 hash over data blocks of a size of 4096 bytes
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
#include <util/string.h>

/* cbe tester includes */
#include <sha256_4k_hash.h>

using namespace Genode;
using namespace Cbe;


bool Cbe::check_sha256_4k_hash(void const *data_ptr,
                               void const *exp_hash_ptr)
{
	uint8_t got_hash[32];
	calc_sha256_4k_hash(data_ptr, &got_hash);
	return !memcmp(&got_hash, exp_hash_ptr, sizeof(got_hash));
}

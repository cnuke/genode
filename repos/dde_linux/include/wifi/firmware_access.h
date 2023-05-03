/*
 * \brief  Firmware access interface
 * \author Josef Soentgen
 * \date   2023-05-05
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__FIRMWARE_ACCESS_H_
#define _WIFI__FIRMWARE_ACCESS_H_


namespace Wifi {

	struct Firmware_request
	{
		enum State { INVALID, PROBING, PROBING_COMPLETE,
		             REQUESTING, REQUESTING_COMPLETE };
		State state;
		bool success;

		char const *name;

		char   *dst;
		size_t  dst_len;

		size_t  fw_len;
	};

} /* namespace Wifi */

#endif /* _WIFI__FIRMWARE_ACCESS_H_ */

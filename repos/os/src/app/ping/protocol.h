/*
 * \brief  Supported protocols
 * \author Martin Stein
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#include <base/stdint.h>
#include <util/string.h>

namespace Genode { enum class Protocol : uint16_t { ICMP, UDP }; }

namespace Genode
{
	inline size_t parse(Span const &s, Protocol &result)
	{
		if (!strcmp(s.start, "icmp", s.num_bytes)) { result = Protocol::ICMP; return 4; }
		if (!strcmp(s.start, "udp",  s.num_bytes)) { result = Protocol::UDP;  return 3; }
		return 0;
	}
}

#endif /* __PROTOCOL_H_ */

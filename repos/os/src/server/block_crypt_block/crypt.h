/*
 * \brief  Crypt backend
 * \author Josef Soentgen
 * \date   2016-07-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CRYPT_H_
#define _CRYPT_H_

/* Genode includes */
#include <base/exception.h>
#include <base/stdint.h>
#include <block_session/connection.h>
#include <util/xml_node.h>


namespace Crypt {

	using Genode::uint8_t;
	using Genode::uint32_t;
	using Genode::uint64_t;
	using Genode::size_t;

	struct Metadata;

	struct Could_not_initialize : Genode::Exception { };

	void initialize(Genode::Xml_node, size_t);
	void cleanup();

	void process(Block::Packet_descriptor const &p, char *dst, char const *src, bool write);
}


/*
 * XXX seperate structure below from interface later
 */

struct Crypt::Metadata
{
	uint32_t size;
	enum { MAGIC = 0x5450524342444e47ULL, };
	uint64_t magic;
	uint32_t rev;
	uint64_t start_offset;
	uint64_t block_count;
	uint8_t  label[32];

	uint8_t key[64];

	union {
		uint8_t checksum[20];
		uint8_t reserved[64];
	};
} __attribute__((packed));

#endif /* _CRYPT_H_ */

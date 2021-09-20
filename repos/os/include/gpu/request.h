/*
 * \brief  Gpu Request
 * \author Josef Soentgen
 * \date   2021-09-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_REQUEST_H_
#define _INCLUDE__GPU_REQUEST_H_

namespace Gpu {

	struct Seqno { Genode::uint64_t value; };

	enum class Buffer_mapping { UNKNOWN, READ, WRITE, NOSYNC };

	struct Virtual_address { unsigned long value; };

	struct Buffer_id;
	struct Operation;
	struct Request;
} /* namespace Gpu */


struct Gpu::Buffer_id
{
	Genode::uint32_t value;

	bool valid() const
	{
		return value != 0;
	}
};


struct Gpu::Operation
{
	enum class Type {
		INVALID = 0,
		ALLOC   = 1,
		FREE    = 2,
		MAP     = 3,
		UNMAP   = 4,
		EXEC    = 5,
		WAIT    = 6,
		VIEW    = 7,
	};

	Type type;

	Virtual_address gpu_vaddr;
	bool            aperture;
	unsigned        mode;

	unsigned long  size;
	Buffer_id      id;
	Seqno          seqno;
	Buffer_mapping buffer_mapping;

	bool valid() const
	{
		return type != Type::INVALID;
	}

	static char const *type_name(Type type)
	{
		switch (type) {
		case Type::INVALID: return "INVALID";
		case Type::ALLOC:   return "ALLOC";
		case Type::FREE:    return "FREE";
		case Type::MAP:     return "MAP";
		case Type::UNMAP:   return "UNMAP";
		case Type::EXEC:    return "EXEC";
		case Type::WAIT:    return "WAIT";
		case Type::VIEW:    return "VIEW";
		}
		return "INVALID";
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, type_name(type));
	}
};


struct Gpu::Request
{
	struct Tag { unsigned long value; };

	Operation operation;

	bool success;

	Tag tag;

	bool valid() const
	{
		return operation.valid();
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, "tag=", tag.value, " success=", success,
		                   " operation=", operation);
	}
};

#endif /* _INCLUDE__GPU_REQUEST_H_ */

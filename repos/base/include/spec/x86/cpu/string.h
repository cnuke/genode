/*
 * \brief  CPU-specific memcpy
 * \author Sebastian Sumpf
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__STRING_H_
#define _INCLUDE__SPEC__X86__CPU__STRING_H_

namespace Genode {

	/**
	 * Copy memory block
	 *
	 * \param dst   destination memory block
	 * \param src   source memory block
	 * \param size  number of bytes to copy
	 *
	 * \return      number of bytes not copied
	 */
	inline size_t memcpy_cpu(void *dst, const void *src, size_t size)
	{
		if (size & 1) asm volatile ("movsb" : "+D"(dst), "+S"(src) : : "memory");
		if (size & 2) asm volatile ("movsw" : "+D"(dst), "+S"(src) : : "memory");
		size /= 4;
		asm volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c" (size) : : "memory");
		return 0;
	}
}

#endif /* _INCLUDE__SPEC__X86__CPU__STRING_H_ */

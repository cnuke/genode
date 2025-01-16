/*
 * \brief  Fallback 2D memory copy
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__INTERNAL__SLOW_H_
#define _INCLUDE__BLIT__INTERNAL__SLOW_H_

#include <blit/types.h>

namespace Blit {

	/*
	 * The back-to-front copy variants work as follows:
	 *
	 *                  normal         flipped
	 *
	 * rotated 0     0  1  2  3       3  2  1  0
	 *               4  5  6  7       7  6  5  4
	 *               8  9 10 11      11 10  9  8
	 *              12 13 14 15      15 14 13 12
	 *
	 * rotated 90   12  8  4  0       0  4  8 12
	 *              13  9  5  1       1  5  9 13
	 *              14 10  6  2       2  6 10 14
	 *              15 11  7  3       3  7 11 15
	 *
	 * rotated 180  15 14 13 12      12 13 14 15
	 *              11 10  9  8       8  9 10 11
	 *               7  6  5  4       4  5  6  7
	 *               3  2  1  0       0  1  2  3
	 *
	 * rotated 270   3  7 11 15      15 11  7  3
	 *               2  6 10 14      14 10  6  2
	 *               1  5  9 13      13  9  5  1
	 *               0  4  8 12      12  8  4  0
	 *
	 * - coordinates are given in units of 16 pixels
	 * - one pixel is 32 bit
	 * - w >= 1
	 * - h >= 1
	 * - w <= line_w,dst_w,src_w
	 */

	struct Slow;

	static inline void _sample_line(uint32_t const *src, uint32_t *dst,
	                                unsigned len, int const step)
	{
		for (; len--; src += step)
			*dst++ = *src;
	}

	static inline void _copy_line(uint32_t const *src, uint32_t *dst, unsigned len)
	{
		_sample_line(src, dst, len, 1);
	}
}


struct Blit::Slow
{
	struct B2f
	{
		static inline void r0    (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
		static inline void r90   (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
		static inline void r180  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
		static inline void r270  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	};

	struct B2f_flip
	{
		static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
		static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
		static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
		static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	};
};


void Blit::Slow::B2f::r0(uint32_t       *dst, unsigned const line_w,
                         uint32_t const *src, unsigned const w, unsigned const h)
{
	for (unsigned lines = h*16; lines; lines--) {
		_copy_line(src, dst, 16*w);
		src += 16*line_w;
		dst += 16*line_w;
	}
}


void Blit::Slow::B2f::r90(uint32_t       *dst, unsigned const dst_w,
                          uint32_t const *src, unsigned const src_w,
                          unsigned const w, unsigned const h)
{
	src += (16*h - 1)*16*src_w;

	for (unsigned i = 16*w; i; i--) {
		_sample_line(src, dst, 16*h, -16*src_w);
		src++;
		dst += 16*dst_w;
	}
}


void Blit::Slow::B2f::r180(uint32_t       *dst, unsigned const line_w,
                           uint32_t const *src, unsigned const w, unsigned const h)
{
	src += 16*h*16*line_w + 16*w - 1;

	for (unsigned i = h*16; i; i--) {
		src -= 16*line_w;
		_sample_line(src, dst, 16*w, -1);
		dst += 16*line_w;
	}
}


void Blit::Slow::B2f::r270(uint32_t       *dst, unsigned const dst_w,
                           uint32_t const *src, unsigned const src_w,
                           unsigned const w, const unsigned h)
{
	src += 16*w;

	for (unsigned i = 16*w; i; i--) {
		src--;
		_sample_line(src, dst, 16*h, 16*src_w);
		dst += 16*dst_w;
	}
}


void Blit::Slow::B2f_flip::r0(uint32_t       *dst, unsigned const line_w,
                              uint32_t const *src, unsigned const w, unsigned const h)
{
	src += 16*w - 1;

	for (unsigned lines = h*16; lines; lines--) {
		_sample_line(src, dst, 16*w, -1);
		src += 16*line_w;
		dst += 16*line_w;
	}
}


void Blit::Slow::B2f_flip::r90(uint32_t       *dst, unsigned const dst_w,
                               uint32_t const *src, unsigned const src_w,
                               unsigned const w, unsigned const h)
{
	for (unsigned i = 16*w; i; i--) {
		_sample_line(src, dst, 16*h, 16*src_w);
		src++;
		dst += 16*dst_w;
	}
}


void Blit::Slow::B2f_flip::r180(uint32_t       *dst, unsigned const line_w,
                                uint32_t const *src, unsigned const w, unsigned const h)
{
	src += 16*h*16*line_w;

	for (unsigned lines = h*16; lines; lines--) {
		src -= 16*line_w;
		_copy_line(src, dst, 16*w);
		dst += 16*line_w;
	}
}


void Blit::Slow::B2f_flip::r270(uint32_t       *dst, unsigned const dst_w,
                                uint32_t const *src, unsigned const src_w,
                                unsigned const w, const unsigned h)
{
	src += (16*h - 1)*16*src_w + 16*w;

	for (unsigned i = 16*w; i; i--) {
		src--;
		_sample_line(src, dst, 16*h, -16*src_w);
		dst += 16*dst_w;
	}
}

#endif /* _INCLUDE__BLIT__INTERNAL__SLOW_H_ */

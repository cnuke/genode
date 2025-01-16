/*
 * \brief  2D memory copy using ARM NEON
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__INTERNAL__NEON_H_
#define _INCLUDE__BLIT__INTERNAL__NEON_H_

#include <blit/types.h>

/* compiler intrinsics */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <arm_neon.h>
#pragma GCC diagnostic pop


namespace Blit { struct Neon; }


struct Blit::Neon
{
	template <typename PTR> struct Ptr4;

	static inline uint32x4_t _reversed(uint32x4_t const v)
	{
		return vrev64q_u32(vcombine_u32(vget_high_u32(v), vget_low_u32(v)));
	}

	static inline void _reverse_line(uint32x4x4_t const *src, uint32x4x4_t *dst, unsigned len)
	{
		src += len;   /* move 'src' from end of line towards begin */
		union Batch { uint32x4x4_t all; uint32x4_t v[4]; };
		Batch b;
		uint32x4_t *d = (uint32x4_t *)dst;
		while (len--) {
			b.all = *--src;
			*d++ = _reversed(b.v[3]);
			*d++ = _reversed(b.v[2]);
			*d++ = _reversed(b.v[1]);
			*d++ = _reversed(b.v[0]);
		}
	};

	static inline void _copy_line(uint32x4x4_t const *s, uint32x4x4_t *d, unsigned len)
	{
		while (len--)
			*d++ = *s++;
	};

	static inline void _rotate_4_lines(auto src_ptr, auto dst_ptr,
	                                   unsigned len, auto const src_step)
	{
		union Tile { uint32x4x4_t all; uint32x4_t row[4]; };
		Tile t;
		while (len--) {
			t.all = vld4q_lane_u32(src_ptr.p0, t.all, 3);
			t.all = vld4q_lane_u32(src_ptr.p1, t.all, 2);
			t.all = vld4q_lane_u32(src_ptr.p2, t.all, 1);
			t.all = vld4q_lane_u32(src_ptr.p3, t.all, 0);

			dst_ptr.append(t.row[0], t.row[1], t.row[2], t.row[3]);
			src_ptr.incr(src_step);
		};
	};

	template <typename PTR>
	struct Ptr4
	{
		PTR *p0, *p1, *p2, *p3;

		Ptr4(PTR *p, int w) : p0(p), p1(p + w), p2(p + 2*w), p3(p + 3*w) { }

		void incr(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void append(auto v0, auto v1, auto v2, auto v3)
		{
			*p0++ = v0; *p1++ = v1; *p2++ = v2; *p3++ = v3;
		}
	};


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


void Blit::Neon::B2f::r0(uint32_t       *dst, unsigned const line_w,
                         uint32_t const *src, unsigned const w, unsigned const h)
{
	uint32x4x4_t const *s = (uint32x4x4_t const *)src;
	uint32x4x4_t       *d = (uint32x4x4_t       *)dst;

	for (unsigned lines = h*16; lines; lines--) {
		_copy_line(s, d, w);
		s += line_w;
		d += line_w;
	}
}


void Blit::Neon::B2f::r90(uint32_t       *dst, unsigned const dst_w,
                          uint32_t const *src, unsigned const src_w,
                          unsigned const w, unsigned const h)
{
	Ptr4<uint32_t const> src_ptr4 (src + 16*src_w*(16*h - 4), 16*src_w);
	Ptr4<uint32x4_t>     dst_ptr4 ((uint32x4_t *)dst, 4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, -4*16*src_w);
		src_ptr4.incr(4);
		dst_ptr4.incr(4*4*dst_w);
	}
}


void Blit::Neon::B2f::r180(uint32_t       *dst, unsigned const line_w,
                           uint32_t const *src, unsigned const w, unsigned const h)
{
	uint32x4x4_t       *d = (uint32x4x4_t *)dst;
	uint32x4x4_t const *s = (uint32x4x4_t const *)src + 16*line_w*h;

	for (unsigned i = h*16; i; i--) {
		s -= line_w;
		_reverse_line(s, d, w);
		d += line_w;
	}
}


void Blit::Neon::B2f::r270(uint32_t       *dst, unsigned const dst_w,
                           uint32_t const *src, unsigned const src_w,
                           unsigned const w, const unsigned h)
{
	Ptr4<uint32_t const> src_ptr4 (src + 3*16*src_w + 16*w - 4, -16*src_w);
	Ptr4<uint32x4_t>     dst_ptr4 ((uint32x4_t *)dst + 3*4*dst_w, -4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, 4*16*src_w);
		src_ptr4.incr(-4);
		dst_ptr4.incr(4*4*dst_w);
	}
}


void Blit::Neon::B2f_flip::r0(uint32_t       *dst, unsigned const line_w,
                              uint32_t const *src, unsigned const w, unsigned const h)
{
	uint32x4x4_t const *s = (uint32x4x4_t const *)src;
	uint32x4x4_t       *d = (uint32x4x4_t       *)dst;

	for (unsigned lines = h*16; lines; lines--) {
		_reverse_line(s, d, w);
		s += line_w;
		d += line_w;
	}
}


void Blit::Neon::B2f_flip::r90(uint32_t       *dst, unsigned const dst_w,
                               uint32_t const *src, unsigned const src_w,
                               unsigned const w, unsigned const h)
{
	Ptr4<uint32_t const> src_ptr4 (src + 3*16*src_w, -16*src_w);
	Ptr4<uint32x4_t>     dst_ptr4 ((uint32x4_t *)dst, 4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, 4*16*src_w);
		src_ptr4.incr(4);
		dst_ptr4.incr(4*4*dst_w);
	}
}


void Blit::Neon::B2f_flip::r180(uint32_t       *dst, unsigned const line_w,
                                uint32_t const *src, unsigned const w, unsigned const h)
{
	uint32x4x4_t const *s = (uint32x4x4_t const *)src + 16*line_w*h;
	uint32x4x4_t       *d = (uint32x4x4_t       *)dst;

	for (unsigned lines = h*16; lines; lines--) {
		s -= line_w;
		_copy_line(s, d, w);
		d += line_w;
	}
}


void Blit::Neon::B2f_flip::r270(uint32_t       *dst, unsigned const dst_w,
                                uint32_t const *src, unsigned const src_w,
                                unsigned const w, const unsigned h)
{
	Ptr4<uint32_t const> src_ptr4 (src + (16*h - 4)*16*src_w + 16*w, 16*src_w);
	Ptr4<uint32x4_t>     dst_ptr4 ((uint32x4_t *)dst + 3*4*dst_w, -4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		src_ptr4.incr(-4);
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, -4*16*src_w);
		dst_ptr4.incr(4*4*dst_w);
	}
}

#endif /* _INCLUDE__BLIT__INTERNAL__NEON_H_ */

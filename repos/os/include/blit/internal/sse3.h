/*
 * \brief  2D memory copy using SSE3
 * \author Norman Feske
 * \date   2025-01-21
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__INTERNAL__SSE3_H_
#define _INCLUDE__BLIT__INTERNAL__SSE3_H_

#include <blit/types.h>

/* compiler intrinsics */
#ifndef _MM_MALLOC_H_INCLUDED   /* discharge dependency from stdlib.h */
#define _MM_MALLOC_H_INCLUDED
#define _MM_MALLOC_H_INCLUDED_PREVENTED
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <immintrin.h>
#pragma GCC diagnostic pop
#ifdef  _MM_MALLOC_H_INCLUDED_PREVENTED
#undef  _MM_MALLOC_H_INCLUDED
#undef  _MM_MALLOC_H_INCLUDED_PREVENTED
#endif


namespace Blit { struct Sse3; };


struct Blit::Sse3
{
	struct Ptr4
	{
		__m128i *p0, *p1, *p2, *p3;

		Ptr4(__m128i *p, int w) : p0(p), p1(p + w), p2(p + 2*w), p3(p + 3*w) { }

		void incr(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }
	};

	struct Ptr4_const
	{
		__m128i const *p0, *p1, *p2, *p3;

		Ptr4_const(__m128i const *p, int w) : p0(p), p1(p + w), p2(p + 2*w), p3(p + 3*w) { }

		void incr(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }
	};

	static inline void _reverse_line(__m128i const *s, __m128i *d, unsigned len)
	{
		static constexpr int reversed = (0 << 6) | (1 << 4) | (2 << 2) | 3;

		d += 4*len;   /* move 'dst' from end towards begin */

		while (len--) {
			__m128i const v0 = _mm_load_si128(s++);
			__m128i const v1 = _mm_load_si128(s++);
			__m128i const v2 = _mm_load_si128(s++);
			__m128i const v3 = _mm_load_si128(s++);
			_mm_stream_si128(--d, _mm_shuffle_epi32(v0, reversed));
			_mm_stream_si128(--d, _mm_shuffle_epi32(v1, reversed));
			_mm_stream_si128(--d, _mm_shuffle_epi32(v2, reversed));
			_mm_stream_si128(--d, _mm_shuffle_epi32(v3, reversed));
		}
	};

	static inline void _copy_line(__m128i const *s, __m128i *d, unsigned len)
	{
		while (len--) {
			__m128i const v0 = _mm_load_si128(s++);
			__m128i const v1 = _mm_load_si128(s++);
			__m128i const v2 = _mm_load_si128(s++);
			__m128i const v3 = _mm_load_si128(s++);
			_mm_stream_si128(d++, v0); /* bypass cache */
			_mm_stream_si128(d++, v1);
			_mm_stream_si128(d++, v2);
			_mm_stream_si128(d++, v3);
		}
	};

	static inline void _rotate_4_lines(auto src_ptr, auto dst_ptr,
	                                   unsigned len, auto const src_step)
	{
		union Tile { __m128i pi[4]; __m128  ps[4]; } t;

		while (len--) {
			t.pi[0] = _mm_load_si128(src_ptr.p3);
			t.pi[1] = _mm_load_si128(src_ptr.p2);
			t.pi[2] = _mm_load_si128(src_ptr.p1);
			t.pi[3] = _mm_load_si128(src_ptr.p0);
			_MM_TRANSPOSE4_PS(t.ps[0], t.ps[1], t.ps[2], t.ps[3]);
			_mm_stream_si128(dst_ptr.p0++, t.pi[0]);
			_mm_stream_si128(dst_ptr.p1++, t.pi[1]);
			_mm_stream_si128(dst_ptr.p2++, t.pi[2]);
			_mm_stream_si128(dst_ptr.p3++, t.pi[3]);
			src_ptr.incr(src_step);
		};
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


void Blit::Sse3::B2f::r0(uint32_t       *dst, unsigned const line_w,
                         uint32_t const *src, unsigned const w, unsigned const h)
{
	__m128i const *s = (__m128i const *)src;
	__m128i       *d = (__m128i       *)dst;

	for (unsigned lines = h*16; lines; lines--) {
		_copy_line(s, d, w);
		s += 4*line_w;
		d += 4*line_w;
	}
}


void Blit::Sse3::B2f::r90(uint32_t       *dst, unsigned const dst_w,
                          uint32_t const *src, unsigned const src_w,
                          unsigned const w, unsigned const h)
{
	Ptr4_const src_ptr4 ((__m128i const *)(src + 16*src_w*(16*h - 4)), 4*src_w);
	Ptr4       dst_ptr4 ((__m128i *)dst, 4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, -4*4*src_w);
		src_ptr4.incr(1);
		dst_ptr4.incr(4*4*dst_w);
	}
}


void Blit::Sse3::B2f::r180(uint32_t       *dst, unsigned const line_w,
                           uint32_t const *src, unsigned const w, unsigned const h)
{
	__m128i       *d = (__m128i *)dst;
	__m128i const *s = (__m128i const *)src + 4*16*line_w*h;

	for (unsigned i = h*16; i; i--) {
		s -= 4*line_w;
		_reverse_line(s, d, w);
		d += 4*line_w;
	}
}


void Blit::Sse3::B2f::r270(uint32_t       *dst, unsigned const dst_w,
                           uint32_t const *src, unsigned const src_w,
                           unsigned const w, const unsigned h)
{
	Ptr4_const src_ptr4 ((__m128i const *)(src + 3*16*src_w + 16*w - 4), -4*src_w);
	Ptr4       dst_ptr4 ((__m128i *)dst + 3*4*dst_w, -4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, 4*4*src_w);
		src_ptr4.incr(-1);
		dst_ptr4.incr(4*4*dst_w);
	}
}


void Blit::Sse3::B2f_flip::r0(uint32_t       *dst, unsigned const line_w,
                              uint32_t const *src, unsigned const w, unsigned const h)
{
	__m128i const *s = (__m128i const *)src;
	__m128i       *d = (__m128i       *)dst;

	for (unsigned lines = h*16; lines; lines--) {
		_reverse_line(s, d, w);
		s += 4*line_w;
		d += 4*line_w;
	}
}


void Blit::Sse3::B2f_flip::r90(uint32_t       *dst, unsigned const dst_w,
                               uint32_t const *src, unsigned const src_w,
                               unsigned const w, unsigned const h)
{
	Ptr4_const src_ptr4 ((__m128i const *)(src + 3*16*src_w), -4*src_w);
	Ptr4       dst_ptr4 ((__m128i *)dst, 4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, 4*4*src_w);
		src_ptr4.incr(1);
		dst_ptr4.incr(4*4*dst_w);
	}
}


void Blit::Sse3::B2f_flip::r180(uint32_t       *dst, unsigned const line_w,
                                uint32_t const *src, unsigned const w, unsigned const h)
{
	__m128i const *s = (__m128i const *)src + 4*16*line_w*h;
	__m128i       *d = (__m128i       *)dst;

	for (unsigned lines = h*16; lines; lines--) {
		s -= 4*line_w;
		_copy_line(s, d, w);
		d += 4*line_w;
	}
}


void Blit::Sse3::B2f_flip::r270(uint32_t       *dst, unsigned const dst_w,
                                uint32_t const *src, unsigned const src_w,
                                unsigned const w, const unsigned h)
{
	Ptr4_const src_ptr4 ((__m128i const *)(src + (16*h - 4)*16*src_w + 16*w), 4*src_w);
	Ptr4       dst_ptr4 ((__m128i *)dst + 3*4*dst_w, -4*dst_w);

	for (unsigned i = 4*w; i; i--) {
		src_ptr4.incr(-1);
		_rotate_4_lines(src_ptr4, dst_ptr4, 4*h, -4*4*src_w);
		dst_ptr4.incr(4*4*dst_w);
	}
}

#endif /* _INCLUDE__BLIT__INTERNAL__SSE3_H_ */

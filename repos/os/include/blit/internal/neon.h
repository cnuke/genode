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

	struct Src_ptr4
	{
		uint32x4_t const *p0, *p1, *p2, *p3;

		inline Src_ptr4(uint32x4_t const *p, int const step)
		:
			p0(p), p1(p0 + step), p2(p1 + step), p3(p2 + step)
		{ }

		void incr_4x(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }
		void incr_y (int v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void prefetch() const
		{
			__builtin_prefetch(p0); __builtin_prefetch(p1);
			__builtin_prefetch(p2); __builtin_prefetch(p3);
		}

		void load(uint32x4x4_t &tile) const { tile = { *p0, *p1, *p2, *p3 }; }
	};

	struct Dst_ptr4
	{
		uint32_t *p0, *p1, *p2, *p3;

		Dst_ptr4(uint32_t *p, int const step)
		:
			p0(p), p1(p0 + step), p2(p1 + step), p3(p2 + step)
		{ }

		void incr_x(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }
		void incr_y(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void store(uint32x4x4_t const &tile)
		{
			vst4q_lane_u32(p0, tile, 0);
			vst4q_lane_u32(p1, tile, 1);
			vst4q_lane_u32(p2, tile, 2);
			vst4q_lane_u32(p3, tile, 3);
		}
	};

	struct Steps
	{
		int const src_4x, src_y, dst_x, dst_y;

		void incr_x_4 (Src_ptr4 &p) const { p.incr_4x(src_4x     ); };
		void incr_x_16(Src_ptr4 &p) const { p.incr_4x(src_4x << 2); };
		void incr_y_4 (Src_ptr4 &p) const { p.incr_y (src_y  << 2); };
		void incr_y_16(Src_ptr4 &p) const { p.incr_y (src_y  << 4); };
		void incr_x_4 (Dst_ptr4 &p) const { p.incr_x (dst_x  << 2); };
		void incr_x_16(Dst_ptr4 &p) const { p.incr_x (dst_x  << 4); };
		void incr_y_4 (Dst_ptr4 &p) const { p.incr_y (dst_y  << 2); };
		void incr_y_16(Dst_ptr4 &p) const { p.incr_y (dst_y  << 4); };
	};

	__attribute__((optimize("-O3")))
	static inline void _load_prefetch_store(Src_ptr4 &src, Dst_ptr4 &dst, Steps const steps)
	{
		uint32x4x4_t tile;
		src.load(tile);
		steps.incr_y_4(src);
		src.prefetch();
		dst.store(tile);
		steps.incr_x_4(dst);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_16x4(Src_ptr4 src, Dst_ptr4 dst, Steps const steps)
	{
		for (unsigned i = 0; i < 4; i++)
			_load_prefetch_store(src, dst, steps);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_16x4_last(Src_ptr4 src, Dst_ptr4 dst, Steps const steps)
	{
		for (unsigned i = 0; i < 3; i++)
			_load_prefetch_store(src, dst, steps);

		uint32x4x4_t tile;
		src.load(tile);
		dst.store(tile);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_16x16(Src_ptr4 src, Dst_ptr4 dst, Steps const steps)
	{
		for (unsigned i = 0; i < 3; i++) {
			_rotate_16x4(src, dst, steps);
			steps.incr_y_4(dst);
			steps.incr_x_4(src);
		}
		_rotate_16x4_last(src, dst, steps);
	}

	static inline void _rotate_16_lines(Src_ptr4 src, Dst_ptr4 dst,
	                                    Steps const steps, unsigned n)
	{
		for (; n > 1; n--) {
			_rotate_16x16(src, dst, steps);
			steps.incr_x_16(dst);
			steps.incr_y_16(src);
		};
		_rotate_16x16(src, dst, steps);
	};

	static inline void _rotate(Src_ptr4 src, Dst_ptr4 dst,
	                           Steps const steps, unsigned w, unsigned h)
	{
		for (unsigned i = w; i; i--) {
			_rotate_16_lines(src, dst, steps, h);
			steps.incr_x_16(src);
			steps.incr_y_16(dst);
		}
	}

	struct B2f
	{
		static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
		static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
		static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
		static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
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
	Steps const steps { 1, -4*int(src_w), 1, 16*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src + 4*src_w*(16*h - 1), steps.src_y);
	Dst_ptr4 dst_ptr4 (dst,                                    steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
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
	Steps const steps { 1, 4*int(src_w), 1, -16*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src,              steps.src_y);
	Dst_ptr4 dst_ptr4 (dst + 16*int(dst_w)*(w*16 - 1), steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
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
	Steps const steps { 1, 4*int(src_w), 1, 16*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src, steps.src_y);
	Dst_ptr4 dst_ptr4 (dst,               steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
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
	Steps const steps { 1, -4*int(src_w), 1, -16*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src + 4*src_w*(16*h - 1), steps.src_y);
	Dst_ptr4 dst_ptr4 (dst + 16*int(dst_w)*(w*16 - 1),         steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}

#endif /* _INCLUDE__BLIT__INTERNAL__NEON_H_ */

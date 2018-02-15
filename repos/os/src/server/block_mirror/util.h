/*
 * \brief  Block mirror component utilities
 * \author Josef Soentgen
 * \date   2018-02-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UITL_H_
#define _UITL_H_

/* Genode includes */
#include <util/string.h>


namespace Util {

	using Label = Genode::String<32>;

	/*
	 * Wrapped Bit_array with a convience methods
	 */

	enum { BITS = sizeof(long) * 8, };

	template <Genode::size_t NUM>
	class Id_allocator : public Genode::Bit_array<BITS>
	{
		private :
			Genode::size_t _used { 0 };

		public:

			enum { INVALID = 0xff, };

			Id_allocator() { }

			/*******************
			 ** Bit interface **
			 *******************/

			void set(unsigned i)
			{
				Genode::Bit_array<BITS>::set(i, 1);
			}

			void clear(unsigned i)
			{
				Genode::Bit_array<BITS>::clear(i, 1);
			}

			bool empty() const { return get(0, BITS) == 0; }

			/****************************
			 ** Id_allocator interface **
			 ****************************/

			unsigned alloc()
			{
				for (Genode::size_t i = 0; i < NUM; i++) {
					if (!Genode::Bit_array<BITS>::get(i, 1)) {
						Genode::Bit_array<BITS>::set(i, 1);
						_used++;
						return i;
					}
				}
				return INVALID;
			}

			void free(unsigned const i)
			{
				_used--;
				Genode::Bit_array<BITS>::clear(i, 1);
			}

			bool allocated(unsigned const i) const
			{
				return Genode::Bit_array<BITS>::get(i, 1);
			}

			unsigned next_id() const
			{
				static unsigned iter = ~0U;
				return ++iter % _used;
			}
	};

} /* namespace Util */

#endif /* _UITL_H_ */

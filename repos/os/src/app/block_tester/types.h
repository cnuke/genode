/*
 * \brief  Common types used by the block tester
 * \author Norman Feske
 * \date   2025-03-25
 */

/*
 * Copyright (C) 2016-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>

namespace Test {

	using namespace Genode;

	struct Scenario;

	struct Stats
	{
		uint64_t rx, tx;
		size_t   bytes;
		unsigned completed;
		unsigned job_cnt;
	};

	using block_number_t = Block::block_number_t;

	/*
	 * Wrapper to get suffixed uint64_t values
	 */
	class Length_in_bytes
	{
		uint64_t _n;

		public:

		/**
		 * Default constructor
		 */
		Length_in_bytes() : _n(0) { }

		/**
		 * Constructor, to be used implicitly via assignment operator
		 */
		Length_in_bytes(Genode::uint64_t n) : _n(n) { }

		/**
		 * Convert number of bytes to 'uint64_t' value
		 */
		operator Genode::uint64_t() const { return _n; }

		void print(Output &output) const
		{
			using Genode::print;

			enum { KB = 1024UL, MB = KB*1024UL, GB = MB*1024UL };

			if      (_n      == 0) print(output, 0);
			else if (_n % GB == 0) print(output, _n/GB, "G");
			else if (_n % MB == 0) print(output, _n/MB, "M");
			else if (_n % KB == 0) print(output, _n/KB, "K");
			else                   print(output, _n);
		}
	};

	inline size_t ascii_to(const char *s, Length_in_bytes &result)
	{
		unsigned long long res = 0;

		/* convert numeric part of string */
		size_t i = ascii_to_unsigned(s, res, 0);

		/* handle suffixes */
		if (i > 0)
			switch (s[i]) {
				case 'G': res *= 1024; [[fallthrough]];
				case 'M': res *= 1024; [[fallthrough]];
				case 'K': res *= 1024; i++;
				default: break;
			}

		result = res;
		return i;
	}
}


struct Test::Scenario : Interface, private Fifo<Scenario>::Element
{
	friend class Fifo<Scenario>;

	struct Attr
	{
		size_t   io_buffer;
		uint64_t progress_interval;
		size_t   batch;
		bool     copy;
		bool     verbose;

		static Attr from_xml(Xml_node const &node)
		{
			return {
				.io_buffer         = node.attribute_value("io_buffer", Number_of_bytes(4*1024*1024)),
				.progress_interval = node.attribute_value("progress", (uint64_t)0),
				.batch             = node.attribute_value("batch",   1u),
				.copy              = node.attribute_value("copy",    true),
				.verbose           = node.attribute_value("verbose", false),
			};
		}
	};

	Attr const attr;

	Scenario(Xml_node const &node) : attr(Attr::from_xml(node)) { }

	struct Init_attr
	{
		size_t         block_size;        /* size of one block in bytes */
		block_number_t block_count;       /* number of blocks */
		size_t         scratch_buffer_size;
	};

	[[nodiscard]] virtual bool init(Init_attr const &) = 0;

	struct No_job { };
	using Next_job_result = Attempt<Block::Operation, No_job>;
	virtual Next_job_result next_job(Stats const &) = 0;

	virtual size_t request_size() const = 0;
	virtual char const *name() const = 0;
	virtual void print(Output &) const = 0;
};

#endif /* _TYPES_H_ */

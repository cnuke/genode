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

/* Genode includes */
#include <base/log.h>
#include <util/string.h>
#include <util/xml_node.h>

/* library includes */
// #include <argon2.h>

/* local includes */
#include <crypt.h>


static Genode::size_t _block_size;


/* maybe allocate extra Ram_dataspace? */
template <Genode::size_t CAPACITY>
struct Guarded_string
{
	Genode::String<CAPACITY> s;

	Guarded_string() { }
	~Guarded_string() { Genode::memset(&s, 0, sizeof(s)); }

	Genode::String<CAPACITY> *string() { return &s; }
};


void Crypt::initialize(Genode::Xml_node config, Genode::size_t block_size)
{
	Guarded_string<128> passphrase;
	Guarded_string<32>  salt;
	unsigned pdf_m, pdf_p, pdf_t;

	try {
		config.attribute("passphrase").value(passphrase.string());
		config.attribute("salt").value(salt.string());
		config.attribute("pdf_memory").value(&pdf_m);
		config.attribute("pdf_parallel").value(&pdf_p);
		config.attribute("pdf_time").value(&pdf_t);
	}
	catch (...) {
		Genode::error("Could not read passphrase");
		throw Crypt::Could_not_initialize();
	}

	// uint8_t key[32];

	// argon2_context context = {
	// 	key,
	// 	sizeof(key),
	// 	passphrase.string().string(),
	// 	passphrase.string().length(),
	// 	salt.string().string(),
	// 	salt.string().length(),
	// 	NULL, 0,
	// 	NULL, 0,
	// 	pdf_t, pdf_m, pdf_p, pdf_p,
	// 	ARGON2_VERSION_13,
	// 	NULL, NULL,
	// 	0,
	// };

	// int const res = argon2i_ctx(&context);
	// if (ARGON2_OK != res) {
	// 	throw Crypt::Could_not_initialize();
	// }
}


void Crypt::cleanup()
{
}


void Crypt::process(Block::Packet_descriptor const &p, char *dst, char const *src, bool write)
{
	Genode::log("block_number: ", p.block_number(), " block_count: ", p.block_count());

	size_t const size = _block_size * p.block_count();

	Genode::memcpy(dst, src, size);
}

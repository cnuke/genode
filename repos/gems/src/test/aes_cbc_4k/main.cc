/*
 * \brief  Test for using aes_cbc_4k
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2018-12-20
 */

/*
 * Copyright (C) 2018-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>

#include <libc/component.h>

#include <aes_cbc_4k/aes_cbc_4k.h>

namespace Test {
	struct Main;
	using namespace Genode;
}


namespace Genode {
	template <typename T>
	static inline void print_local(Output &out, T const &data, unsigned show_max)
	{
		constexpr unsigned BYTES_PER_LINE = 32;

		unsigned rest = sizeof(data.values) < show_max ? sizeof(data.values) : show_max;
		unsigned const lines = rest / BYTES_PER_LINE + ((rest % BYTES_PER_LINE == 0) ? 0 : 1);

		for (unsigned line = 0; line < lines; line++) {

			unsigned const max = rest > BYTES_PER_LINE ? BYTES_PER_LINE : rest;
			for (unsigned i = 0; i < max; i++)
				print(out, Genode::Hex(data.values[line*BYTES_PER_LINE + i],
				                       Genode::Hex::OMIT_PREFIX,
				                       Genode::Hex::PAD));

			rest -= max;
			if (!rest) break;

			print(out, "\n");
		}
		if (sizeof(data.values) != show_max)
			print(out, "\n...");
	}

	static inline void print(Output &out, Aes_cbc_4k::Ciphertext const &data) {
		print_local(out, data, sizeof(data.values)); }

	static inline void print(Output &out, Aes_cbc_4k::Plaintext const &data) {
		print_local(out, data, sizeof(data.values)); }

	static inline void print(Output &out, Aes_cbc_4k::Key const &data) {
		print_local(out, data, sizeof(data.values)); }
}


struct Test::Main
{
	Env &_env;

	Attached_rom_dataspace _crypt_extern { _env, "openssl_crypted" };
	Attached_rom_dataspace _plaintext    { _env, "plaintext" };
	Attached_rom_dataspace _key          { _env, "key" };

	Aes_cbc_4k::Ciphertext _ciphertext { };
	Aes_cbc_4k::Plaintext  _decrypted_plaintext  { };

	Main(Env &env) : _env(env)
	{
		Attached_rom_dataspace config(env, "config");

		Aes_cbc_4k::Block_number const block_number { config.xml().attribute_value("block_number", 0U) };

		Aes_cbc_4k::Key        const &key         = *_key.local_addr<Aes_cbc_4k::Key>();
		Aes_cbc_4k::Plaintext  const &plaintext   = *_plaintext.local_addr<Aes_cbc_4k::Plaintext>();
		Aes_cbc_4k::Ciphertext const &cryptextern = *_crypt_extern.local_addr<Aes_cbc_4k::Ciphertext>();

		log("block number: ", block_number.value);
		log("key: '", key, "'");

		Aes_cbc_4k::encrypt(key, block_number, plaintext, _ciphertext);

		if (_crypt_extern.size() < sizeof(_ciphertext.values)) {
			error("ciphertext size mismatch: ",
			      _crypt_extern.size(), " vs ", sizeof(_ciphertext.values));
			return;
		}

		/* compare ciphertext done by us with external ciphertext */
		if (memcmp(_ciphertext.values, cryptextern.values, sizeof(_ciphertext.values))) {
			log("ciphertext by us:\n",     _ciphertext);
			log("ciphertext by extern:\n", cryptextern);

			error("ciphertext by us differs from external ciphertext");
			return;
		}

		Aes_cbc_4k::decrypt(key, block_number, _ciphertext, _decrypted_plaintext);

		/* compare decrypted ciphertext with original plaintext */
		if (memcmp(plaintext.values, _decrypted_plaintext.values, sizeof(plaintext))) {
			log("plaintext before:\n", plaintext);
			log("plaintext  after:\n", _decrypted_plaintext);

			error("plaintext differs from decrypted ciphertext");
			return;
		}

		log("Test succeeded");
	}
};


#if 1
/* disable Component::construct if you want to use printf in the LibC version */
Genode::Env *__genode_env;


void Component::construct(Genode::Env &env)
{
	/* make ada-runtime happy */
	__genode_env = &env;
	env.exec_static_constructors();

	Genode::error("Component version");
	static Test::Main inst(env);
}

#endif

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		Genode::error("Libc version");
		static Test::Main inst(env);
	});
}

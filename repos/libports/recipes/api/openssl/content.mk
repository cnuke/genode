MIRROR_FROM_REP_DIR := \
	lib/import/import-libcrypto.mk \
	lib/import/import-libssl.mk \
	lib/import/import-openssl.mk \
	lib/symbols/libcrypto \
	lib/symbols/libssl

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openssl)

content: include

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/

content: src/lib/openssl

src/lib/openssl:
	mkdir -p $@
	cp -r $(REP_DIR)/src/lib/openssl/crypto $@/
	cp -r $(REP_DIR)/src/lib/openssl/spec $@/

content: LICENSE libcrypto.pc libssl.pc openssl.pc

LICENSE:
	cp $(PORT_DIR)/src/lib/openssl/LICENSE $@

libcrypto.pc:
	echo 'Name: OpenSSL-libcrypto' > $@
	echo 'Description: OpenSSL cryptography library' >> $@
	echo 'Version: 1.1.1t' >> $@
	echo 'Libs: -l:libcrypto.lib.so' >> $@

libssl.pc:
	echo 'Name: OpenSSL-libssl' > $@
	echo 'Description: Secure Sockets Layer and cryptography libraries' >> $@
	echo 'Version: 1.1.1t' >> $@
	echo 'Requires.private: libcrypto' >> $@
	echo 'Libs: -l:libssl.lib.so' >> $@

openssl.pc:
	echo 'Name: OpenSSL' > $@
	echo 'Description: Secure Sockets Layer and cryptography libraries and tools' >> $@
	echo 'Version: 1.1.1t' >> $@
	echo 'Requires: libssl libcrypto' >> $@

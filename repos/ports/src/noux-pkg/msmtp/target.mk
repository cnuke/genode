NOUX_CONFIGURE_ARGS += --with-ssl=openssl \
                       --with-libidn=no

LIBS += libiconv libssl libcrypto

include $(REP_DIR)/mk/noux.mk

Makefile: dummy_libs

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libcrypto.a libssl.a

libcrypto.a:
	$(VERBOSE)$(AR) -rc $@

libssl.a:
	$(VERBOSE)$(AR) -rc $@

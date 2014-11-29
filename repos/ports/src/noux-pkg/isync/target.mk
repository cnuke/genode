LIBS += libcrypto libdb libssl

include $(REP_DIR)/mk/noux.mk

Makefile: dummy_libs

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libcrypto.a libdb.a libssl.a

libcrypto.a:
	$(VERBOSE)$(AR) -rc $@

libdb.a:
	$(VERBOSE)$(AR) -rc $@

libssl.a:
	$(VERBOSE)$(AR) -rc $@

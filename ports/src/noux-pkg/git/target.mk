NOUX_CONFIGURE_ARGS = \
                      --with-openssl \
                      --with-curl \
                      --with-expat \
                      --without-iconv \
                      --without-pthread \
                      --without-tcltk

#
# Needed for <sys/types.h>
#
NOUX_CFLAGS += -D__BSD_VISIBLE

#
# XXX: Are these defs actaully needed?
#
NOUX_CFLAGS += -DUSE_ST_TIMESPEC \
               -DNO_SYMLINK_HEAD \
               -DNO_MMAP \
               -DNO_PERL \
               -DNO_PYTHON

#
# We need startup.lib.a etc.
#
NOUX_LDFLAGS += $(NOUX_LIBS)

#
# HACK: We also need libgcc.a because we build with -nostdlib
#
NOUX_MAKE_ENV += LIBGCC=$(LIBGCC)

#
# Makefile options
#
NOUX_MAKE_ENV += NO_MMAP=1 NO_SYMLINK_HEAD=1 NO_PERL=1 NO_PYTHON=1 NO_TCLTK=1

NOUX_INSTALL_TARGET = install

LIBS += libc_resolv libiconv libssl curl expat ncurses zlib

noux_env.sh: mirror_git_src.tag flush_config_cache.tag

noux_built.tag: Makefile Makefile_patch

mirror_git_src.tag:
	$(VERBOSE)cp -af $(NOUX_PKG_DIR)/* $(PWD)
	$(VERBOSE)sed -i "/exit/s/^/touch ..\/Makefile\n/" $(PWD)/configure
	@touch $@

flush_config_cache.tag:
	$(VERBOSE)rm -f $(PWD)/config.cache
	@touch $@

Makefile_patch: Makefile
	$(VERBOSE)sed -i "s|$(BUILTIN_OBJS) $(ALL_LDFLAGS) $(LIBS)|$(ALL_LDFLAGS) $(BUILTIN_OBJS) $(LIBS)|g" $(PWD)/Makefile
#
# Make the linking tests succeed
#
Makefile: dummy_libs

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libc.a libcurl.a libexpat.a libiconv.a libssl.a libz.a

libc.a:
	$(VERBOSE)$(AR) -rc $@

libcurl.a:
	$(VERBOSE)$(AR) -rc $@

libexpat.a:
	$(VERBOSE)$(AR) -rc $@

libiconv.a:
	$(VERBOSE)$(AR) -rc $@

libssl.a:
	$(VERBOSE)$(AR) -rc $@

libz.a:
	$(VERBOSE)$(AR) -rc $@

include $(REP_DIR)/mk/noux.mk

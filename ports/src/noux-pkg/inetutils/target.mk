
# for now enable a few programs only
NOUX_CONFIGURE_ARGS = --without-wrap --without-pam \
		      --disable-ipv6 --enable-clients \
		      --enable-ftp --enable-ftpd \
		      --disable-hostname --enable-ping \
		      --disable-ping6 --disable-rcp \
		      --disable-rexec --disable-rexecd \
		      --disable-rlogin --disable-rlogind \
		      --disable-rsh --disable-rshd \
		      --disable-talk --disable-talkd \
		      --enable-telnet --enable-telnetd \
		      --disable-tftp --disable-tftpd \
		      --disable-logger --disable-syslogd \
		      --disable-inetd --enable-whois \
		      --disable-uucpd --disable-ifconfig --disable-traceroute

LIBS += readline libutil libc_resolv

NOUX_MAKE_ENV = $(NOUX_ENV)

include $(REP_DIR)/mk/noux.mk

#
# Make the readline linking test succeed
#
Makefile: dummy_libs

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libreadline.a

libreadline.a:
	        $(VERBOSE)$(AR) -rc $@

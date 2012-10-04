INETUTILS     = inetutils-1.9.1
INETUTILS_TGZ = $(INETUTILS).tar.gz
INETUTILS_URL = http://ftp.gnu.org/gnu/inetutils/$(INETUTILS_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(INETUTILS)

prepare:: $(CONTRIB_DIR)/$(INETUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(INETUTILS_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(INETUTILS_URL) && touch $@

$(CONTRIB_DIR)/$(INETUTILS): $(DOWNLOAD_DIR)/$(INETUTILS_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/inetutils/build.patch

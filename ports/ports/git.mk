GIT     = git-1.8.1.4
GIT_TGZ = $(GIT).tar.gz
GIT_URL = http://git-core.googlecode.com/files/$(GIT_TGZ)
#
# Interface to top-level prepare Makefile
#
PORTS += $(GIT)

prepare:: $(CONTRIB_DIR)/$(GIT)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GIT_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(GIT_URL) && touch $@

$(CONTRIB_DIR)/$(GIT): $(DOWNLOAD_DIR)/$(GIT_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/git/makefile.patch
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/git/configure.patch

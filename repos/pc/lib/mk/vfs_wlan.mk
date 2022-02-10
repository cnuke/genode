SRC_CC = vfs.cc

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

INC_DIR += $(DDE_LINUX_DIR)/src/include

LIBS := wlan

vpath %.cc $(REP_DIR)/src/lib/vfs/wlan

SHARED_LIB := yes

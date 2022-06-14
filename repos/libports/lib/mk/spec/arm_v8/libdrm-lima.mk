include $(REP_DIR)/lib/mk/libdrm.inc

include $(call select_from_repositories,lib/import/import-libdrm.mk)
include $(call select_from_repositories,lib/import/import-mesa_api.mk)

SRC_CC := ioctl_lima.cc

vpath %.cc $(REP_DIR)/src/lib/mesa/lima

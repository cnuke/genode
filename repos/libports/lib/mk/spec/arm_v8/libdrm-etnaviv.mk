include $(REP_DIR)/lib/mk/libdrm.inc

include $(call select_from_repositories,lib/import/import-libdrm-etnaviv.mk)

SRC_C  += $(addprefix etnaviv/,$(notdir $(wildcard $(DRM_SRC_DIR)/etnaviv/*.c)))
SRC_CC := ioctl_etnaviv.cc

#vpath %.c  $(DRM_SRC_DIR)
#vpath %.c  $(LIB_DIR)
#vpath %.cc $(LIB_DIR)

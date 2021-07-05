ifeq ($(CONTRIB_DIR),)
DRM_SRC_DIR := $(call select_from_ports,libdrm)/src/lib/libdrm
else
DRM_SRC_DIR = $(call select_from_ports,libdrm)/src/lib/libdrm
endif

INC_DIR += $(DRM_SRC_DIR) $(addprefix $(DRM_SRC_DIR)/,include/drm include)
INC_DIR += $(DRM_SRC_DIR) $(addprefix $(DRM_SRC_DIR)/,etnaviv)

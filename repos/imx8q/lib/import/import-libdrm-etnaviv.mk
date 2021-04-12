DRM_SRC_DIR := $(call select_from_ports,libdrm-etnaviv)/src/lib/libdrm
INC_DIR     += $(DRM_SRC_DIR) $(addprefix $(DRM_SRC_DIR)/,etnaviv include/drm include)

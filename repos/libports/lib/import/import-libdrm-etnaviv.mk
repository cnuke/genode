include $(call select_from_repositories,/lib/import/import-libdrm.inc)

INC_DIR += $(DRM_SRC_DIR) $(addprefix $(DRM_SRC_DIR)/,etnaviv)

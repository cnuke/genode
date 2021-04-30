SHARED_LIB := yes
LIBS       += etnaviv libdrm-etnaviv

SRC_C += gallium/winsys/etnaviv/drm/etnaviv_drm_winsys.c

CC_OPT += -DGALLIUM_ETNAVIV

include $(REP_DIR)/lib/mk/mesa-21.inc

# use etnaviv_drmif.h from mesa DRM backend
INC_DIR += $(MESA_SRC_DIR)/src/etnaviv

SHARED_LIB = yes
LIBS       = base libc egl-21 libdrm-etnaviv

include $(REP_DIR)/lib/mk/mesa-common-21.inc

SRC_CC := drm_init.cc
SRC_C = platform_etnaviv.c

CC_OPT += -DHAVE_GENODE_PLATFORM

INC_DIR += $(MESA_SRC_DIR)/src/egl/drivers/dri2 \
           $(MESA_SRC_DIR)/src/egl/main \
           $(MESA_SRC_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mesa

vpath %.c  $(LIB_DIR)/etnaviv
vpath %.cc $(LIB_DIR)/etnaviv

CC_CXX_WARN_STRICT =

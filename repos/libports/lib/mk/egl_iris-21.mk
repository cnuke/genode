SHARED_LIB = yes
LIBS       = libc egl-21 iris

include $(REP_DIR)/lib/mk/mesa-common-21.inc

SRC_C   = platform_iris.c
SRC_CC  = drm_init.cc

CC_OPT += -DHAVE_GENODE_PLATFORM

INC_DIR += $(MESA_SRC_DIR)/src/egl/drivers/dri2 \
           $(MESA_SRC_DIR)/src/egl/main \
           $(MESA_SRC_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mesa

vpath %.c  $(LIB_DIR)/iris
vpath %.cc $(LIB_DIR)/iris

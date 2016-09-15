LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/framebuffer/intel
SRC_DIR         = $(REP_DIR)/src/lib/framebuffer/intel

CC_OLEVEL = -O0
CC_OPT += -gdwarf-2
LIBS    += intel_fb_include libc-setjmp blit

SRC_C   :=
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/char/agp/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/i2c/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/i2c/algos/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/i915/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/video/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/video/fbdev/core/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/lib/*.c))

#
# Genode part
#
SRC_C    += dummies.c i915_params.c lx_emul_c.c
SRC_CC   += main.cc lx_emul.cc

# lx_kit
SRC_CC  += irq.cc \
           malloc.cc \
           mapped_io_mem_range.cc \
           pci.cc \
           printf.cc \
           scheduler.cc \
           timer.cc \
           work.cc

#
# Reduce build noise of compiling contrib code
#
CC_C_OPT += -Wall -Wno-uninitialized -Wno-unused-but-set-variable \
            -Wno-unused-variable -Wno-unused-function \
            -Wno-pointer-arith -Wno-pointer-sign \
            -Wno-int-to-pointer-cast -Wno-maybe-uninitialized

vpath %.c $(LX_CONTRIB_DIR)/drivers/char/agp
vpath %.c $(LX_CONTRIB_DIR)/drivers/i2c
vpath %.c $(LX_CONTRIB_DIR)/drivers/i2c/algos
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm/i915
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm
vpath %.c $(LX_CONTRIB_DIR)/drivers/video
vpath %.c $(LX_CONTRIB_DIR)/drivers/video/fbdev/core
vpath %.c $(LX_CONTRIB_DIR)/lib

vpath %.c  $(SRC_DIR)
vpath %.cc $(SRC_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit


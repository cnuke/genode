TARGET  := gfxinit_fb_drv
LIBS    := ada_rts libhwbase libgfxinit
SRC_CC  := component.cc
SRC_ADA := main.adb
SRC_ADA += hw-gfx-gma-gfx.adb

INC_DIR += $(PRG_DIR)

# default opts
CC_ADA_OPT += -gnatA -gnatp -gnatwa.eD.HHTU.U.W.Y -gnatyN

# gfxtest specs
# GFX_DIR := $(call select_from_ports,libgfxinit)/src/lib/libgfxinit/gfxtest
# INC_DIR += $(GFX_DIR)

# binding
ADA_BIND := yes

vpath %.adb $(PRG_DIR)/

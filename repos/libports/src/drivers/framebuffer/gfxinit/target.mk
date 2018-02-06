TARGET  := gfxinit_fb_drv
LIBS    := ada_rts libhwbase libgfxinit blit

SRC_CC  := component.cc
SRC_ADA := hw-gfx-gma-gfx.adb

INC_DIR += $(PRG_DIR)

# default opts
CC_ADA_OPT += -gnatA -gnatp -gnatwa.eD.HHTU.U.W.Y -gnatyN

# binding
ADA_BIND := yes

vpath %.adb $(PRG_DIR)/

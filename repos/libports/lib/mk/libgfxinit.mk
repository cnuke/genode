LIBS    := base libhwbase ada_rts

LIB_DIR := $(REP_DIR)/src/lib/libgfxinit

SRC_ADA := hw-gfx-gma-config.ads

# libgfxinit
FILTER_OUT_GFXINIT :=

LIBGFXINIT_DIR  := $(call select_from_ports,libgfxinit)/src/lib/libgfxinit
SRC_ADA_gfxinit += $(filter-out $(FILTER_OUT_GFXINIT),$(addprefix common/,$(notdir $(wildcard $(LIBGFXINIT_DIR)/common/*.adb))))
SRC_ADA_gfxinit += $(addprefix common/haswell/,$(notdir $(wildcard $(LIBGFXINIT_DIR)/common/haswell/*.adb)))
SRC_ADA_gfxinit += $(addprefix common/haswell_shared/,$(notdir $(wildcard $(LIBGFXINIT_DIR)/common/haswell_shared/*.adb)))
SRC_ADA_gfxinit += common/hw-gfx-dp_defs.ads
SRC_ADA_gfxinit += common/hw-gfx-gma-dp_aux_ch.ads
SRC_ADA_gfxinit += common/hw-gfx-gma-dp_info.ads
SRC_ADA_gfxinit += common/hw-gfx-i2c.ads
SRC_ADA_gfxinit += common/hw-gfx.ads
SRC_ADA_gfxinit += common/hw-gfx-gma-pch.ads
SRC_ADA_gfxinit += common/haswell/hw-gfx-gma-ddi_phy.ads
SRC_ADA_gfxinit += common/haswell/hw-gfx-gma-plls-lcpll.ads
SRC_ADA_gfxinit += common/haswell/hw-gfx-gma-power_and_clocks.ads
SRC_ADA_gfxinit += common/haswell_shared/hw-gfx-gma-ddi_phy_stub.ads
INC_DIR_gfxinit += $(LIBGFXINIT_DIR)/common
INC_DIR_gfxinit += $(LIBGFXINIT_DIR)/common/haswell
INC_DIR_gfxinit += $(LIBGFXINIT_DIR)/common/haswell_shared

SRC_ADA += $(SRC_ADA_gfxinit)
INC_DIR += $(INC_DIR_gfxinit)
INC_DIR += $(REP_DIR)/src/lib/libgfxinit

# default opts
CC_ADA_OPT += -gnatA -gnatp -gnatwa.eD.HHTU.U.W.Y -gnatyN

# binding
ADA_BIND := yes

ADA_INC_DIR += $(INC_DIR_gfxinit)
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit/common
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit/common/haswell
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit/common/haswell_shared

vpath %.adb $(LIBGFXINIT_DIR)/
vpath %.ads $(LIBGFXINIT_DIR)/
vpath %.adb $(LIB_DIR)/
vpath %.ads $(LIB_DIR)/

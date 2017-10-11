LIBS    := base ada_rts

LIB_DIR := $(REP_DIR)/src/lib/libhwbase

SRC_CC := glue.cc

SRC_ADA := hw-config.ads
SRC_ADA += hw-debug_sink.adb
SRC_ADA += hw-pci-dev.adb
SRC_ADA += hw-port_io.adb
SRC_ADA += hw-time-timer.adb

# libhwbase
FILTER_OUT_HWBASE := common/hw-port_io.adb

LIBHWBASE_DIR  := $(call select_from_ports,libhwbase)/src/lib/libhwbase
SRC_ADA_hwbase := $(addprefix ada/dynamic_mmio/,$(notdir $(wildcard $(LIBHWBASE_DIR)/ada/dynamic_mmio/*.adb)))
SRC_ADA_hwbase += $(filter-out $(FILTER_OUT_HWBASE),$(addprefix common/,$(notdir $(wildcard $(LIBHWBASE_DIR)/common/*.adb))))
SRC_ADA_hwbase += $(addprefix debug/,$(notdir $(wildcard $(LIBHWBASE_DIR)/debug/*.adb)))
SRC_ADA_hwbase += common/hw.ads
SRC_ADA_hwbase += common/hw-pci.ads
SRC_ADA_hwbase += common/hw-sub_regs.ads
INC_DIR_hwbase := $(LIBHWBASE_DIR)/ada/dynamic_mmio
INC_DIR_hwbase += $(LIBHWBASE_DIR)/common
INC_DIR_hwbase += $(LIBHWBASE_DIR)/debug

SRC_ADA += $(SRC_ADA_hwbase)
INC_DIR += $(INC_DIR_hwbase)
INC_DIR += $(REP_DIR)/src/lib/libhwbase

# default opts
CC_ADA_OPT += -gnatA -gnatp -gnatwa.eD.HHTU.U.W.Y -gnatyN

ADA_BIND    := yes
ADA_INC_DIR += $(INC_DIR_hwbase)

vpath %.adb $(LIBHWBASE_DIR)/
vpath %.ads $(LIBHWBASE_DIR)/
vpath %.adb $(LIB_DIR)/
vpath %.ads $(LIB_DIR)/
vpath %.cc  $(REP_DIR)/src/lib/libhwbase

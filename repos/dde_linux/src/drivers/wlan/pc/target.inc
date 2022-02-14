REQUIRES := x86

REL_PRG_DIR := $(PRG_DIR)/../..

TARGET  := pc_wlan_drv
LIBS    := base pc_lx_emul
INC_DIR := $(REL_PRG_DIR)
SRC_CC  += main.cc
SRC_CC  += misc.cc
SRC_CC  += time.cc
SRC_CC  += firmware.cc

SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += $(notdir $(wildcard $(REL_PRG_DIR)/generated_dummies.c))
SRC_C   += lx_emul/spec/x86/common_dummies.c
SRC_C   += lx_emul/spec/x86/pci.c

CC_C_OPT += -I$(LX_SRC_DIR)/drivers/net/wireless/intel/iwlwifi
CC_C_OPT += -I$(LX_SRC_DIR)/include/linux

CC_C_OPT += -Wno-address-of-packed-member

vpath %.c $(REL_PRG_DIR)
vpath %.cc $(REL_PRG_DIR)
vpath lx_emul/spec/x86/common_dummies.c $(REP_DIR)/src/lib

#
# Genode C-API backends
#

SRC_CC  += genode_c_api/uplink.cc

vpath genode_c_api/uplink.cc $(subst /genode_c_api,,$(call select_from_repositories,src/lib/genode_c_api))

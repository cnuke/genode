REQUIRES := x86

TARGET  := x86_usb_host_drv
LIBS    := base x86_64_lx_emul
INC_DIR := $(PRG_DIR)
SRC_CC  += main.cc
SRC_CC  += misc.cc
SRC_CC  += time.cc
SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += usb.c
SRC_C   += $(notdir $(wildcard $(PRG_DIR)/generated_dummies.c))
SRC_C   += lx_emul/spec/x86/common_dummies.c
SRC_C   += lx_emul/spec/x86/pci.c
#SRC_C   += lx_emul/shadow/drivers/pci/pci-driver.c

vpath lx_emul/spec/x86/common_dummies.c $(REP_DIR)/src/lib

#
# Genode C-API backends
#

SRC_CC  += genode_c_api/usb.cc

vpath genode_c_api/usb.cc $(subst /genode_c_api,,$(call select_from_repositories,src/lib/genode_c_api))

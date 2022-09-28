include $(REP_DIR)/lib/mk/dde_bsd_audio.inc

SRC_C  += bsd_emul_pci.c
SRC_CC += pci.cc

# enable when debugging
CC_C_OPT += -DAUDIO_DEBUG
CC_C_OPT += -DAZALIA_DEBUG
CC_C_OPT += -DDIAGNOSTIC

# HDA driver
SRC_C += dev/pci/azalia.c dev/pci/azalia_codec.c

# vi: set ft=make :

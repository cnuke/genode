LIBS    := base

RTS_DIR := $(REP_DIR)/src/lib/ada_rts

SRC_ADA := $(notdir $(wildcard $(RTS_DIR)/adainclude/*.adb))
SRC_ADA += a-unccon.ads \
           ada.ads      \
           g-souinf.ads \
           gnat.ads     \
           i-c.ads      \
           interfac.ads \
           s-unstyp.ads \
           system.ads

INC_DIR += $(RTS_DIR)/adainclude

# enable rts building
CC_ADA_OPT += -gnatg

# default opts
CC_ADA_OPT += -gnatA -gnatp -gnatwa.eD.HHTU.U.W.Y -gnatyN

ADA_BIND    := yes
ADA_INC_DIR := $(RTS_DIR)/adainclude

vpath %.adb $(RTS_DIR)/adainclude
vpath %.ads $(RTS_DIR)/adainclude

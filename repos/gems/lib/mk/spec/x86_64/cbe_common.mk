CBE_DIR := $(call select_from_ports,cbe)

LIBS += spark

INC_DIR += $(CBE_DIR)/src/lib/cbe_common

SRC_ADB += cbe.adb
SRC_ADB += cbe-debug.adb
SRC_ADB += cbe-request.adb
SRC_ADB += cbe-primitive.adb

vpath % $(CBE_DIR)/src/lib/cbe_common

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/cbe_common/pragmas.adc

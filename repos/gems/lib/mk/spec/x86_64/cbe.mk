CBE_DIR := $(call select_from_ports,cbe)

LIBS    += spark cbe_common

INC_DIR += $(CBE_DIR)/src/lib/cbe

SRC_ADB += cbe-library.adb
SRC_ADB += cbe-request_pool.adb
SRC_ADB += cbe-generic_index_queue.adb

vpath % $(CBE_DIR)/src/lib/cbe

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/cbe/pragmas.adc

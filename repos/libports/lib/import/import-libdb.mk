INC_DIR += $(call select_from_ports,bdb)/src/lib/bdb/src

ifeq ($(filter-out $(SPECS),32bit),)
TARGET_CPUARCH=32bit
else ifeq ($(filter-out $(SPECS),64bit),)
TARGET_CPUARCH=64bit
endif

REP_INC_DIR += src/lib/bdb/$(TARGET_CPUARCH)
REP_INC_DIR += src/lib/bdb

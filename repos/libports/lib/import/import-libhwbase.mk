LIBHWBASE_DIR := $(REP_DIR)/src/lib/libhwbase
INC_DIR       += $(LIBHWBASE_DIR)

LIBHWBASE_PORT_DIR := $(call select_from_ports,libhwbase)/src/lib/libhwbase
INC_DIR += $(LIBHWBASE_PORT_DIR)/ada/dynamic_mmio
INC_DIR += $(LIBHWBASE_PORT_DIR)/common
INC_DIR += $(LIBHWBASE_PORT_DIR)/debug

ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libhwbase
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libhwbase/ada/dynamic_mmio
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libhwbase/common
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libhwbase/debug

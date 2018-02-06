LIBGFXINIT_DIR := $(REP_DIR)/src/lib/libgfxinit
INC_DIR        += $(LIBGFXINIT_DIR)

LIBGFXINIT_PORT_DIR := $(call select_from_ports,libgfxinit)/src/lib/libgfxinit
INC_DIR += $(LIBGFXINIT_PORT_DIR)
INC_DIR += $(LIBGFXINIT_PORT_DIR)/common
INC_DIR += $(LIBGFXINIT_PORT_DIR)/common/haswell
INC_DIR += $(LIBGFXINIT_PORT_DIR)/common/haswell_shared

ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit/common
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit/common/haswell
ADA_INC_DIR += $(BUILD_BASE_DIR)/var/libcache/libgfxinit/common/haswell_shared

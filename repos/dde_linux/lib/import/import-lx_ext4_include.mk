EXT4_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/ext4
SRC_DIR          := $(REP_DIR)/src/lib/ext4

# architecture-dependent includes
ifeq ($(filter-out $(SPECS),x86),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86
  ifeq ($(filter-out $(SPECS),32bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_32
  endif # 32bit
  ifeq ($(filter-out $(SPECS),64bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_64
  endif # 64bit
endif # x86


#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(SRC_DIR) \
           $(SRC_DIR)/include
INC_DIR += $(REP_DIR)/src/include
INC_DIR += $(ARCH_SRC_INC_DIR)
INC_DIR += $(EXT4_CONTRIB_DIR)/include \
           $(EXT4_CONTRIB_DIR)/include/uapi \
INC_DIR += $(LIB_CACHE_DIR)/lx_ext4_include/include/include/include

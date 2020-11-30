LIBGO_PORT_DIR := $(call select_from_ports,libgo)
# add incudes from port source in contrib
#INC_DIR += $(LIBGO_PORT_DIR)/include
# add includes from build like var/libcache
#INC_DIR += $(LIB_CACHE_DIR)/libgo/include
# add includes from build like noux-pkg/libgo
INC_DIR += $(BUILD_BASE_DIR)/noux-pkg/libgo/

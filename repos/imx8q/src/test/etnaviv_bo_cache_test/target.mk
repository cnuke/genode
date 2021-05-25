DRM_SRC_DIR := $(call select_from_ports,libdrm-etnaviv)/src/lib/libdrm

TARGET := test-etnaviv_bo_cache_test
LIBS   := libc libdrm-etnaviv

SRC_CC := startup.cc
SRC_C  := etnaviv_bo_cache_test.c

vpath %.c  $(DRM_SRC_DIR)/tests/etnaviv
vpath %.cc $(REP_DIR)/src/test/

SRC_DIR := $(call select_from_ports,etnaviv_gpu_tests)/src/test/etnaviv_gpu_tests

TARGET := test-cube_gc7000
LIBS   := libc libdrm-etnaviv

INC_DIR += $(PRG_DIR)/include
INC_DIR += $(SRC_DIR)/src

SRC_CC := startup.cc
SRC_C  := dummies.c
SRC_C  += \
          drm_setup.c gpu_code.c write_bmp.c \
          etnaviv_tiling.c etnaviv_blt.c
SRC_C  += cube_gc7000.c

vpath %.c  $(SRC_DIR)/src
vpath %.cc $(REP_DIR)/src/test/

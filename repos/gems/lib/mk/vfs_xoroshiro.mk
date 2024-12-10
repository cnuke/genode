VFS_DIR := $(REP_DIR)/src/lib/vfs/xoroshiro

SRC_CC := vfs.cc

# be explicit about the header
BASE_INC_DIR := $(call select_from_repositories,src/include/base/internal/xoroshiro.h)
XOR_INC_DIR  := $(BASE_INC_DIR:/base/internal/xoroshiro.h=)
INC_DIR += $(XOR_INC_DIR)


LD_OPT += --version-script=$(VFS_DIR)/symbol.map

SHARED_LIB := yes

vpath %.cc $(VFS_DIR)

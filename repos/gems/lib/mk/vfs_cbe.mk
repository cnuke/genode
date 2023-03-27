LIB_DIR := $(REP_DIR)/src/lib/vfs/cbe

SRC_CC := vfs.cc

INC_DIR += $(LIB_DIR)

vpath % $(LIB_DIR)

LIBS += cbe

SHARED_LIB := yes

CC_CXX_WARN_STRICT :=

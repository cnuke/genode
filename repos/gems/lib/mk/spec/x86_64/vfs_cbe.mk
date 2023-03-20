SRC_CC := vfs.cc
SRC_CC += block_io.cc
SRC_CC += crypto.cc
SRC_CC += free_tree.cc
SRC_CC += meta_tree.cc
SRC_CC += module.cc
SRC_CC += request_pool.cc
SRC_CC += sha256_4k_hash.cc
SRC_CC += superblock_control.cc
SRC_CC += trust_anchor.cc
SRC_CC += vfs_utilities.cc
SRC_CC += virtual_block_device.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/cbe
INC_DIR += $(REP_DIR)/src/app/cbe_tester

LIBS += libcrypto

vpath % $(REP_DIR)/src/lib/vfs/cbe
vpath % $(REP_DIR)/src/app/cbe_tester

SHARED_LIB := yes

CC_CXX_WARN_STRICT :=

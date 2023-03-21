REQUIRES += x86_64

TARGET := cbe_init

SRC_CC := main.cc
SRC_CC += \
          block_allocator.cc \
          block_io.cc \
          crypto.cc \
          ft_initializer.cc \
          module.cc \
          sha256_4k_hash.cc \
          sb_initializer.cc \
          trust_anchor.cc \
          vbd_initializer.cc \
          vfs_utilities.cc

INC_DIR += $(REP_DIR)/src/app/cbe_tester
INC_DIR += $(PRG_DIR)
LIBS    += base vfs libcrypto

vpath main.cc $(PRG_DIR)
vpath    %.cc $(REP_DIR)/src/app/cbe_tester

CONFIG_XSD = config.xsd
CC_CXX_WARN_STRICT_CONVERSION =

TARGET := cbe_tester

SRC_CC += main.cc

INC_DIR += $(PRG_DIR)

LIBS += base
LIBS += cbe

CC_CXX_WARN_STRICT_CONVERSION =

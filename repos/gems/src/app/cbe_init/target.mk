TARGET := cbe_init

SRC_CC += main.cc

INC_DIR += $(PRG_DIR)

LIBS += base
LIBS += cbe

CONFIG_XSD := config.xsd

CC_CXX_WARN_STRICT_CONVERSION =

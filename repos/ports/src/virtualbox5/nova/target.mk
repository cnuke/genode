TARGET   = virtualbox5-nova
REQUIRES = nova

LIBS    += virtualbox5-nova
LIBS    += blit
LIBS    += tracer

include $(REP_DIR)/src/virtualbox5/target.inc

vpath frontend/% $(REP_DIR)/src/virtualbox5/
vpath %.cc       $(REP_DIR)/src/virtualbox5/

CC_CXX_WARN_STRICT =

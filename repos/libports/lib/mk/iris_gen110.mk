LIBS = libc

include $(REP_DIR)/lib/mk/mesa-common-21.inc

CC_OPT  += -DGEN_VERSIONx10=110
include $(REP_DIR)/lib/mk/iris_gen.inc


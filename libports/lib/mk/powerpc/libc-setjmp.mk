LIBC_GEN_POWERPC_DIR = $(LIBC_DIR)/libc/powerpc/gen

SRC_S  = _setjmp.S setjmp.S

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.S $(LIBC_GEN_POWERPC_DIR)

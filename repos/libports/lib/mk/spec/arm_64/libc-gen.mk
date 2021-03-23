include $(REP_DIR)/lib/mk/libc-gen.inc

LIBC_GEN_ARM64_DIR = $(LIBC_DIR)/lib/libc/aarch64/gen

#INC_DIR += $(LIBC_DIR)/lib/msun/aarch64

SRC_S += _ctx_start.S sigsetjmp.S
SRC_C += flt_rounds.c fpgetmask.c fpsetmask.c infinity.c

vpath %.c $(LIBC_GEN_ARM64_DIR)
vpath %.S $(LIBC_GEN_ARM64_DIR)

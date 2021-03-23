MCONTEXT_DIR := $(REP_DIR)/src/lib/mcontext-support

SRC_S  := getcontext.S setcontext.S swapcontext.S
SRC_CC := alloc_secondary_stack.cc
SRC_C  := makecontext.c
LIBS   := base libc

vpath %.cc $(MCONTEXT_DIR)
vpath %.S  $(MCONTEXT_DIR)/spec/$(SPEC_ARCH)
vpath %.c  $(MCONTEXT_DIR)/spec/$(SPEC_ARCH)

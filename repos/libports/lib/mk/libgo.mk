LIBGO_DIR := $(call select_from_ports,libgo)/src/lib/libgo

#SRC_C = $(notdir $(wildcard $(LIBATOMIC_DIR)/SDLnet*.c))

#vpath %.c $(LIBATOMIC_DIR)

#INC_DIR += $(LIBATOMIC_DIR)

LIBS += libc libatomic libbacktrace libffi

#SHARED_LIB = no

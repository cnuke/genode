SQLITE_DIR = $(call select_from_ports,sqlite)/src/lib/sqlite

INC_DIR += $(SQLITE_DIR)

LIBS += jitterentropy vfs libc

SRC_C = sqlite3.c

CC_OPT += -Wno-unused -Wno-misleading-indentation -Wno-return-local-addr

vpath %.c  $(SQLITE_DIR)

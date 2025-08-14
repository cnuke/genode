TARGET = test-sqlite_speedtest
LIBS   = sqlite posix
SRC_C  = speedtest1.c

CC_OPT += -Wno-unused

SQLITE_DIR = $(call select_from_ports,sqlite)/src/lib/sqlite

vpath %.c $(SQLITE_DIR)/

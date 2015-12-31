LIBMIKMOD_PORT_DIR := $(call select_from_ports,libmikmod)
LIBMIKMOD_DIR      := $(LIBMIKMOD_PORT_DIR)/src/lib/libmikmod

SHARED_LIB = yes

# use default warning level for 3rd-party code
#CC_WARN =

INC_DIR += $(LIBMIKMOD_PORT_DIR)/include

CC_OPT += -DMIKMOD_BUILD -DHAVE_LIMITS_H -DHAVE_MALLOC_H -DHAVE_UNISTD_H -DHAVE_FCNTL_H
CC_OPT += -DHAVE_STDLIB_H # needed for SDL_stdinc.h
CC_OPT += -DDRV_RAW -DDRV_SDL -DDRV_WAV

# driver files
SRC_C = drivers/drv_nos.c  \
        drivers/drv_raw.c  \
        drivers/drv_sdl.c  \
        drivers/drv_wav.c

# loader files
SRC_C += $(addprefix loaders/,$(notdir $(wildcard $(LIBMIKMOD_DIR)/loaders/*.c)))

# mmio files
SRC_C += $(addprefix mmio/,$(notdir $(wildcard $(LIBMIKMOD_DIR)/mmio/*.c)))

# depackers files
SRC_C += $(addprefix depackers/,$(notdir $(wildcard $(LIBMIKMOD_DIR)/depackers/*.c)))

# playercode files
SRC_C += $(addprefix playercode/,$(notdir $(wildcard $(LIBMIKMOD_DIR)/playercode/*.c)))

LIBS = libc sdl

vpath % $(REP_DIR)/src/lib/libmikmod
vpath % $(LIBMIKMOD_DIR)

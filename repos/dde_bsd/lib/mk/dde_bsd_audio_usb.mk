LIB_DIR     = $(REP_DIR)/src/lib/audio
LIB_INC_DIR = $(LIB_DIR)/include

AUDIO_CONTRIB_DIR := $(call select_from_ports,dde_bsd)/src/lib/audio

# XXX properly split up MD stuff
INC_DIR += $(LIB_INC_DIR)/spec/x86_64 $(LIB_INC_DIR)/spec/x86

#
# Set include paths up before adding the dde_bsd_audio_include library
# because it will use INC_DIR += and must be at the end
#
INC_DIR += $(LIB_DIR)
INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(AUDIO_CONTRIB_DIR)

LIBS += dde_bsd_audio_include

SRC_C  := bsd_emul_usb.c
SRC_CC += usb.cc

CC_OPT += -Wno-unused-but-set-variable

# disable builtins
CC_OPT += -fno-builtin-printf -fno-builtin-snprintf -fno-builtin-vsnprintf \
          -fno-builtin-malloc -fno-builtin-free -fno-builtin-log -fno-builtin-log2

CC_OPT += -D_KERNEL

# libkern
SRC_C += lib/libkern/strchr.c

# enable when debugging
CC_OPT += -DUAUDIO_DEBUG

CC_C_OPT += -Wno-pointer-sign
CC_C_OPT += -Wno-maybe-uninitialized

# driver
SRC_C += dev/usb/uaudio.c

vpath %.c  $(AUDIO_CONTRIB_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)

# vi: set ft=make :

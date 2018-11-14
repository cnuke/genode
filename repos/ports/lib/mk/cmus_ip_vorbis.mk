CMUS_DIR := $(call select_from_ports,cmus)/src/app/cmus

SRC_C := ip/vorbis.c

# XXX remove later
SRC_C += xmalloc.c debug.c keyval.c read_wrapper.c file.c comment.c uchar.c gbuf.c convert.c

INC_DIR := $(CMUS_DIR)

LIBS := libc libvorbis libogg

SHARED_LIB := yes

vpath %.c $(CMUS_DIR)

include $(REP_DIR)/lib/mk/spec/x86_64/av.inc

include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avfilter.mk

LIBAVFILTER_DIR = $(call select_from_ports,libav)/src/lib/libav/libavfilter

-include $(LIBAVFILTER_DIR)/Makefile
-include $(LIBAVFILTER_DIR)/x86/Makefile

INC_DIR += $(LIBAVFILTER_DIR)/../

YASM       := yasm
YASMFLAGS  := -f elf -m amd64 -DPIC
CONFIG_ASM := $(REP_DIR)/src/lib/libav/config.asm 

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)$(YASM) $(YASMFLAGS) -P$(CONFIG_ASM) -I $(LIBAVFILTER_DIR)/x86 -I $(LIBAVFILTER_DIR)/.. $< -o $@

SRC_C += $(YASM-OBJS:.o=.asm)
SRC_C += $(YASM-OBJS-yes:.o=.asm)

vpath % $(LIBAVFILTER_DIR)

CC_CXX_WARN_STRICT =

include $(REP_DIR)/lib/mk/spec/x86_64/av.inc

include $(REP_DIR)/lib/mk/spec/x86/avcodec.inc

YASM       := yasm
YASMFLAGS  := -f elf -m amd64 -DPIC
CONFIG_ASM := $(REP_DIR)/src/lib/libav/config.asm

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)$(YASM) $(YASMFLAGS) -P$(CONFIG_ASM) -I $(LIBAVCODEC_DIR)/x86 -I $(LIBAVCODEC_DIR)/.. $< -o $@

SRC_C += $(YASM-OBJS:.o=.asm)
SRC_C += $(YASM-OBJS-yes:.o=.asm)

SRC_C += $(MMX-OBJS:.o=.asm)
SRC_C += $(MMX-OBJS-yes:.o=.asm)

#vpath %.asm $(LIBAVCODEC_DIR)


CC_CXX_WARN_STRICT =

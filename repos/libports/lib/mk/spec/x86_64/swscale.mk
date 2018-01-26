include $(REP_DIR)/lib/mk/spec/x86_64/av.inc

include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-swscale.mk

CC_WARN += -Wno-switch

LIBSWSCALE_DIR = $(call select_from_ports,libav)/src/lib/libav/libswscale

-include $(LIBSWSCALE_DIR)/Makefile
-include $(LIBSWSCALE_DIR)/x86/Makefile

INC_DIR += $(LIBSWSCALE_DIR)/../

YASM       := yasm
YASMFLAGS  := -f elf -m amd64 -DPIC
CONFIG_ASM := $(REP_DIR)/src/lib/libav/config.asm 

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)$(YASM) $(YASMFLAGS) -P$(CONFIG_ASM) -I $(LIBSWSCALE_DIR)/x86 -I $(LIBSWSCALE_DIR)/.. $< -o $@

SRC_C += $(YASM-OBJS:.o=.asm)
SRC_C += $(YASM-OBJS-yes:.o=.asm)

vpath % $(LIBSWSCALE_DIR)

CC_CXX_WARN_STRICT =

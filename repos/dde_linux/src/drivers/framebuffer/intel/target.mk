REQUIRES = x86

TARGET   = intel_fb_drv
LIBS     = base intel_fb_drv intel_fb_include libc-setjmp server config blit
SRC_CC   = main.cc lx_emul.cc dummies.cc

# lx_kit
SRC_CC  += irq.cc \
           malloc.cc \
           mapped_io_mem_range.cc \
           pci.cc \
           scheduler.cc \
           timer.cc \
           work.cc

INC_DIR += $(REP_DIR)/src/include

vpath %.cc $(PRG_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

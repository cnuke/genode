REQUIRES   = powerpc
SHARED_LIB = yes
DIR        = $(REP_DIR)/src/lib/ldso

INC_DIR += $(DIR)/contrib/powerpc \
           $(DIR)/include/libc/libc-powerpc \
           $(DIR)/include/powerpc

vpath %    $(DIR)/contrib/powerpc

include $(DIR)/target.inc

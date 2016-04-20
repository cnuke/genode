SHARED_LIB = yes

LIB_DIR     = $(REP_DIR)/src/lib/ext4
LIB_INC_DIR = $(LIB_DIR)/include

LIBS += lx_ext4_include

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/ext4
EXT4_DIR       := $(LX_CONTRIB_DIR)/fs/ext4

#CC_OLEVEL = -O2

CC_OPT += -U__linux__ -D__KERNEL__

CC_WARN = -Wall -Wno-unused-variable -Wno-uninitialized \
          -Wno-unused-function -Wno-overflow -Wno-pointer-arith \
          -Wno-sign-compare

CC_C_OPT  += -Wno-unused-but-set-variable -Wno-pointer-sign

CC_C_OPT  += -include $(LIB_INC_DIR)/lx_emul.h
CC_CXX_OPT = -fpermissive

SRC_CC = dummies.cc lxcc_emul.cc init.cc
SRC_C  = lxc_emul.c

# lx_kit
CC_OPT += -DUSE_INTERNAL_SETJMP
SRC_CC += malloc.cc printf.cc scheduler.cc timer.cc work.cc
SRC_S  += setjmp.S

CC_OPT += -DCONFIG_PRINTK
CC_OPT += -DCONFIG_EXT4_DEBUG

SRC_C_fs  = mbcache.c
SRC_C    += $(addprefix fs/,$(SRC_C_fs))

# SRC_C += $(addprefix fs/ext4/,$(notdir $(wildcard $(EXT4_DIR)/*.c)))
SRC_C_ext4  = balloc.c bitmap.c dir.c file.c fsync.c ialloc.c inode.c page-io.c \
              ioctl.c namei.c super.c symlink.c hash.c resize.c extents.c \
              ext4_jbd2.c migrate.c mballoc.c block_validity.c move_extent.c \
              mmp.c indirect.c extents_status.c xattr.c xattr_user.c \
              xattr_trusted.c inline.c readpage.c sysfs.c
SRC_C      += $(addprefix fs/ext4/,$(SRC_C_ext4))

SRC_C_jbd2  = $(addprefix fs/jbd2/,$(notdir $(wildcard $(LX_CONTRIB_DIR)/fs/jbd2/*.c)))
SRC_C      += $(SRC_C_jbd2)

SRC_C_lib  = halfmd4.c rbtree.c
SRC_C     += $(addprefix lib/,$(SRC_C_lib))

# SRC_C_ext4_acl  = xattr_security.c
# SRC_C          += $(addprefix fs/ext4/,$(SRC_C_ext4_acl))

# SRC_C_ext4_crypto  = crypto_policy.c crypto.c crypto_key.c crypto_fname.c
# SRC_C             += $(addprefix fs/ext4/,$(SRC_C_ext4_acl))


vpath %.c  $(LX_CONTRIB_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit
vpath %.S  $(REP_DIR)/src/lx_kit/spec/x86_64/

include $(REP_DIR)/lib/import/import-argon2.mk

LIBS += libc pthread

#src/argon2.c src/core.c src/blake2/blake2b.c src/thread.c src/encoding.c src/opt.c -o libargon2.so

SRC_C  = argon2.c core.c blake2b.c encoding.c ref.c thread.c

CC_OPT += -fvisibility=hidden

CC_DEF += -DA2_VISCTL=1

vpath %.c  $(ARGON2_DIR)/src/lib/argon2/src
vpath %.c  $(ARGON2_DIR)/src/lib/argon2/src/blake2

SHARED_LIB = yes

ARGON2_DIR := $(call select_from_ports,argon2)

INC_DIR += $(ARGON2_DIR)/include
INC_DIR += $(ARGON2_DIR)/src/lib/argon2/src
INC_DIR += $(ARGON2_DIR)/src/lib/argon2/src/blake2

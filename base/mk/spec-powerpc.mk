#
# PowerPC-specific Genode headers
#
REP_INC_DIR += include/powerpc

SPECS += 32bit

CC_OPT += -mregnames

LD_OPT += --relax

include $(call select_from_repositories,mk/spec-32bit.mk)

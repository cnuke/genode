#
# Specifics for Linux on PowerPC
#
SPECS += linux powerpc

REP_INC_DIR += src/platform/powerpc

#
# We need to manually add the default linker script on the command line in case
# of standard library use. Otherwise, we were not able to extend it by the
# context area section.
#
ifeq ($(USE_HOST_LD_SCRIPT),yes)
LD_SCRIPT_STATIC = ldscripts/elf32ppclinux.xc
endif

#
# Include less-specific configuration
#
include $(call select_from_repositories,mk/spec-powerpc.mk)
include $(call select_from_repositories,mk/spec-linux.mk)

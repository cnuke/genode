include $(call select_from_repositories,lib/import/import-libc.mk)

JEMALLOC_DIR = $(call select_from_ports,jemalloc)/src/lib/jemalloc

INC_DIR += $(REP_DIR)/include/jemalloc

INC_DIR += $(REP_DIR)/src/lib/jemalloc/include
INC_DIR += $(JEMALLOC_DIR)/include

JEMALLOC_SRC_C = \
        jemalloc.c \
        arena.c \
        background_thread.c \
        base.c \
        bin.c \
        bitmap.c \
        ckh.c \
        ctl.c \
        div.c \
        extent.c \
        extent_dss.c \
        extent_mmap.c \
        hash.c \
        hook.c \
        large.c \
        log.c \
        malloc_io.c \
        mutex.c \
        mutex_pool.c \
        nstime.c \
        pages.c \
        prng.c \
        prof.c \
        rtree.c \
        safety_check.c \
        stats.c \
        sc.c \
        sz.c \
        tcache.c \
        test_hooks.c \
        ticker.c \
        tsd.c \
        witness.c

vpath %.c $(JEMALLOC_DIR)/src

#
# We need to place all global variables of jemalloc in one consecutive
# range of the libc's data segment in order to reset them during 'execve'.
#
# To let the linker place all jemalloc variables consecutively, we assign
# their symbols to a dedicated section named '.data.jemalloc' using objcopy
# as a post-processing step.
#
# To make the libc aware of the start and end of the range, we generate
# two marker symbols '_jemalloc_start' and '_jemalloc_end', each in a
# distinct object file ('0_start.o' and 'z_end.o'). Genode's build system
# alphabetically sorts object files passed to the linker. Hence, the file
# prefixes ('0_' and 'z_') are choosen such that the start marker appears
# first and the end marker appears last.
#

UNFILTERED_OBJECTS := 0_start.o $(JEMALLOC_SRC_C:.c=.o) z_end.o

SRC_O := $(addprefix filtered-,$(UNFILTERED_OBJECTS))

# generate assembly code containing a data symbol with the given name
gen_symbol_s = $(VERBOSE)echo ".$1; .global _jemalloc_$1_$2; _jemalloc_$1_$2:"

0_start.s:
	$(call gen_symbol_s,data,start) > $@
	$(call gen_symbol_s,bss,start) >> $@
z_end.s:
	$(call gen_symbol_s,data,end) > $@
	$(call gen_symbol_s,bss,end) >> $@

FILTER_ARGS := --rename-section  .bss=.bss.jemalloc \
               --rename-section .data=.data.jemalloc

filtered-%.o: %.o
	$(VERBOSE)$(OBJCOPY) $(FILTER_ARGS) $< $@

# keep intermediate objects to avoid 'rm ...' messages printed by make
.PRECIOUS: $(UNFILTERED_OBJECTS)

CC_CXX_WARN_STRICT =

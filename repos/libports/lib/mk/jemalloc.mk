include $(call select_from_repositories,lib/import/import-libc.mk)

JEMALLOC_DIR = $(call select_from_ports,jemalloc)/src/lib/jemalloc

INC_DIR += $(REP_DIR)/include/jemalloc

INC_DIR += $(REP_DIR)/src/lib/jemalloc/include
INC_DIR += $(JEMALLOC_DIR)/include

SRC_C = \
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

SHARED_LIB = yes

CC_CXX_WARN_STRICT =

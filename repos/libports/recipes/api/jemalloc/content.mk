content: include lib/symbols/jemalloc LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jemalloc)

include:
	mkdir -p $@
#	cp -r $(PORT_DIR)/include/jemalloc/* $@/
	cp -r $(REP_DIR)/include/jemalloc $@/

lib/symbols/jemalloc:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/jemalloc/COPYING $@

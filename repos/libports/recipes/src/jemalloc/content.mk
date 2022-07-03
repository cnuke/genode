content: src/lib/jemalloc lib/mk/jemalloc.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jemalloc)

src/lib/jemalloc:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/jemalloc $@
	cp -r $(REP_DIR)/src/lib/jemalloc/* $@
	echo "LIBS = jemalloc" > $@/target.mk

lib/mk/%.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/jemalloc/COPYING $@

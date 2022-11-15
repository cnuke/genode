MIRROR_FROM_REP_DIR := src/app/mixer_gui_qt

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_OS := include/mixer

MIRROR_FROM_LIBPORTS := src/lib/libc/internal/thread_create.h \
                        src/lib/libc/internal/types.h

content: $(MIRROR_FROM_OS) $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

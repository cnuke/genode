include $(GENODE_DIR)/repos/base/recipes/content.inc

content: vbox_files.tar

FILES := ubuntu-22.04-raw \
		 windows-10-raw
vbox_files.tar:
	$(TAR) -cf $@ -C $(REP_DIR)/recipes/raw/vbox_files $(FILES)

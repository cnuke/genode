content: init.config assets.tar

FILE_PATH := $(REP_DIR)/recipes/raw/download_debian

init.config:
	cp $(FILE_PATH)/$@ $@

assets.tar:
	tar --mtime='2022-04-29 00:00Z' -cf $@ -C $(FILE_PATH) ./machine.vbox
	tar --mtime='2022-04-29 00:00Z' -rf $@ -C $(FILE_PATH) ./machine.vbox6
	tar --mtime='2022-04-29 00:00Z' -rf $@ -C $(FILE_PATH) ./machine.vdi

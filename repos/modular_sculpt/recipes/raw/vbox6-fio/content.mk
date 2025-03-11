content: machine.vbox6 disk.vmdk

machine.vbox6:
	cp $(REP_DIR)/recipes/raw/vbox6-fio/$@ $@

disk.vmdk:
	cp $(REP_DIR)/recipes/raw/vbox6-fio/$@ $@

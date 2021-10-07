content: machine.vbox6 usb_devices

machine.vbox6:
	cp $(REP_DIR)/recipes/raw/vbox6/$@ $@

usb_devices:
	cp $(REP_DIR)/recipes/raw/vbox6-block/$@ $@

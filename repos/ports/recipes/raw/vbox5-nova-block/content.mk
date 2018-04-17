content: init.config usb_devices

init.config:
	cp $(REP_DIR)/recipes/raw/vbox5-nova-block/$@ $@

usb_devices:
	cp $(REP_DIR)/recipes/raw/vbox5-nova-block/$@ $@

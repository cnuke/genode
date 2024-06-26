SRC_DIR = src/app/msr_gui

content: $(SRC_DIR) LICENSE

$(SRC_DIR):
	mkdir -p $@
	cp -rL $(REP_DIR)/$@/* $@/

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

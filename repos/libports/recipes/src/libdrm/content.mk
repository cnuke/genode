MIRROR_FROM_REP_DIR := lib/mk/libdrm.mk \
                       lib/mk/libdrm.inc \
                       lib/mk/spec/arm_v8/libdrm.mk \
                       src/lib/libdrm/include \
                       src/lib/libdrm/dummies.c \
                       src/lib/libdrm/ioctl_dummy.cc \
                       src/lib/libdrm/ioctl_etnaviv.cc \

content: $(MIRROR_FROM_REP_DIR) src/lib/libdrm/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libdrm)
MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/lib/libdrm -type f)

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)


src/lib/libdrm/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS := libdrm" > $@

content: LICENSE

LICENSE:
	echo "MIT, see header files" > $@

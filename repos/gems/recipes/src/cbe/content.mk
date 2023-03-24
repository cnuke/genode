MIRROR_FROM_REP_DIR := \
	src/lib/cbe \
	lib/import/import-cbe.mk \
	lib/mk/cbe.mk \
	lib/mk/spec/x86_64/vfs_cbe.mk \
	lib/mk/spec/x86_64/vfs_cbe_crypto_aes_cbc.mk \
	lib/mk/spec/x86_64/vfs_cbe_crypto_memcopy.mk \
	lib/mk/spec/x86_64/vfs_cbe_trust_anchor.mk \
	src/lib/vfs/cbe \
	src/lib/vfs/cbe_crypto/vfs.cc \
	src/lib/vfs/cbe_crypto/aes_cbc \
	src/lib/vfs/cbe_crypto/memcopy \
	src/lib/vfs/cbe_trust_anchor \
	src/app/cbe_init \
	src/app/cbe_init_trust_anchor \
	src/app/cbe_tester \
	include/cbe

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

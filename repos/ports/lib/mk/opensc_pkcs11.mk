SRC_C += pkcs11/pkcs11-global.c
SRC_C += pkcs11/pkcs11-session.c
SRC_C += pkcs11/pkcs11-object.c
SRC_C += pkcs11/misc.c
SRC_C += pkcs11/slot.c
SRC_C += pkcs11/mechanism.c
SRC_C += pkcs11/openssl.c
SRC_C += pkcs11/framework-pkcs15.c
SRC_C += pkcs11/framework-pkcs15init.c
SRC_C += pkcs11/debug.c
SRC_C += pkcs11/pkcs11-display.c
SRC_C += common/libscdl.c
SRC_C += ui/notify.c
SRC_C += libopensc/log.c
SRC_C += libopensc/ctx.c

#.libs/libopensc.a
#libopensc_la-sc.o
#libopensc_la-ctx.o
#libopensc_la-log.o
#libopensc_la-errors.o
#libopensc_la-asn1.o
#libopensc_la-base64.o
#libopensc_la-sec.o
#libopensc_la-card.o
#libopensc_la-iso7816.o
#libopensc_la-dir.o
#libopensc_la-ef-atr.o
#libopensc_la-ef-gdo.o
#libopensc_la-padding.o
#libopensc_la-apdu.o
#libopensc_la-simpletlv.o
#libopensc_la-gp.o
#libopensc_la-pkcs15.o
#libopensc_la-pkcs15-cert.o
#libopensc_la-pkcs15-data.o
#libopensc_la-pkcs15-pin.o
#libopensc_la-pkcs15-prkey.o
#libopensc_la-pkcs15-pubkey.o
#libopensc_la-pkcs15-skey.o
#libopensc_la-pkcs15-sec.o
#libopensc_la-pkcs15-algo.o
#libopensc_la-pkcs15-cache.o
#libopensc_la-pkcs15-syn.o
#libopensc_la-pkcs15-emulator-filter.o
#libopensc_la-muscle.o
#libopensc_la-muscle-filesystem.o
#libopensc_la-ctbcs.o
#libopensc_la-reader-ctapi.o
#libopensc_la-reader-pcsc.o
#libopensc_la-reader-openct.o
#libopensc_la-reader-tr03119.o
#libopensc_la-card-setcos.o
#libopensc_la-card-flex.o
#libopensc_la-card-gpk.o
#libopensc_la-card-cardos.o
#libopensc_la-card-tcos.o
#libopensc_la-card-default.o
#libopensc_la-card-mcrd.o
#libopensc_la-card-starcos.o
#libopensc_la-card-openpgp.o
#libopensc_la-card-oberthur.o
#libopensc_la-card-belpic.o
#libopensc_la-card-atrust-acos.o
#libopensc_la-card-entersafe.o
#libopensc_la-card-epass2003.o
#libopensc_la-card-coolkey.o
#libopensc_la-card-incrypto34.o
#libopensc_la-card-piv.o
#libopensc_la-card-cac-common.o
#libopensc_la-card-cac.o
#libopensc_la-card-cac1.o
#libopensc_la-card-muscle.o
#libopensc_la-card-asepcos.o
#libopensc_la-card-akis.o
#libopensc_la-card-gemsafeV1.o
#libopensc_la-card-rutoken.o
#libopensc_la-card-rtecp.o
#libopensc_la-card-westcos.o
#libopensc_la-card-myeid.o
#libopensc_la-card-itacns.o
#libopensc_la-card-authentic.o
#libopensc_la-card-iasecc.o
#libopensc_la-iasecc-sdo.o
#libopensc_la-iasecc-sm.o
#libopensc_la-card-sc-hsm.o
#libopensc_la-card-dnie.o
#libopensc_la-cwa14890.o
#libopensc_la-cwa-dnie.o
#libopensc_la-card-isoApplet.o
#libopensc_la-card-masktech.o
#libopensc_la-card-gids.o
#libopensc_la-card-jpki.o
#libopensc_la-card-npa.o
#libopensc_la-card-esteid2018.o
#libopensc_la-card-idprime.o
#libopensc_la-card-edo.o
#libopensc_la-card-nqApplet.o
#libopensc_la-pkcs15-openpgp.o
#libopensc_la-pkcs15-starcert.o
#libopensc_la-pkcs15-cardos.o
#libopensc_la-pkcs15-tcos.o
#libopensc_la-pkcs15-esteid.o
#libopensc_la-pkcs15-gemsafeGPK.o
#libopensc_la-pkcs15-actalis.o
#libopensc_la-pkcs15-atrust-acos.o
#libopensc_la-pkcs15-tccardos.o
#libopensc_la-pkcs15-piv.o
#libopensc_la-pkcs15-cac.o
#libopensc_la-pkcs15-esinit.o
#libopensc_la-pkcs15-westcos.o
#libopensc_la-pkcs15-pteid.o
#libopensc_la-pkcs15-oberthur.o
#libopensc_la-pkcs15-itacns.o
#libopensc_la-pkcs15-gemsafeV1.o
#libopensc_la-pkcs15-sc-hsm.o
#libopensc_la-pkcs15-coolkey.o
#libopensc_la-pkcs15-din-66291.o
#libopensc_la-pkcs15-idprime.o
#libopensc_la-pkcs15-nqApplet.o
#libopensc_la-pkcs15-dnie.o
#libopensc_la-pkcs15-gids.o
#libopensc_la-pkcs15-iasecc.o
#libopensc_la-pkcs15-jpki.o
#libopensc_la-pkcs15-esteid2018.o
#libopensc_la-compression.o
#libopensc_la-sm.o
#libopensc_la-aux-data.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-asepcos.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-authentic.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-cardos.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-cflex.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-entersafe.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-epass2003.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-gids.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-gpk.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-iasecc.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-incrypto34.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-isoApplet.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-lib.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-muscle.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-myeid.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-oberthur-awp.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-oberthur.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-openpgp.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-rtecp.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-rutoken.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-sc-hsm.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-setcos.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-starcos.o
#.libs/libopensc.lax/libpkcs15init.a/pkcs15-westcos.o
#.libs/libopensc.lax/libpkcs15init.a/profile.o
#.libs/libopensc.lax/libscconf.a/parse.o
#.libs/libopensc.lax/libscconf.a/scconf.o
#.libs/libopensc.lax/libscconf.a/sclex.o
#.libs/libopensc.lax/libscconf.a/write.o
#.libs/libopensc.lax/libscdl.a/libscdl.o
#.libs/libopensc.lax/libnotify.a/notify.o
#.libs/libopensc.lax/libstrings.a/strings.o
#.libs/libopensc.lax/libsmeac.a/libsmeac_la-sm-eac.o
#.libs/libopensc.lax/libsmeac.a/sm-iso.o
#.libs/libopensc.lax/libcompat.a/compat___iob_func.o
#.libs/libopensc.lax/libcompat.a/compat_dummy.o
#.libs/libopensc.lax/libcompat.a/compat_getopt.o
#.libs/libopensc.lax/libcompat.a/compat_getpass.o
#.libs/libopensc.lax/libcompat.a/compat_report_rangecheckfailure.o
#.libs/libopensc.lax/libcompat.a/compat_strlcat.o
#.libs/libopensc.lax/libcompat.a/compat_strlcpy.o
#.libs/libopensc.lax/libcompat.a/compat_strnlen.o
#.libs/libopensc.lax/libcompat.a/simclist.o

PORT_DIR := $(call select_from_ports,opensc)/src/opensc

CC_OPT += -shared -fPIC -DPIC -lcrypto -ldl -g -DHAVE_CONFIG_H
CC_OPT += -DOPENSC_CONF_PATH=\"$(PORT_DIR)/etc/opensc.conf\"

LIBS += stdcxx libc libssl

INC_DIR += $(REP_DIR)/src/lib/opensc_pkcs11
INC_DIR += $(PORT_DIR)/src


vpath % $(PORT_DIR)/src

SHARED_LIB := yes

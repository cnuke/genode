TARGET  := pc_wlan_drv
SRC_CC  := main.cc wpa.cc
LIBS    := base wlan iwl_firmware
LIBS    += libc
LIBS    += wpa_supplicant
LIBS    += libcrypto libssl wpa_driver_nl80211

INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT :=

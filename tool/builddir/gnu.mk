#
# This file is use to specify the name of the various GNU tools
#

#
# Determine the OS if BUILD_OS is not already set
#
export BUILD_OS ?= $(shell uname)

#
# FreeBSD
#
ifeq ($(BUILD_OS), FreeBSD)
TAC        = gtac
GNU_FIND   = gfind
GNU_SED    = gsed
GNU_TAR    = gtar
GNU_PATCH  = gpatch
GNU_XARGS  = gxargs
endif

#
# If a GNU system is used use the normal names
#
TAC        ?= tac
GNU_FIND   ?= find
GNU_SED    ?= sed
GNU_TAR    ?= tar
GNU_PATCH  ?= patch
GNU_XARGS  ?= xargs


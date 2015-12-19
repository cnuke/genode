TARGET := test-libssh
LIBS   := libc libcrypto libssh posix
SRC_C  := authentication.c \
          connect_ssh.c \
          knownhosts.c \
          main.c

CC_CXX_WARN_STRICT =

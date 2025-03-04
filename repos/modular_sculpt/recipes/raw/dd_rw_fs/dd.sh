#!/bin/sh

#set -x

if [ ! -f "$HOME/src.img" ]; then
	dd if=/dev/zero of=$HOME/src.img bs=1M count=1024 oflag=direct iflag=direct conv=fsync
fi

sudo -p 'open sudo session for drop_caches: ' /bin/true

for block_size in 512 2K 4K 8K 32K 128K 512K 2M 8M; do
	echo 1 |sudo tee /proc/sys/vm/drop_caches > /dev/null
	dd if=$HOME/src.img of=$HOME/dst.img bs=$block_size oflag=direct iflag=direct conv=fsync status=progress
done

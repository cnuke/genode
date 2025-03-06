#!/bin/sh

#
# Quick and dirty shell-script that tests various block-size with
# dd(1) in a loop where for each run the VM gets restarted.
#

STATE_FILE="/rw/state"
LOG_FILE="/rw/log"

SRC_FILE=/mnt/src.img

if [ ! -f "$STATE_FILE" ]; then
	echo initialize > $STATE_FILE
fi

STATE=$(head -1 $STATE_FILE)

execute_dd() {
	src_file="$1"
	dst_file="$2"
	dd_bs="$3"
	cmd="dd if=$src_file of=$dst_file bs=$dd_bs oflag=direct iflag=direct conv=fsync"
	echo "START $dd_bs"|tee -a $LOG_FILE
	echo "$cmd"|tee -a $LOG_FILE
	$cmd 2>&1|tee -a $LOG_FILE
	echo "END $dd_bs"|tee -a $LOG_FILE
}

read_block_size() {
	NEW_BS=$(head -1 "$1")
	sed -i 1d "$1"
}

case "$STATE" in
initialize)
	mkfs.ext2 /dev/sda  || exit 1
	mount /dev/sda /mnt || exit 3

	dd if=/dev/zero of=$SRC_FILE bs=1M count=1024

	cat << EOF > /rw/block_sizes
512
2048
4096
8K
32K
128K
512K
2M
8M
EOF
	echo "block_size" > $STATE_FILE
	reboot
	break ;;
block_size)
	mount /dev/sda /mnt || exit 3

	read_block_size "/rw/block_sizes"
	if [ "$NEW_BS" != "" ]; then
		execute_dd "$SRC_FILE" "/mnt/dst.img" "$NEW_BS"
		reboot
	else
		cat /rw/log
		echo "finished dd tests"
	fi
	break ;;
esac

exit 0

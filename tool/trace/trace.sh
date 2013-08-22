#!/bin/sh

if [ $# -lt 1 ]; then
	echo usage: $(basename $0) '<file.out>'
	exit 1
fi

FORMAT="png"
TRACE_FILE=$1
TRACE_GV_FILE=${TRACE_FILE%%.*}.gv
TRACE_IMG_FILE=${TRACE_FILE%%.*}.$FORMAT

sed -e '/^TRACE .*$/d' -e 's/^TRACE: //' $TRACE_FILE | sort -nk 1 \
	| awk -F'\\[|\\]' -f ./trace.gawk > $TRACE_GV_FILE

dot -T$FORMAT $TRACE_GV_FILE -o $TRACE_IMG_FILE

exit 0

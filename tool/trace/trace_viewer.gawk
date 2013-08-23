#!/usr/bin/gawk -f

function conv_ts(t)
{
	# hex to dec
	return sprintf("%d", t)
}

function parse_label(l)
{
	var[1] = ""
	var[2] = ""
	var[3] = ""

	split(l,var,"->")
	if (var[3] != "") {
		return var[1]"->" var[2]","var[3]
	} else
		return var[1]"->" var[2]",mainthread"
}

BEGIN {
	printf("[\n")
}

{
	ts=conv_ts($1)
	tmp=parse_label($2)
	split($3,msg," ")
	split(tmp,ti,",")
	type=msg[1]
	switch (msg[1]) {
	case "RPC_CALL":
		type1="B"
		break
	case "RPC_RETURNED":
		type1="E"
		break
	case "RPC_DISPATCH":
		typ1="B"
		break
	case "RPC_REPLY":
		type="E"
		break
	default:
		type1="I"
	}

	if (NR > 1)
		printf(",\n")
	printf("{\"cat\": \"%s\", \"pid\": \"%s\", \"tid\": \"%s\", \"ts\": %s, \"ph\": \"%s\",\n",
		type, ti[1], ti[2], ts, type1)
	printf("\t\"name\": \"%s\", \"args\":{}}", msg[2]);
}

END {
	printf("\n]")
}

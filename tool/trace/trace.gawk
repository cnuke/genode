BEGIN {
	count=0
}

{
	times[count]=$1
	event[count]=$3
	thread[count]=$2

	threads[$2]++

	count++
}

END {
	printf("digraph tbuf {\n")
	printf("{\nnode [shape=plaintext];\n")
	# timestamps
	printf("/* timestamps */\n")
	for (i = count - 1; i >= 1; --i)
		printf("\"%s\" ->\n", times[i])
	printf("\"%s\";\n", times[i++])
	printf("\n")
	# threads
	printf("/* threads */\n")
	for (i in threads)
		printf("\"%s\";\n", i)
	printf("};\n")

	printf("/* event */\n")
	printf("node [shape=box];\n")
	for (i = count - 1; i >= 0; --i) {
		printf("{ rank = same; \"%s\"; \"%s_%d\"; }\n",
		       times[i], event[i], i)
	}

	for (i in threads) {
		printf("\"%s\"\n", i)
		for (j = count - 1; j > 0; --j) {
			if (thread[j] == i)
				printf("->\t\"%s_%d\"\n", event[j], j)
		}
		printf("[arrowhead=none style=dotted];\n")
	}

	printf("}")
}

/*
 * \brief  Test rsyslog by periodically writing into a log file
 * \author Josef Soentgen
 * \date   2026-04-29
 *
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char buffer[128];

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("usage: %s <log-file> <interval>\n", argv[0]);
		return 1;
	}

	char const * const log_file = argv[1];
	int const interval_msec = atoi(argv[2]);

	int const fd = open(log_file, O_WRONLY);
	if (fd == -1) {
		perror("open");
		return 2;
	}

	printf("Generate new log entry every %d ms\n", interval_msec);
	int i = 0;
	do {
		time_t const curr_time = time(NULL);
		char *time_string = ctime(&curr_time);
		/* remove \n */
		if (time_string != NULL) {
			size_t const length = strlen(time_string);
			time_string[length-1] = '\0';
		}

		/* ignore any truncation, overflow and erroneous writes */
		int const len = snprintf(buffer, sizeof(buffer), "prog='%s' ts='%s' cnt=%d\n",
		                         argv[0], time_string, ++i);
		buffer[len] = '\0';

		(void)write(fd, buffer, len);

		usleep(interval_msec * 1000u);
	} while (1);

	close(fd);
	return 0;
}

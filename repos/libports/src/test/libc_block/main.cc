/*
 * \brief  libc_block test
 * \author Josef Soentgen
 * \date   2013-11-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static char buf[16384];
static char str[] = "deadbeef";

int main(int argc, char *argv[])
{
	int fd;
	ssize_t n;
	off_t offset;

	printf("--- start test ---\n");

	fd = open("/dev/blkdev", O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	offset = lseek(fd, 256, SEEK_SET);
	printf("offset: %lld read bytes: %u\n", (long long)offset, 512);
	n = read(fd, buf, 512);
	printf("bytes: %zd\n", n);

	{
		off_t  const seek_offset = 501;
		size_t const length      = 7;

		offset = lseek(fd, seek_offset, SEEK_SET);
		printf("offset: %lld write bytes: %zu\n", (long long)offset, length);
		n = write(fd, buf, length);
		printf("write %s: n: %zd (%zu) \n", n == length ? "success" : "failed", n, length);
	}

	offset = lseek(fd, 8193, SEEK_SET);
	printf("offset: %lld write bytes: %zu\n", (long long)offset, sizeof(str));
	n = write(fd, str, sizeof (str));
	if (n != sizeof(str))
		printf("error write mismatch: %zd != %zu\n", n, sizeof(str));

	offset = lseek(fd, 8193, SEEK_SET);
	printf("offset: %lld read bytes: %zu\n", (long long)offset, sizeof(str));
	n = read(fd, buf, sizeof (str));
	printf("bytes: %zd\n", n);
	for (size_t i = 0; i < sizeof (str); i++)
		printf("%c ", buf[i]);
	printf("\n");

	offset = lseek(fd, 16384, SEEK_SET);
	printf("offset: %lld write bytes: %zu\n", (long long)offset, sizeof(str));
	n = write(fd, buf, sizeof (buf));
	if (n != sizeof (buf))
		printf("error write mismatch: %zd != %zu\n", n, sizeof (buf));

	offset = lseek(fd, 4060, SEEK_SET);
	printf("offset: %lld write bytes: %zu\n", (long long)offset, sizeof(buf)/2);
	n = write(fd, buf, sizeof (buf) / 2);
	if (n != sizeof (buf)/2)
		printf("error write mismatch: %zd != %zu\n", n, sizeof (buf)/2);

	offset = lseek(fd, 2342, SEEK_SET);
	printf("offset: %lld write bytes: %zu\n", (long long)offset, sizeof(buf));
	n = read(fd, buf, sizeof (buf));
	if (n != sizeof (buf))
		printf("error read mismatch: %zd != %zu\n", n, sizeof (buf));


	for (unsigned i = 0; i < 256; i++) {
		memset(buf, i, 512);
		offset = lseek(fd, i * 512, SEEK_SET);
		n = write(fd, buf, 512);
		if (n != 512)
			printf("error write mismatch: %zd != %u\n", n, 512);
	}

	for (unsigned i = 0; i < 256; i++) {
		memset(buf, i, 512);

		char block_buf[512] { };

		offset = lseek(fd, i * 512, SEEK_SET);
		n = read(fd, block_buf, 512);
		if (n != 512)
			printf("error read mismatch: %zd != %u\n", n, 512);

		for (unsigned j = 0; j < 512; j++) {
			if (block_buf[j] != buf[j])
				printf("error read content mismatch: block[%u]: %u != %u\n", i, block_buf[j], buf[j]);
		}
	}

	offset = lseek(fd, 1020, SEEK_SET);
	n = read(fd, buf, 8);
	if (n != 8)
		printf("error read mismatch: %zd != %u\n", n, 8);
	for (unsigned i = 0; i < 8; i++) {
		printf("%u ", buf[i]);
	}
	printf("\n");

	offset = lseek(fd, 508, SEEK_SET);
	unsigned char arr[8] = { 0xde, 0xad, 0xbe, 0xef, 0xc0, 0xff, 0xee, 0xa5 };
	n = write(fd, arr, sizeof(arr));
	if (n != 8)
		printf("error write mismatch: %zd != %zu\n", n, sizeof(arr));

	offset = lseek(fd, 508, SEEK_SET);
	n = read(fd, buf, 8);
	if (n != 8)
		printf("error read mismatch: %zd != %u\n", n, 8);
	for (unsigned i = 0; i < 8; i++) {
		printf("%x ", buf[i] & 0xffu);
	}
	printf("\n");

	close(fd);

	printf("--- test finished ---\n");

	return 0;
}

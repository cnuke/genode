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

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* libc includes */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


static char buf1[4u << 20];
static char buf2[4u << 20];
static_assert(sizeof(buf1) == sizeof(buf2));

static bool _verbose = true;


struct Test
{
	using Descr = Genode::String<64>;

	Descr _descr;
	int   _fd;
	bool  _success;

	Test(Descr const &descr, char const *blkdev)
	: _descr { descr }, _fd { -1 }, _success { false }
	{
		if (_verbose)
			Genode::log("Start ", _descr);

		_fd = open("/dev/blkdev", O_RDWR);
		if (_fd == -1)
			perror("open");
	}

	void with_fd(auto const &fn, auto const &invalid_fd)
	{
		if (_fd != -1) _success = fn(_fd);
		else           invalid_fd();
	}

	virtual ~Test()
	{
		if (_fd != -1)
			close(_fd);

		if (_verbose)
			Genode::log("Finished ", _descr, " result: ",
		                _success ? "success" : "failed");
	}

	void with_success(auto const &fn) {
		if (_success) fn(); }

	void with_failure(auto const &fn) {
		if (!_success) fn(); }
};


template <off_t OFFSET, size_t LENGTH>
struct Read : Test
{
	Read(char const *blkdev, char *dst)
	: Test(Test::Descr("read@", OFFSET, "-", LENGTH), blkdev)
	{
		with_fd([&] (int const fd) {
			ssize_t const n = ::pread(fd, dst, LENGTH, OFFSET);
			return LENGTH == n;
		}, [&] { });
	}
};


template <off_t OFFSET, size_t LENGTH>
struct Write : Test
{
	Write(char const *blkdev, char const *src)
	: Test(Test::Descr("write@", OFFSET, "-", LENGTH), blkdev)
	{
		with_fd([&] (int const fd) {
			ssize_t const n = ::pwrite(fd, src, LENGTH, OFFSET);
			return LENGTH == n;
		}, [&] { });
	}
};


struct Multiple_write : Test
{
	Multiple_write(char const *blkdev)
	: Test(Test::Descr("multiple-write"), blkdev) { }

	bool write(off_t const offset, char const *src, size_t const length)
	{
		bool result = false;
		with_fd([&] (int const fd) {

			ssize_t const n = ::pwrite(fd, src, length, offset);
			result = n == (ssize_t)length;
			return result;
		}, [&] { });

		return result;
	}
};


struct Multiple_read : Test
{
	Multiple_read(char const *blkdev)
	: Test(Test::Descr("multiple-read"), blkdev) { }

	bool read(off_t const offset, char *dst, size_t const length)
	{
		bool result = false;
		with_fd([&] (int const fd) {
			ssize_t const n = ::pread(fd, dst, length, offset);
			result = n == (ssize_t)length;
			return result;
		}, [&] { });

		return result;
	}
};


struct Sequential : Test
{
	enum class Type { READ, WRITE };

	struct Operation
	{
		Type      const type;
		size_t    const chunk_size;
		unsigned  const chunk_count;
		void    * const buffer;
	};

	Operation _op;

	ssize_t _total;

	void _read()
	{
		with_fd([&] (int const fd) {

			if (_op.chunk_count) {
				unsigned i = 0;
				for ( ; i < _op.chunk_count; i++) {
					ssize_t const n = ::read(fd, _op.buffer, _op.chunk_size);
					if (n != (ssize_t)_op.chunk_size && n != 0)
						break;
				}
				return i == _op.chunk_count;
			}

			ssize_t n = 0;
			do {
				n = ::read(fd, _op.buffer, _op.chunk_size);
				if (n != (ssize_t)_op.chunk_size && n != 0)
					return false;
				_total += n;
			} while (n);

			return _total && n == 0;
		}, [&] { });
	}

	void _write()
	{
		with_fd([&] (int const fd) {

			if (_op.chunk_count) {
				unsigned i = 0;
				for ( ; i < _op.chunk_count; i++) {
					ssize_t const n = ::write(fd, _op.buffer, _op.chunk_size);
					if (n != (ssize_t)_op.chunk_size && n != 0)
						break;
				}
				return i == _op.chunk_count;
			}

			ssize_t n = 0;
			do {
				n = ::write(fd, _op.buffer, _op.chunk_size);
				if (n != (ssize_t)_op.chunk_size && errno != EINVAL)
					return false;

				if (n == -1 && errno == EINVAL)
					break;

				_total += n;
			} while (n);

			return _total != 0;
		}, [&] { });
	}

	Sequential(char const *blkdev, Operation const op)
	:
		Test   { Test::Descr("sequential-",
		         op.type == Type::READ ? "read" : "write"), blkdev },
		_op    { op },
		_total { 0 }
	{
		switch (_op.type) {
		case Type::READ:
			_read();
			break;
		case Type::WRITE:
			_write();
			break;
		}
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, _op.type == Type::READ ? "READ" : "WRITE", ": ",
		              "chunk_size: ", _op.chunk_size, " "
		              "chunk_count: ", _op.chunk_count, " "
		              "total: ", Genode::Num_bytes(_total));
	}
};


int main(int argc, char *argv[])
{
	printf("--- start testing ---\n");

	int exit_code = 0;

	static constexpr char const *blkdev = "/dev/blkdev";

	/*
	 * Query block-device information and bail out if it
	 * does not conform to the test requirments.
	 */
	{
		struct stat sb;
		if (stat(blkdev, &sb) == -1) {
			perror("stat");
			return -1;
		}

		off_t const min_size = off_t(16u << 20);
		if (sb.st_size < min_size) {
			Genode::error("blkdev needs to be at least ",
			              Genode::Num_bytes(min_size));
			return -1;
		}
	}

	/*
	 * Test cross block boundary unaligned full write.
	 */
	{
		size_t const block_size = 512;
		off_t  const offset     = block_size / 2 - 1;

		memset(buf1, 0xa5, block_size);
		memset(buf2, 0xff, block_size * 2);

		{
			using Unaligned_partial_write = Write<offset, block_size>;
			Unaligned_partial_write _(blkdev, buf1);
		}

		{
			using Unaligned_full_read = Read<0, block_size * 2>;
			Unaligned_full_read _(blkdev, buf2);
		}

		bool const equal = (0 == memcmp(buf1, (buf2 + offset), block_size));
		if (!equal) {
			Genode::error("unaligned-full-write failed");
			exit_code = -1;
		}
	}

	/*
	 * Test unaligned partial write and read.
	 */
	{
		char  const str[]  = "deadbeef";
		off_t const offset = 8191 - sizeof(str);

		{
			using Unaligned_partial_write = Write<offset, sizeof(str)>;
			Unaligned_partial_write _(blkdev, str);
		}

		{
			using Unaligned_partial_read = Read<offset, sizeof(str)>;
			Unaligned_partial_read _(blkdev, buf1);
		}

		bool const equal = (0 == memcmp(str, (buf1), sizeof(str)));
		if (!equal) {
			Genode::error("unaligned-partial-write-read failed");
			exit_code = -1;
		}
	}

	/*
	 * Test large write performed in multiple steps by the
	 * underlying Block stack (cf. maximum-transfer-size).
	 */
	{
		size_t const quarter_length = sizeof(buf1) / 4;
		for (size_t i = 0; i < quarter_length; i++)
			memset(buf1 + (i + quarter_length), 30 + i, quarter_length);

		{
			using Aligned_large_write = Write<0, sizeof(buf1)>;
			Aligned_large_write _(blkdev, buf1);
		}

		memset(buf2, 0, sizeof(buf2));

		{
			using Aligned_large_read = Read<0, sizeof(buf1)>;
			Aligned_large_read _(blkdev, buf2);
		}

		bool const equal = (0 == memcmp(buf1, buf2, sizeof(buf1)));
		if (!equal) {
			Genode::error("aligned-large-write failed");
			exit_code = -1;
		}
	}

	/*
	 * Test write and read pattern one block at a time.
	 */
	{
		size_t const block_size = 512;

		{
			Multiple_write _(blkdev);

			for (unsigned i = 0; i < 256; i++) {
				memset(buf1, i, block_size);
				if (!_.write(i * block_size, buf1, block_size)) break;
			}
		}

		bool equal = true;

		{
			Multiple_read _(blkdev);

			for (unsigned i = 0; i < 256; i++) {
				memset(buf1, i, block_size);

				if (!_.read(i * block_size, buf2, block_size))
					break;

				for (unsigned j = 0; j < block_size; j++)
					if (buf2[j] != buf1[j])
						printf("Error: content mismatch: offset %u: %u != %u\n",
						       i, buf2[j], buf1[j]);
			}
		}
		if (!equal) {
			Genode::error("write-read-pattern failed");
			exit_code = -1;
		}
	}

	/*
	 * Perform 'dd' like sequential tests where the whole block-device
	 * is first read (until EOF) and then written (until EINVAL).
	 */
	{
		Sequential::Type const seq_types[] = {
			Sequential::Type::READ, Sequential::Type::WRITE };
		for (auto seq_type : seq_types) {

			size_t const chunk_sizes[] = {
				512, 4096, sizeof(buf1)/2, sizeof(buf1) };

			for (auto chunk_size : chunk_sizes) {
				Sequential::Operation const op {
					.type        = seq_type,
					.chunk_size  = chunk_size,
					.chunk_count = 0,
					.buffer      = buf1
				};
				Sequential _(blkdev, op);
				Genode::log("sequential ", _);

				_.with_failure([&] { exit_code = -1; });
			}
		}
	}

	printf("--- testing finished ---\n");

	return exit_code;
}

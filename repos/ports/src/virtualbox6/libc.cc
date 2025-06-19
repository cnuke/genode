/*
 * \brief  VirtualBox libc runtime
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* libc includes */
#include <signal.h>
#include <sys/times.h>
#include <unistd.h>
#include <aio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>      /* memset */
#include <sys/mount.h>   /* statfs */
#include <sys/statvfs.h> /* fstatvfs */
#include <fcntl.h>       /* open */

/* local includes */
#include <stub_macros.h>

static bool const debug = true; /* required by stub_macros.h */


extern "C" {

int futimes(int fd, const struct timeval tv[2]) TRACE(0)
int lutimes(const char *filename, const struct timeval tv[2]) TRACE(0)
int lchown(const char *pathname, uid_t owner, gid_t group) TRACE(0)
int mlock(const void *addr, size_t len) TRACE(0)
int gethostbyname_r(const char *name,
                    struct hostent *ret, char *buf, size_t buflen,
                    struct hostent **result, int *h_errnop) STOP
int gethostbyname2_r(const char *name, int af,
                     struct hostent *ret, char *buf, size_t buflen,
                     struct hostent **result, int *h_errnop) STOP
int getprotobynumber_r(int proto,
                       struct protoent *result_buf, char *buf,
                       size_t buflen, struct protoent **result) STOP

#if 0
int aio_cancel(int fd, struct aiocb *aiocbp) STOP
int aio_fsync(int op, struct aiocb *aiocbp) STOP

struct Aio_job
{
	enum class State { INVALID, PENDING, COMPLETED };

	State state { State::INVALID };

	ssize_t result;
	int error;

	struct aiocb *aiocb;
};

static constexpr unsigned MAX_AIO_PER_PROC = 32u;
static Aio_job _aio_jobs[MAX_AIO_PER_PROC];

static bool _enough_aio_job_slots(unsigned nitems)
{
	unsigned avail_jobs_slots = 0;
	for (unsigned i = 0; i < MAX_AIO_PER_PROC; i++)
		avail_jobs_slots += _aio_jobs[i].state == Aio_job::State::INVALID;

	return avail_jobs_slots >= nitems;
}


static void _for_each_aio_job(Aio_job::State state, auto const &fn)
{
	for (unsigned i = 0; i < MAX_AIO_PER_PROC; i++)
		if (_aio_jobs[i].state == state)
			fn(_aio_jobs[i]);
}


static bool _queue(struct aiocb *cb)
{
	bool queued = false;
	_for_each_aio_job(Aio_job::State::INVALID, [&] (Aio_job &job) {
		if (queued)
			return;

		job.aiocb = cb;
		job.state = Aio_job::State::PENDING;
		queued = true;
	});

	return queued;
}


ssize_t aio_return(struct aiocb *aiocbp)
{
	ssize_t result = -1;
	int error = 0;
	_for_each_aio_job(Aio_job::State::COMPLETED, [&] (Aio_job &job) {
		if (job.aiocb != aiocbp)
			return;
		error = job.error;
		result = job.result;

		job.state = Aio_job::State::INVALID;
	});

	if (error) {
		errno = error;
		return -1;
	}

	return result;
}


int aio_error(const struct aiocb *aiocbp)
{
	int error = 0;
	bool found = false;
	_for_each_aio_job(Aio_job::State::COMPLETED, [&] (Aio_job &job) {
		if (job.aiocb != aiocbp)
			return;

		found = true;

		error = job.error;
	});

	if (!found) {
		errno = EINVAL;
		return -1;
	}

	if (error != 0) {
		errno = error;
		return -1;
	}

	return 0;
}


int aio_suspend(const struct aiocb * const aiocb_list[],
                int nitems, const struct timespec *timeout)
{
	(void)timeout; // ignore for now

	// Genode::log("vbox", __func__, ": nent: ", nitems);
	for (int i = 0; i < nitems; i++) {
		if (aiocb_list[i] == NULL)
			continue;

		const struct aiocb *cb = aiocb_list[i];

		_for_each_aio_job(Aio_job::State::PENDING, [&] (Aio_job &job) {
			if (job.aiocb != cb)
				return;

			job.error = 0;

			switch (cb->aio_lio_opcode) {
			case LIO_READ:
			{
				ssize_t const bytes_read =
					pread(cb->aio_fildes, (void*)cb->aio_buf, cb->aio_nbytes, cb->aio_offset);
				if (bytes_read != (ssize_t)cb->aio_nbytes)
					Genode::error("Read mismatch: ", bytes_read, " != ", cb->aio_nbytes);
				// else
					//  READ_OK: libc_fd: 8 num_bytes: 2048 out_count: 2048
					// Genode::log("READ_OK: libc_fd: ", cb->aio_fildes, " num_bytes: ",  cb->aio_nbytes, " out_count: ", bytes_read);
				job.error = bytes_read < 0 ? errno : 0;
				job.result = bytes_read;
				break;
			}
			case LIO_WRITE:
			{
				ssize_t const bytes_written =
					pwrite(cb->aio_fildes, (void const *)cb->aio_buf, cb->aio_nbytes, cb->aio_offset);
				if (bytes_written != (ssize_t)cb->aio_nbytes)
					Genode::error("Write mismatch: ", bytes_written, " != ", cb->aio_nbytes);
				// else
					// Genode::log("WRITE_OK: libc_fd: ", cb->aio_fildes, " num_bytes: ", cb->aio_nbytes, " out_count: ", bytes_written);

				job.error = bytes_written < 0 ? errno : 0;
				job.result = bytes_written;
				break;
			}
			default:
				break;
			}

			job.state = Aio_job::State::COMPLETED;
		});
	}

	// Genode::log("vbox", __func__, ": nent: ", nitems, " done");
	return 0;
}

int lio_listio(int mode, struct aiocb *const aiocb_list[],
               int nitems, struct sigevent *sevp)
{
	// Genode::log("vbox", __func__, ": nent: ", nitems);
	if (nitems < 1)  {
		errno = EINVAL;
		return -1;
	}

	/* static allocation scheme, cannot set EAGAIN to reallocate */
	if (!_enough_aio_job_slots((unsigned)nitems)) {
		errno = EINVAL;
		return -1;
	}

	for (int i = 0; i < nitems; i++) {
		if (aiocb_list[i] == NULL)
			continue;

		struct aiocb *cb = aiocb_list[i];
		// XXX clean up already queued jobs
		if (!_queue(cb)) {
			errno = EIO;
			return -1;
		}
	}

	return 0;
}

#endif

} /* extern "C" */



/* Helper for VBOXSVC_LOG_DEFAULT hook in global_defs.h */
extern "C" char const * vboxsvc_log_default_string()
{
	char const *vbox_log_string = getenv("VBOX_LOG");

	return vbox_log_string ? vbox_log_string : "";
}


/* used by Shared Folders and RTFsQueryType() in media checking */
extern "C" int statfs(const char *path, struct statfs *buf)
{
	if (!buf) {
		errno = EFAULT;
		return -1;
	}

	int fd = open(path, 0);

	if (fd < 0)
		return fd;

	struct statvfs result;
	int res = fstatvfs(fd, &result);

	close(fd);

	if (res)
		return res;

	memset(buf, 0, sizeof(*buf));

	buf->f_bavail = result.f_bavail;
	buf->f_bfree  = result.f_bfree;
	buf->f_blocks = result.f_blocks;
	buf->f_ffree  = result.f_ffree;
	buf->f_files  = result.f_files;
	buf->f_bsize  = result.f_bsize;

	/* set file-system type to unknown to prevent application of any quirks */
	strcpy(buf->f_fstypename, "unknown");

	bool show_warning = !buf->f_bsize || !buf->f_blocks || !buf->f_bavail;

	if (!buf->f_bsize)
		buf->f_bsize = 4096;
	if (!buf->f_blocks)
		buf->f_blocks = 128 * 1024;
	if (!buf->f_bavail)
		buf->f_bavail = buf->f_blocks;

	if (show_warning)
		Genode::warning("statfs provides bogus values for '", path, "' (probably a shared folder)");

	return res;
}


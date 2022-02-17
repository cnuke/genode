/*
 * \brief  Linux socket call interface back end
 * \author Josef Soentgen
 * \date   2022-02-17
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* DDE Linux includes */
#include "lx_socket_call.h"

/* kernel includes */
#include <linux/socket.h>
#include <linux/net.h>
#include <net/sock.h>


struct task_struct *lx_socket_call_task;
void *lx_socket_call_task_args;
extern int run_lx_socket_call_task(void *p);


// FIXME find out how to properly initialize a net namespace for us to use
static struct net lx_socket_call_net;


void lx_user_init(void)
{
	int pid = kernel_thread(run_lx_socket_call_task,
	                        lx_socket_call_task_args,
	                        CLONE_FS | CLONE_FILES);
	lx_socket_call_task = find_task_by_pid_ns(pid, NULL);

	lx_socket_call_net.core.prot_inuse = kzalloc(4096, GFP_KERNEL);
	lx_socket_call_net.core.sock_inuse = kzalloc(sizeof (int), GFP_KERNEL);
}


int lx_sock_create_kern(int domain, int type, int protocol,
                        struct socket **res)
{
	return sock_create_kern(&lx_socket_call_net, domain, type, protocol, res);
}


int lx_sock_release(struct socket *sock)
{
	return sock->ops->release(sock);
}


int lx_sock_bind(struct socket *sock, void *sockaddr, int sockaddr_len)
{
	return sock->ops->bind(sock, sockaddr, sockaddr_len);
}


int lx_sock_getname(struct socket *sock, void *sockaddr, int peer)
{
	return sock->ops->getname(sock, sockaddr, peer);
}


int lx_sock_recvmsg(struct socket *sock, struct lx_msghdr *lx_msg,
                    int flags, int dontwait)
{
	struct msghdr *msg;
	struct iovec  *iov;
	size_t         iovlen;
	unsigned       iov_count;
	unsigned       i;

	int err = -1;

	iov_count = lx_msg->msg_iovcount;

	msg = kzalloc(sizeof (struct msghdr), GFP_KERNEL);
	if (!msg)
		goto err_msg;
	iov = kzalloc(sizeof (struct iovec) * iov_count, GFP_KERNEL);
	if (!iov)
		goto err_iov;

	for (i = 0; i < iov_count; i++) {
		iov[i].iov_base = lx_msg->msg_iov[i].iov_base;
		iov[i].iov_len  = lx_msg->msg_iov[i].iov_len;

		iovlen += lx_msg->msg_iov[i].iov_len;
	}

	msg->msg_name         = lx_msg->msg_name;
	msg->msg_namelen      = lx_msg->msg_namelen;
	msg->msg_iter.iov     = iov;
	msg->msg_iter.nr_segs = iov_count;
	msg->msg_iter.count   = iovlen;

	msg->msg_flags = flags;
	if (dontwait)
		msg->msg_flags |= MSG_DONTWAIT;

	err = sock->ops->recvmsg(sock, msg, iovlen, flags);

	kfree(iov);
err_iov:
	kfree(msg);
err_msg:
	return err;
}


int lx_sock_sendmsg(struct socket *sock, struct lx_msghdr* lx_msg,
                    int flags, int dontwait)
{
	struct msghdr *msg;
	struct iovec  *iov;
	size_t         iovlen;
	unsigned       iov_count;
	unsigned       i;

	int err = -1;

	iov_count = lx_msg->msg_iovcount;

	msg = kzalloc(sizeof (struct msghdr), GFP_KERNEL);
	if (!msg)
		goto err_msg;
	iov = kzalloc(sizeof (struct iovec) * iov_count, GFP_KERNEL);
	if (!iov)
		goto err_iov;

	for (i = 0; i < iov_count; i++) {
		iov[i].iov_base = lx_msg->msg_iov[i].iov_base;
		iov[i].iov_len  = lx_msg->msg_iov[i].iov_len;

		iovlen += lx_msg->msg_iov[i].iov_len;
	}

	msg->msg_name         = lx_msg->msg_name;
	msg->msg_namelen      = lx_msg->msg_namelen;
	msg->msg_iter.iov     = iov;
	msg->msg_iter.nr_segs = iov_count;
	msg->msg_iter.count   = iovlen;

	msg->msg_flags = flags;
	if (dontwait)
		msg->msg_flags |= MSG_DONTWAIT;

	err = sock->ops->sendmsg(sock, msg, iovlen);

	kfree(iov);
err_iov:
	kfree(msg);
err_msg:
	return err;
}


int lx_sock_setsockopt(struct socket *sock, int level, int optname,
                       void const *optval, unsigned optlen)
{
	sockptr_t soptval = (sockptr_t) { .user = optval };

	if (level == SOL_SOCKET)
		return sock_setsockopt(sock, level, optname,
		                       soptval, optlen);

	return sock->ops->setsockopt(sock, level, optname,
	                             soptval, optlen);
}


unsigned char const* lx_get_mac_addr()
{
	return NULL;
}


struct lx_poll_result lx_sock_poll(struct socket *sock)
{
	enum {
		POLLIN_SET  = (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR),
		POLLOUT_SET = (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR),
		POLLEX_SET  = (POLLPRI)
	};

	int const mask = sock->ops->poll(0, sock, 0);

	struct lx_poll_result result;

	if (mask & POLLIN_SET)
		result.in = true;
	if (mask & POLLOUT_SET)
		result.out = true;
	if (mask & POLLEX_SET)
		result.ex = true;

	return result;
}

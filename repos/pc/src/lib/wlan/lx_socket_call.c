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


extern struct net init_net;

static struct net_device *_wlan_device;

void open_wlan_device(void)
{
	struct net_device *dev;
	printk("%s:%d\n", __func__, __LINE__);

	for_each_netdev(&init_net, dev) {

		if (!dev_open(dev, 0) && !_wlan_device)
			_wlan_device = dev;

		if (!_wlan_device)
			break;
	}

	printk("%s:%d _wlan_device: %p\n", __func__, __LINE__, _wlan_device);
}

void lx_user_init(void)
{
	// open_wlan_device();

	int pid = kernel_thread(run_lx_socket_call_task,
	                        lx_socket_call_task_args,
	                        CLONE_FS | CLONE_FILES);
	lx_socket_call_task = find_task_by_pid_ns(pid, NULL);
}


int lx_sock_create_kern(int domain, int type, int protocol,
                        struct socket **res)
{
	// int const err = sock_create_kern(&lx_socket_call_net, domain, type,
	//                                  protocol, res);
	int const err = __sock_create(&init_net, domain, type, protocol, res, 1);
	if (err)
		return err;
	printk("%s:%d sock: %p\n", __func__, __LINE__, *res);

	init_waitqueue_head(&(*res)->wq.wait);
	return 0;
}


int lx_sock_release(struct socket *sock)
{
	printk("%s:%d sock: %p\n", __func__, __LINE__, sock);
	return sock->ops->release(sock);
}


int lx_sock_bind(struct socket *sock, void *sockaddr, int sockaddr_len)
{
	printk("%s:%d sock: %p\n", __func__, __LINE__, sock);
	return sock->ops->bind(sock, sockaddr, sockaddr_len);
}


int lx_sock_getname(struct socket *sock, void *sockaddr, int peer)
{
	printk("%s:%d sock: %p\n", __func__, __LINE__, sock);
	return sock->ops->getname(sock, sockaddr, peer);
}


int lx_sock_recvmsg(struct socket *sock, struct lx_msghdr *lx_msg,
                    int flags, int dontwait)
{
	printk("%s:%d sock: %p\n", __func__, __LINE__, sock);
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

	iovlen = 0;
	for (i = 0; i < iov_count; i++) {
		iov[i].iov_base = lx_msg->msg_iov[i].iov_base;
		iov[i].iov_len  = lx_msg->msg_iov[i].iov_len;

		printk("%s:%d msg_iov[%u].iov_len: %lu\n", __func__, __LINE__, i, lx_msg->msg_iov[i].iov_len);
		iovlen += lx_msg->msg_iov[i].iov_len;
	}

	msg->msg_name         = lx_msg->msg_name;
	msg->msg_namelen      = lx_msg->msg_namelen;
	msg->msg_iter.iov     = iov;
	msg->msg_iter.nr_segs = iov_count;
	msg->msg_iter.count   = iovlen;

	msg->msg_flags = flags;
	if (dontwait) {
		printk("%s: MSG_DONTWAIT sock->ops->recvmsg: %p\n", __func__, sock->ops->recvmsg);
		msg->msg_flags |= MSG_DONTWAIT;
		flags |= MSG_DONTWAIT;
	}

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
	printk("%s:%d sock: %p\n", __func__, __LINE__, sock);
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

	iovlen = 0;
	for (i = 0; i < iov_count; i++) {
		iov[i].iov_base = lx_msg->msg_iov[i].iov_base;
		iov[i].iov_len  = lx_msg->msg_iov[i].iov_len;

		printk("%s:%d msg_iov[%u].iov_len: %lu\n", __func__, __LINE__, i, lx_msg->msg_iov[i].iov_len);

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

	printk("%s:%d dontwait: %d\n", __func__, __LINE__, dontwait);
	err = sock->ops->sendmsg(sock, msg, iovlen);
	printk("%s:%d err: %d\n", __func__, __LINE__, err);

	kfree(iov);
err_iov:
	kfree(msg);
err_msg:
	return err;
}


int lx_sock_setsockopt(struct socket *sock, int level, int optname,
                       void const *optval, unsigned optlen)
{
	printk("%s:%d sock: %p\n", __func__, __LINE__, sock);
	sockptr_t soptval = (sockptr_t) { .user = optval };

	if (level == SOL_SOCKET)
		return sock_setsockopt(sock, level, optname,
		                       soptval, optlen);

	return sock->ops->setsockopt(sock, level, optname,
	                             soptval, optlen);
}


unsigned char const* lx_get_mac_addr()
{
	size_t i;

	static char mac_addr_buffer[16];
	memset(mac_addr_buffer, 0, sizeof (mac_addr_buffer));

	struct sockaddr addr;
	memset(addr.sa_data, 0, sizeof (addr.sa_data));

	int res = dev_get_mac_address(&addr, &init_net, "wlan0");
	if (res)
		return NULL;

	size_t const length =
		sizeof (mac_addr_buffer) < sizeof (addr.sa_data)
		                         ? sizeof (mac_addr_buffer)
		                         : sizeof (addr.sa_data);
	memcpy(mac_addr_buffer, addr.sa_data, length);
	// for (i = 0; i < sizeof (mac_addr_buffer); i++)
	// 	printk("%x:", mac_addr_buffer[i]);
	// printk("\n");

	return mac_addr_buffer;
}


struct lx_poll_result lx_sock_poll(struct socket *sock)
{
	enum {
		POLLIN_SET  = (EPOLLRDHUP | EPOLLIN | EPOLLRDNORM),
		POLLOUT_SET = (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND),
		POLLEX_SET  = (EPOLLERR | EPOLLPRI)
	};

	// enum {
	// 	POLLIN_SET  = (EPOLLRDNORM | EPOLLRDBAND | EPOLLIN | EPOLLHUP | EPOLLERR)
	// 	POLLOUT_SET = (EPOLLWRBAND | EPOLLWRNORM | EPOLLOUT | EPOLLERR)
	// 	POLLEX_SET =  (EPOLLPRI)
	// };

	struct lx_poll_result result = { false, false, false };

	if (!sock || !sock->ops) {
		printk("%s:%d sock: %p sk: %p ops: %p invalid\n",
		       __func__, __LINE__, sock, sock ? sock->sk : NULL, sock ? sock->ops : NULL);
		return result;
	}
	printk("%s:%d sock: %p sk: %p ops: %p\n",
	       __func__, __LINE__, sock, sock ? sock->sk : NULL, sock ? sock->ops : NULL);

	if (sock->ops == (void*)0xcbf400000000000 || sock->ops == (void*)0x2ee000) {
		printk("%s:%d sock: %p ops: %p BROKEN\n",
		       __func__, __LINE__, sock, sock ? sock->ops : NULL);
		return result;
	}

	int const mask = sock->ops->poll(0, sock, 0);

	if (mask & POLLIN_SET)
		result.in = true;
	if (mask & POLLOUT_SET)
		result.out = true;
	if (mask & POLLEX_SET)
		result.ex = true;
	printk("%s:%d: sock: %p sk: %p poll: %p mask: %x (in: %d (%x) out: %d (%x) ex: %d (%x)\n",
	       __func__, __LINE__, sock, sock->sk, sock->ops->poll, mask,
	       result.in, POLLIN_SET, result.out, POLLOUT_SET, result.ex, POLLEX_SET);

	return result;
}


int lx_sock_poll_wait(struct socket *socks[], unsigned num, int timeout)
{
	unsigned i;

	for (i = 0; i < num; i++) {
		struct socket *sock = socks[i];
		if (!sock) {
			printk("%s:%d ignore invalid sock[%u]\n", __func__, __LINE__, i);
			continue;
		}
		// printk("%s:%d sock: %p sk: %p sk_wq: %p\n", __func__, __LINE__, sock, sock->sk, sock->sk->sk_wq);
		printk("%s:%d sock: %p sk: %p\n", __func__, __LINE__, sock, sock->sk);
	}
	lx_emul_task_schedule(true);
	return 0;
}

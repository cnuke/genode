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

extern void uplink_init(void);

void lx_user_init(void)
{
	// open_wlan_device();
	// uplink_init();

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

	init_waitqueue_head(&(*res)->wq.wait);
	return 0;
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
	static char mac_addr_buffer[16];

	struct sockaddr addr;
	int err;
	size_t length;

	memset(mac_addr_buffer, 0, sizeof (mac_addr_buffer));
	memset(addr.sa_data, 0, sizeof (addr.sa_data));

	err = dev_get_mac_address(&addr, &init_net, "wlan0");
	if (err)
		return NULL;

	length = sizeof (mac_addr_buffer) < sizeof (addr.sa_data)
		                              ? sizeof (mac_addr_buffer)
		                              : sizeof (addr.sa_data);
	memcpy(mac_addr_buffer, addr.sa_data, length);

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

	int const mask = sock->ops->poll(0, sock, 0);

	struct lx_poll_result result = { false, false, false };

	if (mask & POLLIN_SET)
		result.in = true;
	if (mask & POLLOUT_SET)
		result.out = true;
	if (mask & POLLEX_SET)
		result.ex = true;

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
	}
	// lx_emul_task_schedule(true);
	// return 0;
	__set_current_state(TASK_INTERRUPTIBLE);
	timeout = 1000;
	unsigned long const j  = msecs_to_jiffies(timeout);
	signed   long const ex = schedule_timeout(j);
	unsigned int  const to =  jiffies_to_msecs(ex);
	// printk("%s:%d: timeout: %d to: %u\n", __func__, __LINE__, timeout, to);
	return (int)to+1;
}

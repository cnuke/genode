/*
 * \brief  Ioctl functions
 * \author Josef Soentgen
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* compiler includes */
#include <stdarg.h>

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* libc includes */
#include <sys/sockio.h>
#include <sys/socket.h>

/* local includes */
#include <wifi/socket_call.h>
#include <lx_user_emul.h>


extern Wifi::Socket_call socket_call;


extern "C" {

unsigned int wifi_ifindex(char const *ifname);
char const * wifi_ifname(void);

enum {
	MSG_EXCESSIVE, MSG_MSGDUMP, MSG_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR
};

void wpa_printf(int level, const char *fmt, ...);

/* need to define these specially as Linux and BSD disagree on them */
enum {
	LX_SIOCGIFFLAGS = 0x8913,
	LX_SIOCSIFFLAGS = 0x8914,
};

int ioctl(int fd, unsigned long request, ...)
{
	long arg;
	va_list ap;
	va_start(ap, request);
	arg = va_arg(ap, long);
	va_end(ap);

	struct ifreq *ifr = (struct ifreq *)(arg);

	switch (request) {
	case SIOCGIFADDR:
		Genode::error("ioctl: request SIOCGIFADDR not implemented.");
		return -1;
	case SIOCGIFINDEX:
		ifr->ifr_ifindex = wifi_ifindex(wifi_ifname());
		return 0;
	case SIOCGIFHWADDR:
		socket_call.get_mac_address((unsigned char*)ifr->ifr_hwaddr.sa_data);
		return 0;
	case SIOCGIFFLAGS:
		return socket_call.ioctl(LX_SIOCGIFFLAGS, (void *)ifr);
	case SIOCSIFFLAGS:
		return socket_call.ioctl(LX_SIOCSIFFLAGS, (void *)ifr);
	}

	Genode::warning("ioctl: request ", request, " not handled by switch");
	return -1;
}

unsigned int wifi_ifindex(const char *ifname)
{
	return socket_call.get_wifi_ifindex(ifname);
}

int linux_set_iface_flags(int sock, const char *ifname, int dev_up)
{
	struct ifreq ifr;
	int ret;

	if (sock < 0)
		return -1;

	Genode::memset(&ifr, 0, sizeof(ifr));
	Genode::copy_cstring(ifr.ifr_name, ifname, IFNAMSIZ);

	ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
	if ( ret != 0) {
		if (ret > 0)
			ret = -ret;
		wpa_printf(MSG_ERROR, "Could not read interface %s flags: %d",
		           ifname, ret);
		return ret;
	}

	if (dev_up) {
		if (ifr.ifr_flags & IFF_UP)
			return 0;
		ifr.ifr_flags |= IFF_UP;
	} else {
		if (!(ifr.ifr_flags & IFF_UP))
			return 0;
		ifr.ifr_flags &= ~IFF_UP;
	}

	ret = ioctl(sock, SIOCSIFFLAGS, &ifr);
	if ( ret != 0) {
		if (ret > 0)
			ret = -ret;
		wpa_printf(MSG_ERROR, "Could not set interface %s flags (%s): %d",
		           ifname, dev_up ? "UP" : "DOWN", ret);
		return ret;
	}
	return 0;
}


int linux_iface_up(int sock, const char *ifname)
{
	struct ifreq ifr;
	int ret;

	if (sock < 0)
		return -1;

	Genode::memset(&ifr, 0, sizeof(ifr));
	Genode::copy_cstring(ifr.ifr_name, ifname, IFNAMSIZ);

	ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
	if (ret != 0) {
		if (ret > 0)
			ret = -ret;
		wpa_printf(MSG_ERROR, "Could not read interface %s flags: %d",
		           ifname, ret);
		return ret;
	}

	return !!(ifr.ifr_flags & IFF_UP);
}


int linux_get_ifhwaddr(int sock, const char *ifname, unsigned char *addr)
{
	socket_call.get_mac_address(addr);
	return 0;
}


int linux_set_ifhwaddr(int sock, const char *ifname, const unsigned char *addr)
{
	return -1;
}


int linux_br_add(int sock, const char *brname) { return -1; }


int linux_br_del(int sock, const char *brname) { return -1; }


int linux_br_add_if(int sock, const char *brname, const char *ifname) {
	return -1; }


int linux_br_del_if(int sock, const char *brname, const char *ifname) {
	return -1; }


int linux_br_get(char *brname, const char *ifname) { return -1; }


int linux_master_get(char *master_ifname, const char *ifname) {
	return -1; }

} /* extern "C" */

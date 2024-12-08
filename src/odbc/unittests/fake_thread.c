/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/time.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include "fake_thread.h"

tds_thread fake_thread;

/* build a listening socket to connect to */
bool
init_fake_server(int ip_port)
{
	struct sockaddr_in sin;
	TDS_SYS_SOCKET s;

	memset(&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons((short) ip_port);
	sin.sin_family = AF_INET;

	if (TDS_IS_SOCKET_INVALID(s = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("socket");
		exit(1);
	}
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("bind");
		CLOSESOCKET(s);
		return false;
	}
	if (listen(s, 5) < 0) {
		perror("listen");
		CLOSESOCKET(s);
		return false;
	}
	if (tds_thread_create(&fake_thread, fake_thread_proc, TDS_INT2PTR(s)) != 0) {
		perror("tds_thread_create");
		exit(1);
	}
	return true;
}

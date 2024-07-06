/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2018  Ziglio Frediano
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <freetds/utils.h>
#include <freetds/utils/nosigpipe.h>
#include <freetds/macros.h>

/**
 * \addtogroup network
 * @{
 */

/**
 * Set socket to not throw SIGPIPE.
 * Not many systems support this feature (in this case ENOTSUP can be
 * returned).
 * @param sock socket to set
 * @param on   flag if enable or disable
 * @return 0 on success or error code
 */
int
tds_socket_set_nosigpipe(TDS_SYS_SOCKET sock TDS_UNUSED, int on TDS_UNUSED)
{
#if defined(SO_NOSIGPIPE)
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (const void *) &on, sizeof(on)))
		return sock_errno;
	return 0;
#elif defined(_WIN32)
	return 0;
#else
	return on ? ENOTSUP : 0;
#endif
}

/** @} */


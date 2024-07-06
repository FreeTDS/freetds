/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2024  Ziglio Frediano
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

#ifndef _tds_utils_nosigpipe_h_
#define _tds_utils_nosigpipe_h_

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if (!defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__FreeBSD) \
     && !defined(__NetBSD__) && !defined(__NetBSD)) \
	|| defined(__SYMBIAN32__)
#undef SO_NOSIGPIPE
#endif

#endif /* _tds_utils_nosigpipe_h_ */

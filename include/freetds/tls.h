/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2015  Frediano Ziglio
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

#ifndef _freetds_tls_h_
#define _freetds_tls_h_

#ifndef _tds_h_
#error tds.h must be included before tls.h
#endif

#ifdef HAVE_GNUTLS
#  if defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)
#    include <freetds/thread.h>
#    ifndef GNUTLS_USE_NETTLE
#      include <gcrypt.h>
#    endif
#  endif
#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>
#elif defined(HAVE_OPENSSL)
#  include <openssl/ssl.h>
#  include <openssl/x509v3.h>
#  include <openssl/err.h>
#endif

#include <freetds/pushvis.h>

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
TDSRET tds_ssl_init(TDSSOCKET *tds, bool full);
void tds_ssl_deinit(TDSCONNECTION *conn);

#  ifdef HAVE_GNUTLS

static inline int
tds_ssl_pending(TDSCONNECTION *conn)
{
	return gnutls_record_check_pending((gnutls_session_t) conn->tls_session);
}

static inline int
tds_ssl_read(TDSCONNECTION *conn, unsigned char *buf, int buflen)
{
	return gnutls_record_recv((gnutls_session_t) conn->tls_session, buf, buflen);
}

static inline int
tds_ssl_write(TDSCONNECTION *conn, const unsigned char *buf, int buflen)
{
	return gnutls_record_send((gnutls_session_t) conn->tls_session, buf, buflen);
}
#  else

/* compatibility for LibreSSL 2.7  */
#ifdef LIBRESSL_VERSION_NUMBER
#define TLS_ST_OK SSL_ST_OK
#endif

static inline int
tds_ssl_pending(TDSCONNECTION *conn)
{
	return SSL_pending((SSL *) conn->tls_session);
}

static inline int
tds_ssl_read(TDSCONNECTION *conn, unsigned char *buf, int buflen)
{
	return SSL_read((SSL *) conn->tls_session, buf, buflen);
}

static inline int
tds_ssl_write(TDSCONNECTION *conn, const unsigned char *buf, int buflen)
{
	return SSL_write((SSL *) conn->tls_session, buf, buflen);
}
#  endif
#else
static inline TDSRET
tds_ssl_init(TDSSOCKET *tds TDS_UNUSED, bool full TDS_UNUSED)
{
	return TDS_FAIL;
}

static inline void
tds_ssl_deinit(TDSCONNECTION *conn TDS_UNUSED)
{
}

static inline int
tds_ssl_pending(TDSCONNECTION *conn TDS_UNUSED)
{
	return 0;
}

static inline int
tds_ssl_read(TDSCONNECTION *conn TDS_UNUSED, unsigned char *buf TDS_UNUSED, int buflen TDS_UNUSED)
{
	return -1;
}

static inline int
tds_ssl_write(TDSCONNECTION *conn TDS_UNUSED, const unsigned char *buf TDS_UNUSED, int buflen TDS_UNUSED)
{
	return -1;
}
#endif

#include <freetds/popvis.h>

#endif /* _freetds_tls_h_ */

/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Brian Bruns
 * Copyright (C) 2004-2015  Ziglio Frediano
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

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/tds.h>
#include <freetds/string.h>
#include <freetds/tls.h>
#include "replacements.h"

#include <assert.h>

/**
 * \addtogroup network
 * @{ 
 */

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)

#ifdef HAVE_GNUTLS
#define SSL_RET ssize_t
#define SSL_PULL_ARGS gnutls_transport_ptr ptr, void *data, size_t len
#define SSL_PUSH_ARGS gnutls_transport_ptr ptr, const void *data, size_t len
#define SSL_PTR ptr
#else
#define SSL_RET int
#define SSL_PULL_ARGS BIO *bio, char *data, int len
#define SSL_PUSH_ARGS BIO *bio, const char *data, int len
#define SSL_PTR bio->ptr
#endif

static SSL_RET
tds_pull_func_login(SSL_PULL_ARGS)
{
	TDSSOCKET *tds = (TDSSOCKET *) SSL_PTR;
	int have;

	tdsdump_log(TDS_DBG_INFO1, "in tds_pull_func_login\n");
	
	/* here we are initializing (crypted inside TDS packets) */

	/* if we have some data send it */
	/* here MARS is not already initialized so test is correct */
	/* TODO test even after initializing ?? */
	if (tds->out_pos > 8)
		tds_flush_packet(tds);

	for(;;) {
		have = tds->in_len - tds->in_pos;
		tdsdump_log(TDS_DBG_INFO1, "have %d\n", have);
		assert(have >= 0);
		if (have > 0)
			break;
		tdsdump_log(TDS_DBG_INFO1, "before read\n");
		if (tds_read_packet(tds) < 0)
			return -1;
		tdsdump_log(TDS_DBG_INFO1, "after read\n");
	}
	if (len > have)
		len = have;
	tdsdump_log(TDS_DBG_INFO1, "read %lu bytes\n", (unsigned long int) len);
	memcpy(data, tds->in_buf + tds->in_pos, len);
	tds->in_pos += len;
	return len;
}

static SSL_RET
tds_push_func_login(SSL_PUSH_ARGS)
{
	TDSSOCKET *tds = (TDSSOCKET *) SSL_PTR;

	tdsdump_log(TDS_DBG_INFO1, "in tds_push_func_login\n");

	/* initializing SSL, write crypted data inside normal TDS packets */
	tds_put_n(tds, data, len);
	return len;
}

static SSL_RET
tds_pull_func(SSL_PULL_ARGS)
{
	TDSCONNECTION *conn = (TDSCONNECTION *) SSL_PTR;
	TDSSOCKET *tds;

	tdsdump_log(TDS_DBG_INFO1, "in tds_pull_func\n");

#if ENABLE_ODBC_MARS
	tds = conn->in_net_tds;
	assert(tds);
#else
	tds = (TDSSOCKET *) conn;
#endif

	/* already initialized (crypted TDS packets) */

	/* read directly from socket */
	/* TODO we block write on other sessions */
	/* also we should already have tested for data on socket */
	return tds_goodread(tds, (unsigned char*) data, len);
}

static SSL_RET
tds_push_func(SSL_PUSH_ARGS)
{
	TDSCONNECTION *conn = (TDSCONNECTION *) SSL_PTR;
	TDSSOCKET *tds;

	tdsdump_log(TDS_DBG_INFO1, "in tds_push_func\n");

	/* write to socket directly */
	/* TODO use cork if available here to flush only on last chunk of packet ?? */
#if ENABLE_ODBC_MARS
	tds = conn->in_net_tds;
	/* FIXME with SMP trick to detect final is not ok */
	return tds_goodwrite(tds, (const unsigned char*) data, len,
			     conn->send_packets->next == NULL);
#else
	tds = (TDSSOCKET *) conn;
	return tds_goodwrite(tds, (const unsigned char*) data, len, tds->out_buf[1]);
#endif
}

static int tls_initialized = 0;
static tds_mutex tls_mutex = TDS_MUTEX_INITIALIZER;

#ifdef HAVE_GNUTLS

static void
tds_tls_log( int level, const char* s)
{
	tdsdump_log(TDS_DBG_INFO1, "GNUTLS: level %d:\n  %s", level, s);
}

#ifdef TDS_ATTRIBUTE_DESTRUCTOR
static void __attribute__((destructor))
tds_tls_deinit(void)
{
	if (tls_initialized)
		gnutls_global_deinit();
}
#endif

#if defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#define tds_gcry_init() gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread)
#else
#define tds_gcry_init() do {} while(0)
#endif

static int
tds_verify_certificate(gnutls_session_t session)
{
	unsigned int status;
	int ret;

	ret = gnutls_certificate_verify_peers2(session, &status);
	if (ret < 0) {
		tdsdump_log(TDS_DBG_ERROR, "Error verifying certificate: %s\n", gnutls_strerror(ret));
		return GNUTLS_E_CERTIFICATE_ERROR;
	}

	/* Certificate is not trusted */
	if (status != 0) {
		tdsdump_log(TDS_DBG_ERROR, "Certificate status: %u\n", status);
		return GNUTLS_E_CERTIFICATE_ERROR;
	}

	/* notify gnutls to continue handshake normally */
	return 0;
}

TDSRET
tds_ssl_init(TDSSOCKET *tds)
{
	gnutls_session session;
	gnutls_certificate_credentials xcred;
	int ret;
	const char *tls_msg;

	xcred = NULL;
	session = NULL;	
	tls_msg = "initializing tls";

	if (!tls_initialized) {
		ret = 0;
		tds_mutex_lock(&tls_mutex);
		if (!tls_initialized) {
			tds_gcry_init();
			ret = gnutls_global_init();
			if (ret == 0)
				tls_initialized = 1;
		}
		tds_mutex_unlock(&tls_mutex);
		if (ret != 0)
			goto cleanup;
	}

	if (tds_write_dump && tls_initialized < 2) {
		gnutls_global_set_log_level(11);
		gnutls_global_set_log_function(tds_tls_log);
		tls_initialized = 2;
	}

	tls_msg = "allocating credentials";
	ret = gnutls_certificate_allocate_credentials(&xcred);
	if (ret != 0)
		goto cleanup;

	if (!tds_dstr_isempty(&tds->login->cafile)) {
		tls_msg = "loading CA file";
		ret = gnutls_certificate_set_x509_trust_file(xcred, tds_dstr_cstr(&tds->login->cafile), GNUTLS_X509_FMT_PEM);
		if (ret <= 0)
			goto cleanup;
		if (!tds_dstr_isempty(&tds->login->crlfile)) {
			tls_msg = "loading CRL file";
			ret = gnutls_certificate_set_x509_crl_file(xcred, tds_dstr_cstr(&tds->login->crlfile), GNUTLS_X509_FMT_PEM);
			if (ret <= 0)
				goto cleanup;
		}
		gnutls_certificate_set_verify_function(xcred, tds_verify_certificate);
	}

	/* Initialize TLS session */
	tls_msg = "initializing session";
	ret = gnutls_init(&session, GNUTLS_CLIENT);
	if (ret != 0)
		goto cleanup;

	gnutls_transport_set_ptr(session, tds);
	gnutls_transport_set_pull_function(session, tds_pull_func_login);
	gnutls_transport_set_push_function(session, tds_push_func_login);

	/* NOTE: there functions return int however they cannot fail */

	/* use default priorities... */
	gnutls_set_default_priority(session);

	/* ... but overwrite some */
	ret = gnutls_priority_set_direct (session, "NORMAL:%COMPAT:-VERS-SSL3.0", NULL);
	if (ret != 0)
		goto cleanup;

	/* mssql does not like padding too much */
#ifdef HAVE_GNUTLS_RECORD_DISABLE_PADDING
	gnutls_record_disable_padding(session);
#endif

	/* put the anonymous credentials to the current session */
	tls_msg = "setting credential";
	ret = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	if (ret != 0)
		goto cleanup;

	/* Perform the TLS handshake */
	tls_msg = "handshake";
	ret = gnutls_handshake (session);
	if (ret != 0)
		goto cleanup;

	tdsdump_log(TDS_DBG_INFO1, "handshake succeeded!!\n");

	gnutls_transport_set_ptr(session, tds->conn);
	gnutls_transport_set_pull_function(session, tds_pull_func);
	gnutls_transport_set_push_function(session, tds_push_func);

	tds->conn->tls_session = session;
	tds->conn->tls_credentials = xcred;

	return TDS_SUCCESS;

cleanup:
	if (session)
		gnutls_deinit(session);
	if (xcred)
		gnutls_certificate_free_credentials(xcred);
	tdsdump_log(TDS_DBG_ERROR, "%s failed: %s\n", tls_msg, gnutls_strerror (ret));
	return TDS_FAIL;
}

void
tds_ssl_deinit(TDSCONNECTION *conn)
{
	if (conn->tls_session) {
		gnutls_deinit((gnutls_session) conn->tls_session);
		conn->tls_session = NULL;
	}
	if (conn->tls_credentials) {
		gnutls_certificate_free_credentials((gnutls_certificate_credentials) conn->tls_credentials);
		conn->tls_credentials = NULL;
	}
}

#else
static long
tds_ssl_ctrl_login(BIO *b, int cmd, long num, void *ptr)
{
	TDSSOCKET *tds = (TDSSOCKET *) b->ptr;

	switch (cmd) {
	case BIO_CTRL_FLUSH:
		if (tds->out_pos > 8)
			tds_flush_packet(tds);
		return 1;
	}
	return 0;
}

static int
tds_ssl_free(BIO *a)
{
	/* nothing to do but required */
	return 1;
}

static BIO_METHOD tds_method_login =
{
	BIO_TYPE_MEM,
	"tds",
	tds_push_func_login,
	tds_pull_func_login,
	NULL,
	NULL,
	tds_ssl_ctrl_login,
	NULL,
	tds_ssl_free,
	NULL,
};

static BIO_METHOD tds_method =
{
	BIO_TYPE_MEM,
	"tds",
	tds_push_func,
	tds_pull_func,
	NULL,
	NULL,
	NULL,
	NULL,
	tds_ssl_free,
	NULL,
};


static SSL_CTX *
tds_init_openssl(void)
{
	const SSL_METHOD *meth;

	if (!tls_initialized) {
		tds_mutex_lock(&tls_mutex);
		if (!tls_initialized) {
			SSL_library_init();
			tls_initialized = 1;
		}
		tds_mutex_unlock(&tls_mutex);
	}
	meth = TLSv1_client_method ();
	if (meth == NULL)
		return NULL;
	return SSL_CTX_new (meth);
}

int
tds_ssl_init(TDSSOCKET *tds)
{
#define OPENSSL_CIPHERS \
	"DHE-RSA-AES256-SHA DHE-DSS-AES256-SHA " \
	"AES256-SHA EDH-RSA-DES-CBC3-SHA " \
	"EDH-DSS-DES-CBC3-SHA DES-CBC3-SHA " \
	"DES-CBC3-MD5 DHE-RSA-AES128-SHA " \
	"DHE-DSS-AES128-SHA AES128-SHA RC2-CBC-MD5 RC4-SHA RC4-MD5"

	SSL *con;
	SSL_CTX *ctx;
	BIO *b, *b2;

	int ret;
	const char *tls_msg;

	con = NULL;
	b = NULL;
	b2 = NULL;
	ret = 1;

	tds_ssl_deinit(tds->conn);

	tls_msg = "initializing tls";
	ctx = tds_init_openssl();
	if (!ctx)
		goto cleanup;

	if (!tds_dstr_isempty(&tds->login->cafile)) {
		tls_msg = "loading CA file";
		ret = SSL_CTX_load_verify_locations(ctx, tds_dstr_cstr(&tds->login->cafile), NULL);
		if (ret != 1)
			goto cleanup;
		if (!tds_dstr_isempty(&tds->login->crlfile)) {
			X509_STORE *store = SSL_CTX_get_cert_store(ctx);
			X509_LOOKUP *lookup;

			tls_msg = "loading CRL file";
			if (!(lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file()))
			    || (!X509_load_crl_file(lookup, tds_dstr_cstr(&tds->login->crlfile), X509_FILETYPE_PEM)))
				goto cleanup;

			X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
		}
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	}

	/* Initialize TLS session */
	tls_msg = "initializing session";
	con = SSL_new(ctx);
	if (!con)
		goto cleanup;

	tls_msg = "creating bio";
	b = BIO_new(&tds_method_login);
	if (!b)
		goto cleanup;

	b2 = BIO_new(&tds_method);
	if (!b2)
		goto cleanup;

	b->shutdown=1;
	b->init=1;
	b->num= -1;
	b->ptr = tds;
	SSL_set_bio(con, b, b);
	b = NULL;

	/* use priorities... */
	SSL_set_cipher_list(con, OPENSSL_CIPHERS);

#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
	/* this disable a security improvement but allow connection... */
	SSL_set_options(con, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif

	/* Perform the TLS handshake */
	tls_msg = "handshake";
	SSL_set_connect_state(con);
	ret = SSL_connect(con) != 1 || con->state != SSL_ST_OK;
	if (ret != 0)
		goto cleanup;

	tdsdump_log(TDS_DBG_INFO1, "handshake succeeded!!\n");

	b2->shutdown = 1;
	b2->init = 1;
	b2->num = -1;
	b2->ptr = tds->conn;
	SSL_set_bio(con, b2, b2);

	tds->conn->tls_session = con;
	tds->conn->tls_ctx = ctx;

	return TDS_SUCCESS;

cleanup:
	if (b2)
		BIO_free(b2);
	if (b)
		BIO_free(b);
	if (con) {
		SSL_shutdown(con);
		SSL_free(con);
	}
	SSL_CTX_free(ctx);
	tdsdump_log(TDS_DBG_ERROR, "%s failed\n", tls_msg);
	return TDS_FAIL;
}

void
tds_ssl_deinit(TDSCONNECTION *conn)
{
	if (conn->tls_session) {
		/* NOTE do not call SSL_shutdown here */
		SSL_free(conn->tls_session);
		conn->tls_session = NULL;
	}
	if (conn->tls_ctx) {
		SSL_CTX_free(conn->tls_ctx);
		conn->tls_ctx = NULL;
	}
}
#endif

#endif
/** @} */


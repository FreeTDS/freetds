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

#include <config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/time.h>
#include <freetds/tds.h>
#include <freetds/bytes.h>
#include <freetds/string.h>
#include "replacements.h"

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/abstract.h>

#include <nettle/asn1.h>
#include <nettle/rsa.h>
#include <nettle/bignum.h>
#elif defined(HAVE_OPENSSL)
#include <openssl/rand.h>
#endif


/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{
 */

#if defined(HAVE_NETTLE) && defined(HAVE_GMP)

static void
rnd_func(void *ctx, unsigned len, uint8_t * out)
{
	tds_random_buffer(out, len);
}

static void
sha1(uint8_t *hash, const void *data, size_t len)
{
	struct sha1_ctx ctx;
	sha1_init(&ctx);
	sha1_update(&ctx, len, (const uint8_t *) data);
	sha1_digest(&ctx, 20, hash);
}

#define dumpl(b,l) tdsdump_dump_buf(TDS_DBG_INFO1, #b, b, l)
#ifndef dumpl
#define dumpl(b,l) do {} while(0)
#endif
#define dump(b) dumpl(b, sizeof(b))

/* OAEP configuration parameters */
#define hash_func sha1
enum { hash_len = 20  };	/* sha1 length */
enum { key_size_max = 1024 };	/* max key in bytes */
static const char label[] = "";

static void
xor(uint8_t *dest, const uint8_t *src, size_t len)
{
	size_t n;
	for (n = 0; n < len; ++n)
		dest[n] = dest[n] ^ src[n];
}

static void
mgf_mask(uint8_t *dest, size_t dest_len, const uint8_t *mask, size_t mask_len)
{
	unsigned n = 0;
	uint8_t hash[hash_len];
	uint8_t seed[mask_len + 4];

	memcpy(seed, mask, mask_len);
	/* we always have some data and check is done internally */
	for (;;) {
		TDS_PUT_UA4BE(seed+mask_len, n);

		hash_func(hash, seed, sizeof(seed));
		if (dest_len <= hash_len) {
			xor(dest, hash, dest_len);
			break;
		}

		xor(dest, hash, hash_len);
		dest += hash_len;
		dest_len -= hash_len;
		++n;
	}
}

static int
oaep_encrypt(size_t key_size, void *random_ctx, nettle_random_func *random,
	       size_t length, const uint8_t *message, mpz_t m)
{
	/* EM: 0x00 ROS (HASH 0x00.. 0x01 message) */
	struct {
		uint8_t all[1]; /* zero but used to access all data */
		uint8_t ros[hash_len];
		uint8_t db[key_size_max - hash_len - 1];
	} em;
	const unsigned db_len = key_size - hash_len - 1;

	if (length + hash_len * 2 + 2 > key_size)
		/* Message too long for this key. */
		return 0;

	/* create db */
	memset(&em, 0, sizeof(em));
	hash_func(em.db, label, strlen(label));
	em.all[key_size - length - 1] = 0x1;
	memcpy(em.all+(key_size - length), message, length);
	dumpl(em.db, db_len);

	/* create ros */
	random(random_ctx, hash_len, em.ros);
	dump(em.ros);

	/* mask db */
	mgf_mask(em.db, db_len, em.ros, hash_len);
	dumpl(em.db, db_len);

	/* mask ros */
	mgf_mask(em.ros, hash_len, em.db, db_len);
	dump(em.ros);

	nettle_mpz_set_str_256_u(m, key_size, em.all);

	return 1;
}

static int
rsa_encrypt_oaep(const struct rsa_public_key *key, void *random_ctx, nettle_random_func *random,
	    size_t length, const uint8_t *message, mpz_t gibberish)
{
	if (!oaep_encrypt(key->size, random_ctx, random, length, message, gibberish))
		return 0;

	mpz_powm(gibberish, gibberish, key->e, key->n);
	return 1;
}

static void*
tds5_rsa_encrypt(const char *key, size_t key_len, const void *nonce, size_t nonce_len, const char *pwd, size_t *em_size)
{
	int ret;
	mpz_t p;
	gnutls_datum_t pubkey_datum = { (void *) key, key_len };
	struct asn1_der_iterator der;
	struct rsa_public_key pubkey;
	uint8_t *message;
	size_t message_len, pwd_len;
	uint8_t *em = NULL;
	unsigned char der_buf[2048];
	size_t size = sizeof(der_buf);

	mpz_init(p);
	rsa_public_key_init(&pubkey);

	pwd_len = strlen(pwd);
	message_len = nonce_len + pwd_len;
	message = (uint8_t *) malloc(message_len);
	if (!message)
		return NULL;
	memcpy(message, nonce, nonce_len);
	memcpy(message + nonce_len, pwd, pwd_len);

	/* use nettle directly */
	/* parse PEM, get DER */
	ret = gnutls_pem_base64_decode("RSA PUBLIC KEY", &pubkey_datum, der_buf, &size);
	if (ret) {
		tdsdump_log(TDS_DBG_ERROR, "Error %d decoding public key: %s\n", ret, gnutls_strerror(ret));
		goto error;
	}

	/* get key with nettle using DER */
	ret = asn1_der_iterator_first(&der, size, der_buf);
	if (ret != ASN1_ITERATOR_CONSTRUCTED || der.type != ASN1_SEQUENCE) {
		tdsdump_log(TDS_DBG_ERROR, "Invalid DER content\n");
		goto error;
	}

	ret = rsa_public_key_from_der_iterator(&pubkey, key_size_max * 8, &der);
	if (!ret) {
		tdsdump_log(TDS_DBG_ERROR, "Invalid DER content\n");
		goto error;
	}

	/* get password encrypted */
	ret = rsa_encrypt_oaep(&pubkey, NULL, rnd_func, message_len, message, p);
	if (!ret) {
		tdsdump_log(TDS_DBG_ERROR, "Error encrypting message\n");
		goto error;
	}

	em = malloc(pubkey.size);
	*em_size = pubkey.size;
	if (!em)
		goto error;

	nettle_mpz_get_str_256(pubkey.size, em, p);

	tdsdump_dump_buf(TDS_DBG_INFO1, "em", em, pubkey.size);

error:
	free(message);
	rsa_public_key_clear(&pubkey);
	mpz_clear(p);
	return em;
}


typedef struct tds5_negotiate
{
	TDSAUTHENTICATION tds_auth;
	/** message type from server */
	unsigned msg_type;
} TDS5NEGOTIATE;

static TDSRET
tds5_negotiate_free(TDSCONNECTION * conn, TDSAUTHENTICATION * tds_auth)
{
	TDS5NEGOTIATE *auth = (TDS5NEGOTIATE *) tds_auth;

	free(auth->tds_auth.packet);
	free(auth);

	return TDS_SUCCESS;
}

void
tds5_negotiate_set_msg_type(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth, unsigned msg_type)
{
	TDS5NEGOTIATE *auth = (TDS5NEGOTIATE *) tds_auth;

	if (tds_auth && tds_auth->free == tds5_negotiate_free)
		auth->msg_type = msg_type;
}

static void
tds5_send_msg(TDSSOCKET *tds, TDS_USMALLINT msg_type)
{
	tds_put_tinyint(tds, TDS_MSG_TOKEN);
	tds_put_tinyint(tds, 3);
	tds_put_tinyint(tds, 1);
	tds_put_smallint(tds, msg_type);
}

static TDSRET
tds5_negotiate_handle_next(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth, size_t len)
{
	TDS5NEGOTIATE *auth = (TDS5NEGOTIATE *) tds_auth;
	TDSPARAMINFO *info;
	void *rsa, *nonce = NULL;
	size_t rsa_len, nonce_len = 0;
	uint8_t *em;
	size_t em_size;
	TDSRET rc = TDS_FAIL;

	/* send next data for authentication */

	if (!tds->login)
		goto error;

	/* we only support RSA authentication, we should have send 2/3 parameters:
	 * 1- integer.. unknown actually 1 TODO
	 * 2- binary, rsa public key in PEM format
	 * 3- binary, nonce (optional)
	 */

	/* message not supported */
	if (auth->msg_type != 0x1e)
		goto error;

	info = tds->param_info;
	if (!info || info->num_cols < 2)
		goto error;

	if (info->columns[1]->column_type != SYBLONGBINARY)
		goto error;
	if (info->num_cols >= 3 && info->columns[2]->column_type != SYBLONGBINARY)
		goto error;
	rsa = ((TDSBLOB*) info->columns[1]->column_data)->textvalue;
	rsa_len = info->columns[1]->column_size;
	if (info->num_cols >= 3) {
		nonce = ((TDSBLOB*) info->columns[2]->column_data)->textvalue;
		nonce_len = info->columns[2]->column_size;
	}

	em = tds5_rsa_encrypt(rsa, rsa_len, nonce, nonce_len, tds_dstr_cstr(&tds->login->password), &em_size);
	if (!em)
		goto error;

	tds->out_flag = TDS_NORMAL;

	/* password */
	tds5_send_msg(tds, 0x1f);
	tds_put_n(tds, "\xec\x0e\x00\x01\x00\x00\x00\x00\x00\x00\x00\xe1\xff\xff\xff\x7f\x00", 0x11);
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	tds_put_int(tds, em_size);
	tds_put_n(tds, em, em_size);

	/* remote password */
	tds5_send_msg(tds, 0x20);
	tds_put_n(tds, "\xec\x17\x00\x02\x00\x00\x00\x00\x00\x00\x00\x27\xff\x00\x00\x00\x00\x00\x00\x00\xe1\xff\xff\xff\x7f\x00", 0x1a);
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	tds_put_byte(tds, 0);
	tds_put_int(tds, em_size);
	tds_put_n(tds, em, em_size);

	free(em);

	rc = tds_flush_packet(tds);

error:
	tds5_negotiate_free(tds->conn, tds_auth);
	tds->conn->authentication = NULL;

	return rc;
}

/**
 * Initialize Sybase negotiate handling
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @return authentication info
 */
TDSAUTHENTICATION *
tds5_negotiate_get_auth(TDSSOCKET * tds)
{
	TDS5NEGOTIATE *auth;

	if (!tds->login)
		return NULL;

	auth = (TDS5NEGOTIATE *) calloc(1, sizeof(TDS5NEGOTIATE));
	if (!auth)
		return NULL;

	auth->tds_auth.free = tds5_negotiate_free;
	auth->tds_auth.handle_next = tds5_negotiate_handle_next;

	return (TDSAUTHENTICATION *) auth;
}

#else /* not Nettle or GMP */

void
tds5_negotiate_set_msg_type(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth, unsigned msg_type)
{
}

TDSAUTHENTICATION *
tds5_negotiate_get_auth(TDSSOCKET * tds)
{
	tdsdump_log(TDS_DBG_ERROR,
		"Sybase authentication not supported if GnuTLS Nettle and GMP libraries not present\n");

	return NULL;
}

#endif

/** @} */


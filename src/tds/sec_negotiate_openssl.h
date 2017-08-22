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

#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{
 */

#ifndef HAVE_OPENSSL
#error HAVE_OPENSSL not defines, this file should not be included
#endif

static inline const BIGNUM*
rsa_get_n(const RSA *rsa)
{
#if HAVE_RSA_GET0_KEY
	const BIGNUM *n, *e, *d;
	RSA_get0_key(rsa, &n, &e, &d);
	return n;
#else
	return rsa->n;
#endif
}

static void*
tds5_rsa_encrypt(const void *key, size_t key_len, const void *nonce, size_t nonce_len, const char *pwd, size_t *em_size)
{
	RSA *rsa = NULL;
	BIO *keybio;

	uint8_t *message = NULL;
	size_t message_len, pwd_len;
	uint8_t *em = NULL;

	int result;

	keybio = BIO_new_mem_buf((void*) key, key_len);
	if (keybio == NULL)
		goto error;

	rsa = PEM_read_bio_RSAPublicKey(keybio, &rsa, NULL, NULL);
	if (!rsa)
		goto error;

	pwd_len = strlen(pwd);
	message_len = nonce_len + pwd_len;
	message = tds_new(uint8_t, message_len);
	if (!message)
		goto error;
	memcpy(message, nonce, nonce_len);
	memcpy(message + nonce_len, pwd, pwd_len);

	em = tds_new(uint8_t, BN_num_bytes(rsa_get_n(rsa)));
	if (!em)
		goto error;

	result = RSA_public_encrypt(message_len, message, em, rsa, RSA_PKCS1_OAEP_PADDING);
	if (result < 0)
		goto error;

	free(message);
	RSA_free(rsa);
	BIO_free(keybio);

	*em_size = result;
	return em;

error:
	free(message);
	free(em);
	RSA_free(rsa);
	BIO_free(keybio);
	return NULL;
}

/** @} */


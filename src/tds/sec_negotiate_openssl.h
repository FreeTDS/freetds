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
#include <openssl/rsa.h>

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

static void *
tds5_rsa_encrypt(const void *pem_key, size_t pem_key_len, const void *nonce, size_t nonce_len, const char *pwd, size_t *em_size)
{
	void *ret = NULL;
	EVP_PKEY *key = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	BIO *keybio;

#if OPENSSL_VERSION_NUMBER < 0x3000000FL
	RSA *rsa = NULL;
#endif

	uint8_t *message = NULL;
	size_t message_len, pwd_len;
	uint8_t *em = NULL;

	keybio = BIO_new_mem_buf((void *) pem_key, pem_key_len);
	if (keybio == NULL)
		goto error;

#if OPENSSL_VERSION_NUMBER < 0x3000000FL
	/* Old OpenSSL versions seem to not like RSA public key format if PEM_read_bio_PUBKEY is used */
	key = EVP_PKEY_new();
	if (!key)
		goto error;

	rsa = PEM_read_bio_RSAPublicKey(keybio, &rsa, NULL, NULL);
	if (!rsa)
		goto error;

	EVP_PKEY_set1_RSA(key, rsa);
#else
	key = PEM_read_bio_PUBKEY(keybio, &key, NULL, NULL);
	if (!key)
		goto error;
#endif

	pwd_len = strlen(pwd);
	message_len = nonce_len + pwd_len;
	message = tds_new(uint8_t, message_len);
	if (!message)
		goto error;
	memcpy(message, nonce, nonce_len);
	memcpy(message + nonce_len, pwd, pwd_len);

	*em_size = EVP_PKEY_size(key);
	em = tds_new(uint8_t, *em_size);
	if (!em)
		goto error;

	ctx = EVP_PKEY_CTX_new(key, NULL);
	if (!ctx)
		goto error;
	if (EVP_PKEY_encrypt_init(ctx) <= 0
	    || EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, -1, EVP_PKEY_CTRL_RSA_PADDING, RSA_PKCS1_OAEP_PADDING, NULL) <= 0)
		goto error;

	if (EVP_PKEY_encrypt(ctx, em, em_size, message, message_len) <= 0)
		goto error;

	ret = em;

      error:
#if OPENSSL_VERSION_NUMBER < 0x3000000FL
	RSA_free(rsa);
#endif
	EVP_PKEY_CTX_free(ctx);
	free(message);
	if (!ret)
		free(em);
	EVP_PKEY_free(key);
	BIO_free(keybio);
	return ret;
}

/** @} */

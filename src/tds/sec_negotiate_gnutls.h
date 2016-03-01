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

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#ifdef HAVE_GNUTLS_ABSTRACT_H
#  include <gnutls/abstract.h>
#endif

#if !defined(HAVE_NETTLE) || !defined(HAVE_GMP) || !defined(HAVE_GNUTLS_RND)
#  include <gcrypt.h>
#endif

#ifndef HAVE_NETTLE
#  include <libtasn1.h>
#endif

#ifdef HAVE_NETTLE
#  include <nettle/asn1.h>
#  include <nettle/rsa.h>
#  include <nettle/bignum.h>
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

#ifndef HAVE_GNUTLS
#error HAVE_GNUTLS not defines, this file should not be included
#endif

/* emulate GMP if not present */
#ifndef HAVE_GMP
#define HAVE_GMP 1

typedef struct {
	gcry_mpi_t num;
} mpz_t[1];

#define mpz_powm(w,n,e,m) \
	gcry_mpi_powm((w)->num, (n)->num, (e)->num, (m)->num);
#define mpz_init(n) do { (n)->num = NULL; } while(0)
#define mpz_clear(n) gcry_mpi_release((n)->num)

#endif


/* emulate Nettle is not present */
#ifndef HAVE_NETTLE
#define HAVE_NETTLE 1

typedef void nettle_random_func(void *ctx, size_t len, uint8_t *out);

static inline void
nettle_mpz_set_str_256_u(mpz_t x, unsigned length, const uint8_t *s)
{
	gcry_mpi_scan(&x->num, GCRYMPI_FMT_USG, s, length, NULL);
}

static inline void
nettle_mpz_get_str_256(unsigned length, uint8_t *s, const mpz_t x)
{
	gcry_mpi_print(GCRYMPI_FMT_USG, s, length, NULL, x->num);
}

struct asn1_der_iterator {
	const unsigned char *data, *data_end;
	unsigned long length;
	unsigned long type;
};

enum asn1_iterator_result {
	ASN1_ITERATOR_ERROR,
	ASN1_ITERATOR_PRIMITIVE,
	ASN1_ITERATOR_CONSTRUCTED,
	ASN1_ITERATOR_END,
};

enum {
	ASN1_SEQUENCE = ASN1_TAG_SEQUENCE,
};

static enum asn1_iterator_result
asn1_der_iterator_next(struct asn1_der_iterator *der)
{
	unsigned char cls;
	unsigned long tag;
	int len;
	long l;

	if (asn1_get_tag_der(der->data, der->data_end - der->data, &cls, &len, &tag) != ASN1_SUCCESS)
		return ASN1_ITERATOR_ERROR;
	der->type = tag;
	der->data += len;
	l = asn1_get_length_der(der->data, der->data_end - der->data, &len);
	if (l < 0)
		return ASN1_ITERATOR_ERROR;
	der->data += len;
	der->length = l;
	if (cls == ASN1_CLASS_STRUCTURED)
		return ASN1_ITERATOR_CONSTRUCTED;
	return ASN1_ITERATOR_PRIMITIVE;
}

static enum asn1_iterator_result
asn1_der_iterator_first(struct asn1_der_iterator *der, int size, const void *der_buf)
{
	der->data = (const unsigned char *) der_buf;
	der->data_end = der->data + size;

	return asn1_der_iterator_next(der);
}

struct rsa_public_key {
	unsigned size;
	mpz_t n, e;
};

static void
rsa_public_key_init(struct rsa_public_key *key)
{
	key->size = 0;
	mpz_init(key->n);
	mpz_init(key->e);
}

static void
rsa_public_key_clear(struct rsa_public_key *key)
{
	mpz_clear(key->n);
	mpz_clear(key->e);
}

static int
rsa_public_key_from_der_iterator(struct rsa_public_key *key, unsigned key_bits, struct asn1_der_iterator *der)
{
	enum asn1_iterator_result ret;

	ret = asn1_der_iterator_next(der);
	if (ret != ASN1_ITERATOR_PRIMITIVE || der->type != ASN1_TAG_INTEGER)
		return 0;
	gcry_mpi_scan(&key->n->num, GCRYMPI_FMT_USG, der->data, der->length, NULL);
	key->size = (gcry_mpi_get_nbits(key->n->num)+7)/8;
	der->data += der->length;

	ret = asn1_der_iterator_next(der);
	if (ret != ASN1_ITERATOR_PRIMITIVE || der->type != ASN1_TAG_INTEGER)
		return 0;
	gcry_mpi_scan(&key->e->num, GCRYMPI_FMT_USG, der->data, der->length, NULL);

	return 1;
}

static void
sha1(uint8_t *hash, const void *data, size_t len)
{
	gcry_md_hash_buffer(GCRY_MD_SHA1, hash, data, len);
}
#else
static void
sha1(uint8_t *hash, const void *data, size_t len)
{
	struct sha1_ctx ctx;
	sha1_init(&ctx);
	sha1_update(&ctx, len, (const uint8_t *) data);
	sha1_digest(&ctx, 20, hash);
}
#endif


static void
rnd_func(void *ctx, size_t len, uint8_t * out)
{
	tds_random_buffer(out, len);
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
memxor(uint8_t *dest, const uint8_t *src, size_t len)
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
			memxor(dest, hash, dest_len);
			break;
		}

		memxor(dest, hash, hash_len);
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
tds5_rsa_encrypt(const void *key, size_t key_len, const void *nonce, size_t nonce_len, const char *pwd, size_t *em_size)
{
	int ret;
	mpz_t p;
	gnutls_datum_t pubkey_datum = { (unsigned char *) key, key_len };
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
	message = tds_new(uint8_t, message_len);
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

	em = tds_new(uint8_t, pubkey.size);
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

/** @} */


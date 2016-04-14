#ifndef DES_H
#define DES_H

#ifdef HAVE_NETTLE
#include <nettle/des.h>

typedef struct des_ctx DES_KEY;
#endif

#include <freetds/pushvis.h>

typedef unsigned char des_cblock[8];

#ifndef HAVE_NETTLE
typedef struct des_key
{
	unsigned char kn[16][8];
	TDS_UINT sp[8][64];
	unsigned char iperm[16][16][8];
	unsigned char fperm[16][16][8];
} DES_KEY;

int tds_des_set_key(DES_KEY * dkey, const des_cblock user_key, int len);
void tds_des_encrypt(DES_KEY * key, des_cblock block);
#endif

void tds_des_set_odd_parity(des_cblock key);
int tds_des_ecb_encrypt(const void *plaintext, int len, DES_KEY * akey, unsigned char *output);

#include <freetds/popvis.h>

#ifdef HAVE_NETTLE
static inline void tds_des_encrypt(DES_KEY * key, des_cblock block)
{
	nettle_des_encrypt(key, sizeof(des_cblock), block, block);
}

static inline int tds_des_set_key(DES_KEY * dkey, const des_cblock user_key, int len)
{
	return nettle_des_set_key(dkey, user_key);
}
#endif

#endif /* !DES_H */

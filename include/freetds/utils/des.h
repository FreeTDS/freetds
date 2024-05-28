#ifndef _tdsguard_frXREepoqzIh7i5y4TSoh7_
#define _tdsguard_frXREepoqzIh7i5y4TSoh7_

#ifdef HAVE_NETTLE
#include <nettle/des.h>

typedef struct des_ctx DES_KEY;
#endif

#include <freetds/pushvis.h>

typedef uint8_t des_cblock[8];

#ifndef HAVE_NETTLE
typedef struct des_key
{
	uint8_t  kn[16][8];
	uint32_t sp[8][64];
	uint8_t  iperm[16][16][8];
	uint8_t  fperm[16][16][8];
} DES_KEY;

int tds_des_set_key(DES_KEY * dkey, const des_cblock user_key);
void tds_des_encrypt(const DES_KEY * key, des_cblock block);
#endif

void tds_des_set_odd_parity(des_cblock key);
int tds_des_ecb_encrypt(const void *plaintext, size_t len, DES_KEY * akey, uint8_t *output);

#include <freetds/popvis.h>

#ifdef HAVE_NETTLE
static inline void tds_des_encrypt(const DES_KEY * key, des_cblock block)
{
	nettle_des_encrypt(key, sizeof(des_cblock), block, block);
}

static inline int tds_des_set_key(DES_KEY * dkey, const des_cblock user_key)
{
	return nettle_des_set_key(dkey, user_key);
}
#endif

#endif /* !_tdsguard_frXREepoqzIh7i5y4TSoh7_ */

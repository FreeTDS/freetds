#ifndef DES_H
#define DES_H

static char rcsid_des_h[] =
	"$Id: des.h,v 1.7 2002-12-18 03:33:57 jklowden Exp $";
static void *no_unused_des_h_warn[] = {
	rcsid_des_h,
	no_unused_des_h_warn
};

typedef unsigned char des_cblock[8];

typedef struct des_key {
	unsigned char kn[16][8];
	TDS_UINT sp[8][64];
	unsigned char iperm[16][16][8];
	unsigned char fperm[16][16][8];
} DES_KEY;

void des_set_odd_parity(des_cblock key);
int tds_des_ecb_encrypt(const void *plaintext, int len, DES_KEY *akey, des_cblock output);
int des_set_key(DES_KEY *dkey, des_cblock user_key, int len);
void des_encrypt(DES_KEY *key, des_cblock block);
void _mcrypt_decrypt(DES_KEY *key, unsigned char *block);

#endif /* !DES_H */

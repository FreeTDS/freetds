#ifndef DES_H
#define DES_H

typedef unsigned char des_cblock[8];

typedef struct des_key {
	char kn[16][8];
	TDS_UINT sp[8][64];
	char iperm[16][16][8];
	char fperm[16][16][8];
} DES_KEY;

void des_set_odd_parity(des_cblock key);
int des_ecb_encrypt( void *plaintext, int len, DES_KEY *akey, des_cblock *output);

#endif /* !DES_H */

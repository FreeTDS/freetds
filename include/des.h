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
int des_ecb_encrypt(const void *plaintext, int len, DES_KEY *akey, des_cblock *output);
int des_set_key(DES_KEY *dkey, char *user_key, int len);
void des_encrypt(DES_KEY *key, des_cblock *block);
void _mcrypt_decrypt(DES_KEY *key, char *block);

#endif /* !DES_H */

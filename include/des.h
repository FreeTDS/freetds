#ifndef DES_H
#define DES_H

static const char rcsid_des_h[] = "$Id: des.h,v 1.10 2004-10-28 12:42:12 freddy77 Exp $";
static const void *const no_unused_des_h_warn[] = {	rcsid_des_h, no_unused_des_h_warn };

typedef unsigned char des_cblock[8];

typedef struct des_key
{
	unsigned char kn[16][8];
	TDS_UINT sp[8][64];
	unsigned char iperm[16][16][8];
	unsigned char fperm[16][16][8];
} DES_KEY;

void tds_des_set_odd_parity(des_cblock key);
int tds_des_ecb_encrypt(const void *plaintext, int len, DES_KEY * akey, des_cblock output);
int tds_des_set_key(DES_KEY * dkey, des_cblock user_key, int len);
void tds_des_encrypt(DES_KEY * key, des_cblock block);
void _mcrypt_decrypt(DES_KEY * key, unsigned char *block);

#endif /* !DES_H */

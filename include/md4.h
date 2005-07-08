#ifndef MD4_H
#define MD4_H

/* $Id: md4.h,v 1.6 2005-07-08 08:22:52 freddy77 Exp $ */

struct MD4Context
{
	TDS_UINT buf[4];
	TDS_UINT bits[2];
	unsigned char in[64];
};

void MD4Init(struct MD4Context *context);
void MD4Update(struct MD4Context *context, unsigned char const *buf, unsigned len);
void MD4Final(struct MD4Context *context, unsigned char *digest);
void MD4Transform(TDS_UINT buf[4], TDS_UINT const in[16]);

typedef struct MD4Context MD4_CTX;

#endif /* !MD4_H */

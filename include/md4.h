#ifndef MD4_H
#define MD4_H

static const char rcsid_md4_h[] = "$Id: md4.h,v 1.5 2004-10-28 12:42:12 freddy77 Exp $";
static const void *const no_unused_md4_h_warn[] = { rcsid_md4_h, no_unused_md4_h_warn };

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

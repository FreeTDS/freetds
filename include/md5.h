#ifndef MD5_H
#define MD5_H

static char rcsid_md5_h[] = "$Id: md5.h,v 1.1 2004-03-22 19:13:13 freddy77 Exp $";
static void *no_unused_md5_h_warn[] = { rcsid_md5_h, no_unused_md5_h_warn };

struct MD5Context {
	TDS_UINT buf[4];
	TDS_UINT bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(struct MD5Context *context, unsigned char *digest);
void MD5Transform(TDS_UINT buf[4], TDS_UINT const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#endif /* !MD5_H */

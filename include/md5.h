#ifndef MD5_H
#define MD5_H

/* $Id: md5.h,v 1.6 2010-01-25 23:05:58 freddy77 Exp $ */

#include <freetds/pushvis.h>

struct MD5Context {
	TDS_UINT buf[4];
	TDS_UINT8 bytes;
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf, size_t len);
void MD5Final(struct MD5Context *context, unsigned char *digest);
void MD5Transform(TDS_UINT buf[4], TDS_UINT const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#include <freetds/popvis.h>

#endif /* !MD5_H */

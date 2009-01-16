#ifndef MD5_H
#define MD5_H

/* $Id: md5.h,v 1.5 2009-01-16 20:27:56 jklowden Exp $ */

#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC visibility push(hidden)
#endif

struct MD5Context {
	TDS_UINT buf[4];
	TDS_UINT bits[2];
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

#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC visibility pop
#endif

#endif /* !MD5_H */

#ifndef MD5_H
#define MD5_H

#ifndef HAVE_NETTLE

#include <freetds/pushvis.h>

struct MD5Context {
	TDS_UINT buf[4];
	TDS_UINT8 bytes;
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf, size_t len);
void MD5Final(struct MD5Context *context, unsigned char *digest);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#include <freetds/popvis.h>

#else

#include <nettle/md5.h>

typedef struct md5_ctx MD5_CTX;

static inline void MD5Init(MD5_CTX *ctx)
{
	nettle_md5_init(ctx);
}

static inline void MD5Update(MD5_CTX *ctx, unsigned char const *buf, size_t len)
{
	nettle_md5_update(ctx, len, buf);
}

static inline void MD5Final(MD5_CTX *ctx, unsigned char *digest)
{
	nettle_md5_digest(ctx, 16, digest);
}

#endif

#endif /* !MD5_H */

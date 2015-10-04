#ifndef MD4_H
#define MD4_H

#ifndef HAVE_NETTLE

#include <freetds/pushvis.h>

struct MD4Context
{
	TDS_UINT buf[4];
	TDS_UINT8 bytes;
	unsigned char in[64];
};

void MD4Init(struct MD4Context *context);
void MD4Update(struct MD4Context *context, unsigned char const *buf, size_t len);
void MD4Final(struct MD4Context *context, unsigned char *digest);

typedef struct MD4Context MD4_CTX;

#include <freetds/popvis.h>

#else

#include <nettle/md4.h>

typedef struct md4_ctx MD4_CTX;

static inline void MD4Init(MD4_CTX *ctx)
{
	nettle_md4_init(ctx);
}

static inline void MD4Update(MD4_CTX *ctx, unsigned char const *buf, size_t len)
{
	nettle_md4_update(ctx, len, buf);
}

static inline void MD4Final(MD4_CTX *ctx, unsigned char *digest)
{
	nettle_md4_digest(ctx, 16, digest);
}


#endif

#endif /* !MD4_H */

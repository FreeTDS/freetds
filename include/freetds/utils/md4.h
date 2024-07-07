#ifndef _tdsguard_bJRBdagK4r9w58mnUbyvA0_
#define _tdsguard_bJRBdagK4r9w58mnUbyvA0_

#ifndef HAVE_NETTLE

#include <freetds/pushvis.h>

struct MD4Context
{
	uint32_t buf[4];
	uint64_t bytes;
	uint32_t in[16];
};

void MD4Init(struct MD4Context *context);
void MD4Update(struct MD4Context *context, const uint8_t *buf, size_t len);
void MD4Final(struct MD4Context *context, uint8_t *digest);

typedef struct MD4Context MD4_CTX;

#include <freetds/popvis.h>

#else

#include <nettle/md4.h>

typedef struct md4_ctx MD4_CTX;

static inline void MD4Init(MD4_CTX *ctx)
{
	nettle_md4_init(ctx);
}

static inline void MD4Update(MD4_CTX *ctx, const uint8_t *buf, size_t len)
{
	nettle_md4_update(ctx, len, buf);
}

static inline void MD4Final(MD4_CTX *ctx, uint8_t *digest)
{
	nettle_md4_digest(ctx, 16, digest);
}


#endif

#endif /* !_tdsguard_bJRBdagK4r9w58mnUbyvA0_ */

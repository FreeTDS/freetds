#ifndef _tdsguard_d0MZPmUZs0d3gpgxVUiFES_
#define _tdsguard_d0MZPmUZs0d3gpgxVUiFES_

#ifndef HAVE_NETTLE

#include <freetds/pushvis.h>

struct MD5Context {
	uint32_t buf[4];
	uint64_t bytes;
	uint32_t in[16];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, const uint8_t *buf, size_t len);
void MD5Final(struct MD5Context *context, uint8_t *digest);

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

static inline void MD5Update(MD5_CTX *ctx, const uint8_t *buf, size_t len)
{
	nettle_md5_update(ctx, len, buf);
}

static inline void MD5Final(MD5_CTX *ctx, uint8_t *digest)
{
	nettle_md5_digest(ctx, 16, digest);
}

#endif

#endif /* !_tdsguard_d0MZPmUZs0d3gpgxVUiFES_ */

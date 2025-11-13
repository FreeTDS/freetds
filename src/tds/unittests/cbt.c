#include "common.h"

static char *
bin2ascii(char *dest, const void *data, size_t len)
{
	char *s = dest;
	const unsigned char *src = (const unsigned char *) data;
	for (; len > 0; --len, s += 2)
		sprintf(s, "%02x", *src++);
	*s = 0;
	return dest;
}

static void
calc_cbt_from_tls_unique_test(const char *tls_unique, const char *cbt)
{
	unsigned char cbt_buf[16];
	char cbt_str[33];

	TDSRET rc = tds_calc_cbt_from_tls_unique((unsigned char *) tls_unique, strlen(tls_unique), cbt_buf);
	if (TDS_FAILED(rc)) {
		fprintf(stderr, "Failed to calculate CBT from TLS unique: %s\n", tls_unique);
		exit(1);
	}

	/* convert to hex string and compare */
	bin2ascii(cbt_str, cbt_buf, 16);
	if (strcasecmp(cbt_str, cbt) != 0) {
		fprintf(stderr, "Wrong calc_cbt_from_tls_unique(%s) -> %s expected %s\n", tls_unique, cbt_buf, cbt);
		exit(1);
	}
}

TEST_MAIN()
{
	calc_cbt_from_tls_unique_test("0123456789abcdef", "1eb7620a5e38cb1f50478b1690621a03");
	return 0;
}

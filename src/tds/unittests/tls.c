/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2023 Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Check check_hostname and tls-server-end-point hash functions
 */
#undef NDEBUG
#include "../tls.c"

#include "common.h"

#include <freetds/tds/data.h>

#include <freetds/bool.h>

#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)

/* This certificate has common name as "www.abc.com" and alternate names
   as "xyz.org", "127.0.0.1", "::2:3:4:5:6" and "192.168.127.1". */
static const char certificate[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIE0jCCA7qgAwIBAgIUbIV2n53RPAMttnVuFQlE9C0tPvAwDQYJKoZIhvcNAQEL\n"
"BQAwgYwxCzAJBgNVBAYTAlVLMQswCQYDVQQIDAJDQTESMBAGA1UEBwwJQ2FtYnJp\n"
"ZGdlMRMwEQYDVQQKDApFeGFtcGxlIENvMRAwDgYDVQQLDAd0ZWNob3BzMRMwEQYD\n"
"VQQDDApUZXN0aW5nIENBMSAwHgYJKoZIhvcNAQkBFhFjZXJ0c0BleGFtcGxlLmNv\n"
"bTAeFw0yMzA5MjYxOTI2MjZaFw0yNjA2MjExOTI2MjZaMIGLMQswCQYDVQQGEwJV\n"
"SzELMAkGA1UECAwCQ0ExEjAQBgNVBAcMCUNhbWJyaWRnZTERMA8GA1UECgwIRnJl\n"
"ZGlhbm8xEDAOBgNVBAsMB3RlY2hvcHMxFDASBgNVBAMMC3d3dy5hYmMuY29tMSAw\n"
"HgYJKoZIhvcNAQkBFhFjZXJ0c0BleGFtcGxlLmNvbTCCASIwDQYJKoZIhvcNAQEB\n"
"BQADggEPADCCAQoCggEBAMcXWlvCeX//9wxvaTP9qD1RaFYUhxOppC/+JDBHnn8Y\n"
"T9915OYzctoAoVrcThMsg5GNWTB0/OkXz0/IHgxJZ9HFFsTJSUFvVKSD2UrG2ypF\n"
"aSLdJOD2CpqNbrr0cNhIFfRBrJ7KC3F3PHKB7BoROiSCgTTz46Hx29fRLW3Rqxh0\n"
"tz/tj7Yt5vesqByWo5zj3vha/F4+eK1hNNuP93i8wkZIOPStWNOO2OQ/ULh8MZON\n"
"qpvJHw6NveDmVFIVGtutrA+5w30Wp2vUJI60erRSailsMpXFyElYdnYZ+24/hA7P\n"
"Hfx3v5cQ+DHF3+AKFU7G2bcS/kB48vLSZzDz82/5O88CAwEAAaOCASkwggElMAwG\n"
"A1UdEwEB/wQCMAAwCwYDVR0PBAQDAgXgMDAGA1UdEQQpMCeCB3h5ei5vcmeHBH8A\n"
"AAGHEAAAAAAAAAACAAMABAAFAAaHBMCofwEwHQYDVR0OBBYEFDWbwRVMZvyOL8oA\n"
"nVpuRW2xkeeoMIG2BgNVHSMEga4wgauhgZKkgY8wgYwxCzAJBgNVBAYTAlVLMQsw\n"
"CQYDVQQIDAJDQTESMBAGA1UEBwwJQ2FtYnJpZGdlMRMwEQYDVQQKDApFeGFtcGxl\n"
"IENvMRAwDgYDVQQLDAd0ZWNob3BzMRMwEQYDVQQDDApUZXN0aW5nIENBMSAwHgYJ\n"
"KoZIhvcNAQkBFhFjZXJ0c0BleGFtcGxlLmNvbYIUW7YAeQBh0HFi6VWbFh9+tG2F\n"
"8NAwDQYJKoZIhvcNAQELBQADggEBAL0SfWxEufOYqg9e3vnLJj5Jxv1arayEWHrt\n"
"hL64GmEw4DltxX2DXAlPnQvpMvYGV3ynnAdnvDaFlBceG0iZzu9ZQTw0bdB12L30\n"
"PETIYUN1uHPaIXA8cCtLFi0BNVIeGH8WYbOVEu0Kl7JX+WSbZqnC9+wqpGrQv578\n"
"Ml+EIP8L1ZLaJx7W1U+A/WW+xtWmpTnHVNyOAWdX3c+GE4kSYzsW+6D3Ha2EYAno\n"
"R46tD+akLPNKjYETaB+MU72xF7h4crpEqfOZx2WVwMKjRsZed33xG4kG3P8SItYI\n"
"UJdPxDazCiKJYJ/JlRLZTW7+LisH33QcRRmbtS7KHrBDXveDez8=\n"
"-----END CERTIFICATE-----\n";

/* certificates to test tls-server-end-point hashes */
static const char cert_ec_sha1[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIBfDCCASKgAwIBAgIUTB4hkHPG6X6hmMGVgA75sZhExmswCQYHKoZIzj0EATAT\n"
"MREwDwYDVQQDDAhjYnQtc2hhMTAgFw0yNjExMDgxOTM0NTlaGA8yMTI2MTAxNTE5\n"
"MzQ1OVowEzERMA8GA1UEAwwIY2J0LXNoYTEwWTATBgcqhkjOPQIBBggqhkjOPQMB\n"
"BwNCAAT8QpztFfiMMD4+9ZlEn0/2fe7rbn++Eo+EfCoW0sqg1lcNp6ot9FHy+acs\n"
"6AfxMojColVyN5somrKUf9Xld57Po1MwUTAdBgNVHQ4EFgQUJJqrBRFIcBUfZxLs\n"
"jwuooD+0O+swHwYDVR0jBBgwFoAUJJqrBRFIcBUfZxLsjwuooD+0O+swDwYDVR0T\n"
"AQH/BAUwAwEB/zAJBgcqhkjOPQQBA0kAMEYCIQDVKrdYfJgHbeIbRXRwmaWd68Dm\n"
"23yieag4m9/d6n8q+wIhALsFkdnxDh01KQgir7EnvIEpco9SAOfNDFZGmRiPmERx\n"
"-----END CERTIFICATE-----\n";

static const char cert_ec_sha384[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIBgTCCASegAwIBAgIUccs1UcJw3sAIshRnQ6GRRvQ3KRwwCgYIKoZIzj0EAwMw\n"
"FTETMBEGA1UEAwwKY2J0LXNoYTM4NDAgFw0yNjExMDgxOTM0NTlaGA8yMTI2MTAx\n"
"NTE5MzQ1OVowFTETMBEGA1UEAwwKY2J0LXNoYTM4NDBZMBMGByqGSM49AgEGCCqG\n"
"SM49AwEHA0IABPxCnO0V+IwwPj71mUSfT/Z97utuf74Sj4R8KhbSyqDWVw2nqi30\n"
"UfL5pyzoB/EyiMKiVXI3myiaspR/1eV3ns+jUzBRMB0GA1UdDgQWBBQkmqsFEUhw\n"
"FR9nEuyPC6igP7Q76zAfBgNVHSMEGDAWgBQkmqsFEUhwFR9nEuyPC6igP7Q76zAP\n"
"BgNVHRMBAf8EBTADAQH/MAoGCCqGSM49BAMDA0gAMEUCIQCb7jfifDbjBooSRdqx\n"
"Gh503SX8cudf1M4hnhYGhUerCgIgazwXYIGCYva3VVSg+497orPlh/cyaxPv7sk8\n"
"mo8+wuY=\n"
"-----END CERTIFICATE-----\n";

static const char cert_rsa_pss[] =
"-----BEGIN CERTIFICATE-----\n"
"MIICaDCCAZ2gAwIBAgIUIe+kCug5l3yAdzrbsxQOwMgBpU0wQQYJKoZIhvcNAQEK\n"
"MDSgDzANBglghkgBZQMEAgMFAKEcMBoGCSqGSIb3DQEBCDANBglghkgBZQMEAgMF\n"
"AKIDAgE+MBIxEDAOBgNVBAMMB2NidC1wc3MwIBcNMjYxMTA4MTkzNDU5WhgPMjEy\n"
"NjEwMTUxOTM0NTlaMBIxEDAOBgNVBAMMB2NidC1wc3MwgZ0wCwYJKoZIhvcNAQEK\n"
"A4GNADCBiQKBgQC80+3Qohg3uoQK7Lq+G9VckHho+Mst0ShgfaFCabcF3ky3IcTS\n"
"r9pPtKTaqJZfi4awPt1NNGWWfSSfygKyzsGYf2tEJmhTqwsY+yJobsjev8566Y7d\n"
"9IwGhnyyL1CZFEl0LYnVdqu7TiZZChObFflxCpTRuwO8m7pZsiivTehHhwIDAQAB\n"
"o1MwUTAdBgNVHQ4EFgQU2Un1P10dRbtPHikVP/cFrtTM/ZowHwYDVR0jBBgwFoAU\n"
"2Un1P10dRbtPHikVP/cFrtTM/ZowDwYDVR0TAQH/BAUwAwEB/zBBBgkqhkiG9w0B\n"
"AQowNKAPMA0GCWCGSAFlAwQCAwUAoRwwGgYJKoZIhvcNAQEIMA0GCWCGSAFlAwQC\n"
"AwUAogMCAT4DgYEAOnapoDfVEGatJZF3QrWMwZDcs2rK9YDQW2jEN5jjRzCiObIL\n"
"yyJdnEZX2HPB+uxOQzkMDapAeIWBHzioO25TB6MZQhbebAHSsBPQ+4mV/MHZOcKu\n"
"SDFIsWeITYShZkztCpV3upGrz/5t6HW3B+wiK8k/ZP5/qv54PIG2Xi67QYQ=\n"
"-----END CERTIFICATE-----\n";

static bool got_failure = false;

static void TDS_UNUSED
check_end_point_hash(const char *name, const unsigned char *hash, size_t hash_len, const char *expected)
{
	char hex[64 * 2 + 1];
	size_t i;

	for (i = 0; i < hash_len; ++i)
		sprintf(hex + i * 2, "%02x", hash[i]);
	hex[hash_len * 2] = 0;
	if (strcmp(hex, expected) != 0) {
		fprintf(stderr, "wrong tls-server-end-point hash for %s, got '%s' expected '%s'\n", name, hex, expected);
		got_failure = true;
	}
}

#if defined(HAVE_OPENSSL)
static void
test_hostname(X509 *cert, const char *hostname, bool expected)
{
	bool got = !!check_hostname(cert, hostname);
	if (got != expected) {
		fprintf(stderr, "unexpected result, got %d expected %d for '%s'\n", got, expected, hostname);
		got_failure = true;
	}
}

static X509 *
load_cert(const char *pem)
{
	BIO *bufio;
	X509 *cert;

	bufio = BIO_new_mem_buf((void *) pem, -1);
	if (!bufio) {
		fprintf(stderr, "error allocating BIO\n");
		exit(1);
	}
	cert = PEM_read_bio_X509(bufio, NULL, 0, NULL);
	BIO_free(bufio);
	if (!cert) {
		fprintf(stderr, "error creating certificate\n");
		exit(1);
	}
	return cert;
}

static void
test_end_point(const char *name TDS_UNUSED, const char *pem TDS_UNUSED, const char *expected TDS_UNUSED)
{
#ifdef TLS1_3_VERSION
	X509 *cert = load_cert(pem);
	unsigned char hash[EVP_MAX_MD_SIZE];
	size_t hash_len;

	hash_len = tds_ssl_end_point_hash(cert, hash, sizeof(hash));
	check_end_point_hash(name, hash, hash_len, expected);
	X509_free(cert);
#endif
}

static void
test_hostnames(void)
{
	X509 *cert;
	bool expected;

#define test(hostname) test_hostname(cert, hostname, expected)

	cert = load_cert(certificate);

	/* check valid names are accepted */
	expected = true;
	test("xyz.org");
	test("127.0.0.1");
	test("::2:3:4:5:6");
	test("192.168.127.1");
	test("www.abc.com");

	/* check some invalid names */
	expected = false;
	test("fail.com");
	test("127.0.0.2");
	test("::2:3:4:5:7");
	test("::1");
#undef test

	X509_free(cert);
}
#else
static void
test_end_point(const char *name TDS_UNUSED, const char *pem TDS_UNUSED, const char *expected TDS_UNUSED)
{
#if GNUTLS_VERSION_NUMBER >= 0x030603
	gnutls_x509_crt_t cert;
	gnutls_datum_t pem_datum, der;
	unsigned char hash[64];
	size_t hash_len;

	pem_datum.data = (unsigned char *) pem;
	pem_datum.size = strlen(pem);
	if (gnutls_x509_crt_init(&cert) < 0) {
		fprintf(stderr, "error allocating certificate\n");
		exit(1);
	}
	if (gnutls_x509_crt_import(cert, &pem_datum, GNUTLS_X509_FMT_PEM) < 0
	    || gnutls_x509_crt_export2(cert, GNUTLS_X509_FMT_DER, &der) < 0) {
		fprintf(stderr, "error loading certificate\n");
		exit(1);
	}
	gnutls_x509_crt_deinit(cert);

	hash_len = tds_ssl_end_point_hash(&der, hash, sizeof(hash));
	gnutls_free(der.data);
	check_end_point_hash(name, hash, hash_len, expected);
#endif
}

static void
test_hostnames(void)
{
}
#endif

TEST_MAIN()
{
	test_hostnames();

	/* tls-server-end-point uses the certificate signature hash... */
	test_end_point("RSA SHA-256", certificate, "dc3a5af024d5fdb147356a586bd281158b164e79fc5a44dccb6d71dacc3a896a");
	test_end_point("ECDSA SHA-384", cert_ec_sha384,
		       "8954cd89332ddb24789584fbd4ec16a1745fa67e374070a71f83e1cb40bc4fd83d6140cae8f16cfa03aa5f26cf480588");
	test_end_point("RSA-PSS SHA-512", cert_rsa_pss,
		       "c49c37cc97a9763988de1dd897a3a2990dcbfc4facc1c31cd0547f3fa6ac37bcf1f92d8b0e11aa57543009fdcbfd6496b479c132bdad15e8b069c3f060d54206");
	/* ... but SHA-1 is replaced by SHA-256 */
	test_end_point("ECDSA SHA-1", cert_ec_sha1, "cb8c2b74f14ab5c995ea781e0445fa869f18dded88d0ece12ff1c8940e76bf62");

	return got_failure ? 1 : 0;
}
#else
TEST_MAIN()
{
	return 0;
}
#endif

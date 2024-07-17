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
 * Check check_hostname function
 */
#undef NDEBUG
#include "../tls.c"

#include "common.h"

#include <freetds/data.h>

#if defined(HAVE_OPENSSL)

#include <freetds/bool.h>

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

static bool got_failure = false;

static void
test_hostname(X509 *cert, const char *hostname, bool expected)
{
	bool got = !!check_hostname(cert, hostname);
	if (got != expected) {
		fprintf(stderr, "unexpected result, got %d expected %d for '%s'\n", got, expected, hostname);
		got_failure = true;
	}
}

TEST_MAIN()
{
	BIO *bufio;
	X509 *cert;
	bool expected;
#define test(hostname) test_hostname(cert, hostname, expected)

	bufio = BIO_new_mem_buf((void*)certificate, -1);
	if (!bufio) {
		fprintf(stderr, "error allocating BIO\n");
		return 1;
	}
	cert = PEM_read_bio_X509(bufio, NULL, 0, NULL);
	if (!cert) {
		fprintf(stderr, "error creating certificate\n");
		return 1;
	}

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

	X509_free(cert);
	BIO_free(bufio);
	return got_failure ? 1 : 0;
}
#else
TEST_MAIN()
{
	return 0;
}
#endif

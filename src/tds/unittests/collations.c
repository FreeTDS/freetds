/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2015  Frediano Ziglio
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
#include "common.h"
#include <freetds/iconv.h>
#include <freetds/utils/md5.h>

#include <ctype.h>
#include <assert.h>

#if HAVE_UNISTD_H
#undef getpid
#include <unistd.h>
#elif defined(_WIN32)
# include <io.h>
# undef isatty
# define isatty(fd) _isatty(fd)
#endif /* HAVE_UNISTD_H */

static TDSSOCKET *tds;

static void
get_coll_md5(const char *name, char *digest)
{
	MD5_CTX ctx;
	char *p = NULL, *sql = NULL;
	int ret;
	TDSRET rc;
	TDS_INT result_type;
	int done_flags, i;
	unsigned char dig[16];

	ret = asprintf(&sql, "convert(nvarchar(1024), bin) collate %s", name);
	assert(ret >= 0);

	ret = asprintf(&p, "convert(varchar(4096), %s)", sql);
	assert(ret >= 0);
	free(sql);
	sql = NULL;

	ret = asprintf(&sql, "select convert(varbinary(8000), %s) from #all_chars order by id", p);
	assert(ret >= 0);
	free(p);
	p = NULL;

	rc = tds_submit_query(tds, sql);
	assert(rc == TDS_SUCCESS);
	free(sql);
	sql = NULL;

	MD5Init(&ctx);

	while ((rc = tds_process_tokens(tds, &result_type, &done_flags, TDS_RETURN_ROW)) == TDS_SUCCESS) {
		TDSCOLUMN *curcol;

		assert(result_type == TDS_ROW_RESULT);

		curcol = tds->current_results->columns[0];

		assert(!is_blob_col(curcol));
		assert(curcol->on_server.column_type == XSYBVARBINARY);

		MD5Update(&ctx, curcol->column_data, curcol->column_cur_size);
	}
	assert(rc == TDS_NO_MORE_RESULTS);

	memset(dig, 0, sizeof(dig));
	MD5Final(&ctx, dig);
	for (i = 0; i < 16; ++i)
		sprintf(digest + i * 2, "%02x", dig[i]);
}

static void
get_encoding_coll(TDS71_COLLATION *coll, char *digest, char *cp, const char *name)
{
	int rc;
	int done_flags;
	TDS_INT result_type;
	char sql[512];
	static TDS71_COLLATION old_coll = { 0, 0, 0};
	static char old_digest[33];

	sprintf(sql, "SELECT CAST(CAST('a' AS NVARCHAR(10)) COLLATE %s AS VARCHAR(10)) COLLATE %s", name, name);

	/* do a select and check all results */
	rc = tds_submit_query(tds, sql);
	assert(rc == TDS_SUCCESS);

	assert(tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS) == TDS_SUCCESS);

	if (result_type == TDS_DONE_RESULT)
		return;

	assert(result_type == TDS_ROWFMT_RESULT);

	assert(tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS) == TDS_SUCCESS);

	assert(result_type == TDS_ROW_RESULT);

	while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_STOPAT_ROWFMT|TDS_STOPAT_DONE|TDS_RETURN_ROW|TDS_RETURN_COMPUTE)) == TDS_SUCCESS) {

		TDSCOLUMN *curcol;
		TDS_UCHAR *c;

		if (result_type != TDS_ROW_RESULT)
			break;

		curcol = tds->current_results->columns[0];

		strcpy(cp, curcol->char_conv->to.charset.name);

		c = curcol->column_collation;
		coll->locale_id = c[0] + 256 * c[1];
		coll->flags = c[2] + 256 * c[3];
		coll->charset_id = c[4];
	}

	if (rc != TDS_SUCCESS || result_type == TDS_ROW_RESULT || result_type == TDS_COMPUTE_RESULT) {
		fprintf(stderr, "tds_process_tokens() unexpected return\n");
		exit(1);
	}

	while ((rc = tds_process_tokens(tds, &result_type, &done_flags, TDS_TOKEN_RESULTS)) == TDS_SUCCESS) {
		switch (result_type) {
		case TDS_NO_MORE_RESULTS:
			return;

		case TDS_DONE_RESULT:
		case TDS_DONEPROC_RESULT:
		case TDS_DONEINPROC_RESULT:
			if (!(done_flags & TDS_DONE_ERROR))
				break;

		default:
			fprintf(stderr, "tds_proces_tokens() unexpected result_type\n");
			exit(1);
			break;
		}
	}

	get_coll_md5(name, digest);

	memcpy(old_digest, digest, 33);
	memcpy(&old_coll, coll, sizeof(*coll));
}

static void
test_column_encoding(void)
{
	FILE *f = fopen("collations.txt", "r");
	char line[1024];

	assert(f);
	while (fgets(line, sizeof(line), f)) {
		TDS71_COLLATION coll;
		char cp[128], digest[33];
		char *s = strtok(line, " \n");

		if (!s[0])
			continue;

		memset(&coll, 0, sizeof(coll));
		cp[0] = 0;
		digest[0] = 0;

		get_encoding_coll(&coll, digest, cp, s);
		printf("%s %04x %04x %d %s %s\n", s, coll.locale_id, coll.flags, coll.charset_id, cp, digest);
	}
	fclose(f);
}

static void
add_couple(unsigned n)
{
	enum { CHUNK = 512 };
	static char buf[CHUNK * 4+1];
	static int cnt = 0;
	static int id = 0;

	char *sql = NULL;
	int ret;

	sprintf(buf + cnt * 4, "%02x%02x", n & 0xff, (n >> 8) & 0xff);

	/* time to insert the command into database ? */
	if (++cnt != CHUNK)
		return;

	cnt = 0;
	++id;
	ret = asprintf(&sql, "insert into #all_chars values(%d, convert(varbinary(2048), 0x%s))", id, buf);
	assert(ret >= 0);

	if (isatty(fileno(stdout))) {
		printf("\rInserting: %d", id);
		fflush(stdout);
	}

	ret = run_query(tds, sql);
	assert(ret == TDS_SUCCESS);

	free(sql);
}

static void
add_plane0(void)
{
	unsigned n;
	for (n = 0; n < 65536; ++n)
		add_couple(n);
}

static void
add_couples(void)
{
	unsigned n;
	for (n = 0; n <= 0xfffff; ++n) {
		add_couple( (n >> 10) + 0xd800 );
		add_couple( (n & 0x3ff) + 0xdc00 );
	}
}

static void
prepare_all_chars(void)
{
	int ret;

	ret = run_query(tds, "CREATE TABLE #all_chars(id int, bin varbinary(2048))");
	assert(ret == TDS_SUCCESS);

	add_plane0();
	add_couples();
	printf("\n");
}

static void
extract_collations(void)
{
	TDS_INT result_type;
	int done_flags;
	TDSRET rc;
	FILE *f;

	f = fopen("collations.txt", "w");
	assert(f);

	rc = tds_submit_query(tds, "select name from ::fn_helpcollations() order by name");
	assert(rc == TDS_SUCCESS);

	while ((rc = tds_process_tokens(tds, &result_type, &done_flags, TDS_RETURN_ROW)) == TDS_SUCCESS) {
		TDSCOLUMN *curcol;
		int len;

		assert(result_type == TDS_ROW_RESULT);

		curcol = tds->current_results->columns[0];

		assert(!is_blob_col(curcol));
		assert(curcol->column_type == SYBVARCHAR);

		len = curcol->column_cur_size;
		fprintf(f, "%*.*s\n", len, len, curcol->column_data);
	}
	assert(rc == TDS_NO_MORE_RESULTS);

	fclose(f);
}

TEST_MAIN()
{
	TDSLOGIN *login;
	int ret;
	int verbose = 0;

	/* use UTF-8 as our coding */
	strcpy(common_pwd.charset, "UTF-8");

	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCESS) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	if (IS_TDS71_PLUS(tds->conn)) {
		printf("Preparing table with all characters\n");
		prepare_all_chars();

		printf("Extracting collation list\n");
		extract_collations();

		printf("Testing encodings\n");
		test_column_encoding();
	}

	try_tds_logout(login, tds, verbose);
	return 0;
}

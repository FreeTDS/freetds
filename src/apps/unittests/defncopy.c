/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2024  Frediano Ziglio
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

/**
 * This tests execute some command using tsql and defncopy to check behaviour
 */

#include <freetds/utils/test_base.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <process.h>
#define EXE_SUFFIX ".exe"
#define SDIR_SEPARATOR "\\"
#else
#define EXE_SUFFIX ""
#define SDIR_SEPARATOR "/"
#endif

#include <freetds/bool.h>
#include <freetds/macros.h>
#include <freetds/sysdep_private.h>

/* content of output file, from command executed */
static char *output;

static bool
read_login_info(void)
{
	return read_login_info_base(&common_pwd, DEFAULT_PWD_PATH) != NULL;
}

static void
no_space(void)
{
	fprintf(stderr, "No space left on buffer\n");
	exit(1);
}

static void
normalize_spaces(char *s)
{
	char *p, *dest, prev;

	/* replace all tabs with spaces */
	for (p = s; *p; ++p)
		if (*p == '\t')
			*p = ' ';

	/* replace duplicate spaces with a single space */
	prev = 'x';
	for (dest = s, p = s; *p; ++p) {
		if (prev == ' ' && *p == ' ')
			continue;
		*dest++ = prev = *p;
	}
	*dest = 0;
}

/* read a file and output on a stream */
static void
cat(const char *fn, FILE *out)
{
	char line[1024];
	FILE *f = fopen(fn, "r");
	assert(f);
	while (fgets(line, sizeof(line), f)) {
		fputs("  ", out);
		fputs(line, out);
	}
	fclose(f);
}

/* read a text file into memory, return it as a string */
static char *
read_file(const char *fn)
{
	long pos;
	char *buf;
	size_t readed, size;

	FILE *f = fopen(fn, "r");
	assert(f);
	assert(fseek(f, 0, SEEK_END) == 0);
	pos = ftell(f);
	assert(pos >= 0 && pos <= 0x1000000);
	size = (size_t) pos;
	assert(fseek(f, 0, SEEK_SET) == 0);
	buf = malloc(size + 10); /* allocate some more space */
	assert(buf);
	readed = fread(buf, 1, size + 1ul, f);
	assert(readed <= size);
	assert(feof(f));
	fclose(f);
	buf[readed] = 0;
	return buf;
}

#define CHECK(n) do {\
	if (dest + (n) > dest_end) \
		no_space(); \
} while(0)

static char *
quote_arg(char *dest, char *dest_end, const char *arg)
{
#ifndef _WIN32
	CHECK(1);
	*dest++ = '\'';
	for (; *arg; ++arg) {
		if (*arg == '\'') {
			CHECK(3);
			strcpy(dest, "'\\'");
			dest += 3;
		}
		CHECK(1);
		*dest++ = *arg;
	}
	CHECK(1);
	*dest++ = '\'';
#else
	CHECK(1);
	*dest++ = '\"';
	for (; *arg; ++arg) {
		if (*arg == '\\' || *arg == '\"') {
			CHECK(1);
			*dest++ = '\\';
		}
		CHECK(1);
		*dest++ = *arg;
	}
	CHECK(1);
	*dest++ = '\"';
#endif
	return dest;
}

static char *
add_string(char *dest, char *const dest_end, const char *str)
{
	size_t len = strlen(str);
	CHECK(len);
	memcpy(dest, str, len);
	return dest + len;
}

#undef CHECK

static char *
add_server(char *dest, char *const dest_end)
{
	dest = add_string(dest, dest_end, " -S ");
	dest = quote_arg(dest, dest_end, common_pwd.server);
	dest = add_string(dest, dest_end, " -U ");
	dest = quote_arg(dest, dest_end, common_pwd.user);
	dest = add_string(dest, dest_end, " -P ");
	dest = quote_arg(dest, dest_end, common_pwd.password);
	if (common_pwd.database[0]) {
		dest = add_string(dest, dest_end, " -D ");
		dest = quote_arg(dest, dest_end, common_pwd.database);
	}
	return dest;
}

static void
cleanup(void)
{
	unlink("empty");
	unlink("output");
	unlink("input");
	TDS_ZERO_FREE(output);
}

static void
tsql(const char *input_data)
{
	char cmd[2048];
	char *const end = cmd + sizeof(cmd) - 1;
	char *p;
	FILE *f;

	f = fopen("input", "w");
	assert(f);
	fputs(input_data, f);
	fclose(f);

	strcpy(cmd, ".." SDIR_SEPARATOR "tsql" EXE_SUFFIX " -o q");
	p = strchr(cmd, 0);
	p = add_server(p, end);
	p = add_string(p, end, "<input >output");
	*p = 0;
	printf("Executing: %s\n", cmd);
	if (system(cmd) != 0) {
		printf("Output is:\n");
		cat("output", stdout);
		fprintf(stderr, "Failed command\n");
		exit(1);
	}
	TDS_ZERO_FREE(output);
	output = read_file("output");
}

static void
defncopy(const char *object_name)
{
	char cmd[2048];
	char *const end = cmd + sizeof(cmd) - 1;
	char *p;
	FILE *f;

	// empty input
	f = fopen("input", "w");
	assert(f);
	fclose(f);

	strcpy(cmd, ".." SDIR_SEPARATOR "defncopy" EXE_SUFFIX);
	p = strchr(cmd, 0);
	p = add_server(p, end);
	p = add_string(p, end, " ");
	p = quote_arg(p, end, object_name);
	p = add_string(p, end, "<input >output");
	*p = 0;
	printf("Executing: %s\n", cmd);
	if (system(cmd) != 0) {
		printf("Output is:\n");
		cat("output", stdout);
		fprintf(stderr, "Failed command\n");
		exit(1);
	}
	TDS_ZERO_FREE(output);
	output = read_file("output");
}

/* table with a column name that is also a keyword, should be quoted */
static void
test_keyword(void)
{
	const char *sql;
	static const char clean[] =
		"IF OBJECT_ID('dbo.table_with_column_named_key') IS NOT NULL DROP TABLE dbo.table_with_column_named_key\n";

	tsql(clean);
	tsql(
"IF OBJECT_ID('dbo.table_with_column_named_key') IS NOT NULL DROP TABLE dbo.table_with_column_named_key\n"
"GO\n"
"CREATE TABLE dbo.table_with_column_named_key\n"
"(\n"
"  [key]        nvarchar(4000)  NOT NULL\n"
")\n");
	defncopy("dbo.table_with_column_named_key");
	cat("output", stdout);
	normalize_spaces(output);
	sql =
"CREATE TABLE [dbo].[table_with_column_named_key]\n"
" ( [key] nvarchar(4000) NOT NULL\n"
" )\n"
"GO";
	if (strstr(output, sql) == NULL) {
		fprintf(stderr, "Expected SQL string not found\n");
		exit(1);
	}
	tsql(clean);
	tsql(sql);
	tsql(clean);
}

/* table with an index with a space inside */
static void
test_index_name_with_space(void)
{
	const char *sql;
	static const char clean[] =
		"IF OBJECT_ID('dbo.tblReportPeriod') IS NOT NULL DROP TABLE dbo.tblReportPeriod\n";

	tsql(clean);
	tsql(
"CREATE TABLE dbo.tblReportPeriod\n"
"	( RecordID   int             NOT NULL\n"
"	, FromDate   nvarchar(40)        NULL\n"
"	, ToDate     nvarchar(40)        NULL\n"
"	)\n"
"CREATE  nonclustered INDEX [From Date] on dbo.tblReportPeriod(FromDate)\n");
	defncopy("dbo.tblReportPeriod");
	cat("output", stdout);
	normalize_spaces(output);
	sql =
"CREATE TABLE [dbo].[tblReportPeriod]\n"
" ( [RecordID] int NOT NULL\n"
" , [FromDate] nvarchar(40) NULL\n"
" , [ToDate] nvarchar(40) NULL\n"
" )\n"
"GO\n"
"\n"
"CREATE nonclustered INDEX [From Date] on [dbo].[tblReportPeriod]([FromDate])";
	if (strstr(output, sql) == NULL) {
		fprintf(stderr, "Expected SQL string not found\n");
		exit(1);
	}
	tsql(clean);
	tsql(sql);
	tsql(clean);
}

/* table with an index with a space inside */
static void
test_weird_index_names(void)
{
	const char *sql, *sql_sybase;
	static const char clean[] =
		"IF OBJECT_ID('dbo.tblReportPeriod2') IS NOT NULL DROP TABLE dbo.tblReportPeriod2\n";

	tsql(clean);
	tsql(
"CREATE TABLE dbo.tblReportPeriod2\n"
"	( RecordID   int             NOT NULL\n"
"	, [To, ]   nvarchar(40)        NULL\n"
"	, [To]     nvarchar(40)        NULL\n"
"	, [To, , ]     nvarchar(40)        NULL\n"
"	)\n"
"CREATE  nonclustered INDEX [From Date] on dbo.tblReportPeriod2([To, ],[To, , ])\n");
	defncopy("dbo.tblReportPeriod2");
	cat("output", stdout);
	normalize_spaces(output);
	sql =
"CREATE TABLE [dbo].[tblReportPeriod2]\n"
" ( [RecordID] int NOT NULL\n"
" , [To, ] nvarchar(40) NULL\n"
" , [To] nvarchar(40) NULL\n"
" , [To, , ] nvarchar(40) NULL\n"
" )\n"
"GO\n"
"\n"
"CREATE nonclustered INDEX [From Date] on [dbo].[tblReportPeriod2]([To, ],[To, , ])";
	/* Sybase remove spaces at the end */
	sql_sybase =
"CREATE TABLE [dbo].[tblReportPeriod2]\n"
" ( [RecordID] int NOT NULL\n"
" , [To,] nvarchar(40) NULL\n"
" , [To] nvarchar(40) NULL\n"
" , [To, ,] nvarchar(40) NULL\n"
" )\n"
"GO\n"
"\n"
"CREATE nonclustered INDEX [From Date] on [dbo].[tblReportPeriod2]([To,],[To, ,])";
	if (strstr(output, sql) == NULL && strstr(output, sql_sybase) == NULL) {
		fprintf(stderr, "Expected SQL string not found\n");
		exit(1);
	}
	tsql(clean);
	tsql(sql);
	tsql(clean);
}

TEST_MAIN()
{
	FILE *f;

	cleanup();

	if (!read_login_info())
		return 1;

	f = fopen("empty", "w");
	if (f)
		fclose(f);

	test_keyword();
	test_index_name_with_space();
	test_weird_index_names();

	cleanup();
	return 0;
}

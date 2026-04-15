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

#include "common.h"

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <freetds/sysdep_private.h>
#include <freetds/replacements.h>

/* content of output file, from command executed */
static char *output;

static bool
read_login_info(void)
{
	return read_login_info_base(&common_pwd, DEFAULT_PWD_PATH) != NULL;
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
defncopy(const char *object_name)
{
	char cmd[2048];
	char *const end = cmd + sizeof(cmd) - 1;
	char *p;
	FILE *f;

	/* empty input */
	f = fopen("input", "w");
	assert(f);
	fclose(f);

	strcpy(cmd, "defncopy" EXE_SUFFIX);
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
	update_path();

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

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
#include <freetds/utils.h>

/* Sybase mode using a different terminator */
#define TERMINATOR "GO"
#define TERMINATOR_SYBASE "/* ### DEFNCOPY: END OF DEFINITION */"

/* content of output file, from command executed */
static char *output;

/* Whether using sybase syntax */
static bool sybase_syntax;
static bool stream_io = true;

static bool
read_login_info(void)
{
	return read_login_info_base(&common_pwd, DEFAULT_PWD_PATH) != NULL;
}

static void
cleanup(void)
{
	unlink("empty");
	unlink(output_fn());
	unlink(input_fn());
	TDS_ZERO_FREE(output);
}

/* check expected output, ignoring whitespace diffs */
static bool
output_matches(char const *expect)
{
	bool result;
	char *tmp;

	if (!expect)
		return false;
	normalize_spaces(output);
	tmp = xstrdup(expect);
	normalize_spaces(tmp);
	result = strstr(output, tmp) != NULL;
	free(tmp);
	return result;
}

static void
required_output(char const *expect)
{
	if (!output_matches(expect)) {
		fprintf(stderr, "Expected SQL string not found\n");
		exit(EXIT_FAILURE);
	}
}

static void
defncopy(const char *object_name)
{
	char cmd[2048] = { 0 };
	char *const end = cmd + sizeof(cmd) - 1;
	char *p;
	bool redirs = stream_io && !sybase_syntax;

	strcat(cmd, VMS_APP_EXE_INVOKE "defncopy" EXE_SUFFIX);
	p = strchr(cmd, 0);

	if (sybase_syntax) {
		p = add_login(p, end);
		p = add_string(p, end, object_name ? " out " : " in ");
		p = add_string(p, end, object_name ? output_fn() : input_fn());
		if (common_pwd.database[0]) {
			p = add_string(p, end, " ");
			p = quote_arg(p, end, common_pwd.database);
		}
	} else {
		p = add_server(p, end);
		if (!stream_io) {
			p = add_string(p, end, object_name ? " -o " : " -i ");
			p = add_string(p, end, object_name ? output_fn() : input_fn());
		}
	}

	if (object_name) {
		p = add_string(p, end, " ");
		p = quote_arg(p, end, object_name);
	}

	/* FreeTDS defncopy syntax supports both standard streams, and input/output files.
	 */
	if (redirs) {
		p = add_string(p, end, object_name ? " >" : " <");
		p = add_string(p, end, object_name ? output_fn() : input_fn());
	}
	*p = 0;

	printf("Executing: %s\n", cmd);
	if (system(cmd) != 0) {
		if (object_name) {
			printf("Output is:\n");
			cat(output_fn(), stdout);
		}
		fprintf(stderr, "Failed command\n");
		exit(EXIT_FAILURE);
	}

	TDS_ZERO_FREE(output);
	if (object_name)
		output = read_file(output_fn());
}

static void
defncopy_in(char const *sql)
{
	FILE *f;

	f = fopen(input_fn(), "w");
	assert(f);
	fputs(sql, f);
	fclose(f);
	defncopy(NULL);
}

/* table with a column name that is also a keyword, should be quoted */
static void
test_keyword(void)
{
	const char *sql;
	static const char clean[] =
		"IF OBJECT_ID('dbo.table_with_column_named_key') IS NOT NULL DROP TABLE dbo.table_with_column_named_key\n";

	tsql(clean);
/* *INDENT-OFF* */
	tsql(
"IF OBJECT_ID('dbo.table_with_column_named_key') IS NOT NULL DROP TABLE dbo.table_with_column_named_key\n"
"GO\n"
"CREATE TABLE dbo.table_with_column_named_key\n"
"(\n"
"  [key]        nvarchar(4000)  NOT NULL\n"
")\n");
/* *INDENT-ON* */
	defncopy("dbo.table_with_column_named_key");
	cat(output_fn(), stdout);
#define EXPECT_KEYWORD_OUT "CREATE TABLE [dbo].[table_with_column_named_key]\n ( [key] nvarchar(4000) NOT NULL\n )\n"
	sql = sybase_syntax ? EXPECT_KEYWORD_OUT TERMINATOR_SYBASE "\n" : EXPECT_KEYWORD_OUT TERMINATOR "\n";

	required_output(sql);
	tsql(clean);

	/* test defncopy input */
	defncopy_in(sql);
	unlink(output_fn());
	defncopy("dbo.table_with_column_named_key");
	required_output(sql);
	tsql(clean);

	/* test non-stream syntax */
	stream_io = false;
	defncopy_in(sql);
	unlink(output_fn());
	defncopy("dbo.table_with_column_named_key");
	required_output(sql);
	tsql(clean);
	stream_io = true;

	tsql(sql);
	tsql(clean);
}

/* table with an index with a space inside */
static void
test_index_name_with_space(void)
{
	const char *sql;
	static const char clean[] = "IF OBJECT_ID('dbo.tblReportPeriod') IS NOT NULL DROP TABLE dbo.tblReportPeriod\n";

	tsql(clean);
/* *INDENT-OFF* */
	tsql(
"CREATE TABLE dbo.tblReportPeriod\n"
"	( RecordID   int             NOT NULL\n"
"	, FromDate   nvarchar(40)        NULL\n"
"	, ToDate     nvarchar(40)        NULL\n"
"	)\n"
"CREATE  nonclustered INDEX [From Date] on dbo.tblReportPeriod(FromDate)\n");
/* *INDENT-ON* */
	defncopy("dbo.tblReportPeriod");
	cat(output_fn(), stdout);
/* *INDENT-OFF* */
	sql =
"CREATE TABLE [dbo].[tblReportPeriod]\n"
" ( [RecordID] int NOT NULL\n"
" , [FromDate] nvarchar(40) NULL\n"
" , [ToDate] nvarchar(40) NULL\n"
" )\n"
"CREATE nonclustered INDEX [From Date] on [dbo].[tblReportPeriod]([FromDate]) "
TERMINATOR
;
/* *INDENT-ON* */
	required_output(sql);
	tsql(clean);
	tsql(sql);
	tsql(clean);
}

/* table with an index with a space inside */
static void
test_weird_index_names(void)
{
	const char *sql;
	static const char clean[] = "IF OBJECT_ID('dbo.tblReportPeriod2') IS NOT NULL DROP TABLE dbo.tblReportPeriod2\n";

	tsql(clean);
/* *INDENT-OFF* */
	tsql(
"CREATE TABLE dbo.tblReportPeriod2\n"
"	( RecordID   int             NOT NULL\n"
"	, [To, ]   nvarchar(40)        NULL\n"
"	, [To]     nvarchar(40)        NULL\n"
"	, [To, , ]     nvarchar(40)        NULL\n"
"	)\n"
"CREATE  nonclustered INDEX [From Date] on dbo.tblReportPeriod2([To, ],[To, , ])\n");
/* *INDENT-ON* */
	defncopy("dbo.tblReportPeriod2");
	cat(output_fn(), stdout);

/* *INDENT-OFF* */
	sql =
"CREATE TABLE [dbo].[tblReportPeriod2]\n"
" ( [RecordID] int NOT NULL\n"
" , [To, ] nvarchar(40) NULL\n"
" , [To] nvarchar(40) NULL\n"
" , [To, , ] nvarchar(40) NULL\n"
" )\n"
"CREATE nonclustered INDEX [From Date] on [dbo].[tblReportPeriod2]([To, ],[To, ,])\n"
TERMINATOR
;
/* *INDENT-ON* */
	required_output(sql);
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

	sybase_syntax = true;
	test_keyword();
	sybase_syntax = false;
	test_keyword();
	test_index_name_with_space();
	test_weird_index_names();

	cleanup();
	return 0;
}

/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2026  Frediano Ziglio
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
 * This tests execute some command using tsql and freebcp to check behaviour
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

static bool
read_login_info(void)
{
	return read_login_info_base(&common_pwd, DEFAULT_PWD_PATH) != NULL;
}

static void
cleanup(void)
{
	unlink(output_fn());
	unlink(input_fn());
	unlink("error");
}

static void
freebcp(const char *object_name, const char *input_data, const char *added_options)
{
	char cmd[2048];
	char *const end = cmd + sizeof(cmd) - 1;
	char *p;
	FILE *f;

	/* empty input */
	f = fopen(input_fn(), "w");
	assert(f);
	fputs(input_data, f);
	fclose(f);

	strcpy(cmd, "freebcp" EXE_SUFFIX);
	p = strchr(cmd, 0);
	p = add_string(p, end, " ");
	p = quote_arg(p, end, object_name);
	p = add_string(p, end, " in ");
	p = add_string(p, end, input_fn());
	p = add_string(p, end, " ");
	p = add_string(p, end, added_options);
	p = add_server(p, end);
	*p = 0;
	printf("Executing: %s\n", cmd);
	if (system(cmd) != 0) {
		fprintf(stderr, "Failed command\n");
		exit(1);
	}
	unlink(input_fn());
}

static void
compare_error(const char *content)
{
	char line[1024];
	FILE *const f = fopen("error", "r");
	char *const lines = strdup(content);
	char *tokens = lines;
	int num_line = 0;

	assert(f);
	assert(lines);
	for (num_line = 1; fgets(line, sizeof(line), f); ++num_line) {
		size_t len = strlen(line);
		char *expected;

		/* Skip comments */
		if (strncmp(line, "#@ ", 3) == 0)
			continue;
		if (len > 4 && strcmp(line + len - 3, " @#") == 0)
			continue;

		/* chomp */
		if (len > 0 && line[len - 1] == '\n')
			line[--len] = 0;

		/* compare with expected line */
		expected = strsep(&tokens, "\n");
		if (!expected) {
			fprintf(stderr, "%d: Unexpected lines: '%s'\n", num_line, line);
			exit(1);
		}
		if (strcmp(line, expected) != 0) {
			fprintf(stderr, "%d: Wrong line\n exp: '%s'\n got: '%s'\n", num_line, expected, line);
			exit(1);
		}
	}
	free(lines);
	fclose(f);
}

static void
test_error(void)
{
	static const char clean[] = "IF OBJECT_ID('bcp_in') IS NOT NULL DROP TABLE bcp_in\n";

	tsql(clean);
	tsql("CREATE TABLE bcp_in(num INT NOT NULL)\n");
	freebcp("bcp_in", "123\nerror\nother error\n", "-c -e error ");
	compare_error("error\n\nother error\n\n");
	tsql(clean);
}

TEST_MAIN()
{
	cleanup();
	update_path();

	if (!read_login_info())
		return 1;

	test_error();

	cleanup();
	return 0;
}

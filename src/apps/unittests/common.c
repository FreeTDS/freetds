/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2024-2026  Frediano Ziglio
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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <freetds/sysdep_private.h>
#include <freetds/utils/path.h>

static void
no_space(void)
{
	fprintf(stderr, "No space left on buffer\n");
	exit(1);
}

void
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

void
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
char *
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
	buf = malloc(size + 10);	/* allocate some more space */
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

char *
quote_arg(char *dest, char *const dest_end, const char *arg)
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

char *
add_string(char *dest, char *const dest_end, const char *str)
{
	size_t len = strlen(str);

	CHECK(len);
	memcpy(dest, str, len);
	return dest + len;
}

#undef CHECK

char *
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

void
update_path(void)
{
	static const tds_dir_char name[] = TDS_DIR("PATH");
	tds_dir_char *path = tds_dir_getenv(name);

#ifndef _WIN32
	int len;

	if (!path) {
		setenv(name, "..", 1);
		return;
	}

	len = asprintf(&path, "..:%s", path);
	assert(len > 0);
	setenv(name, path, 1);
#else
	const tds_dir_char *only = L"PATH=..";
	const tds_dir_char *start = L"PATH=..;%s";
	tds_dir_char *p;
	size_t len;

#ifdef CMAKE_INTDIR
	if (CMAKE_INTDIR[0]) {
		only = L"PATH=..\\" TDS_DIR(CMAKE_INTDIR);
		start = L"PATH=..\\" TDS_DIR(CMAKE_INTDIR) L";%s";
	}
#endif

	if (!path) {
		_wputenv(only);
		return;
	}

	len = tds_dir_len(path) + 100;
	p = tds_new(tds_dir_char, len);
	assert(p);
	tds_dir_snprintf(p, len, start, path);
	path = p;
	_wputenv(path);
#endif
	free(path);
}

static char *
tsql_generic(const char *input_data, bool get_output)
{
	char cmd[2048];
	char *const end = cmd + sizeof(cmd) - 1;
	char *p;
	char *output = NULL;
	FILE *f;
	bool success;

	f = fopen(input_fn(), "w");
	assert(f);
	fputs(input_data, f);
	fclose(f);

	strcpy(cmd, "tsql" EXE_SUFFIX " -o q");
	p = strchr(cmd, 0);
	p = add_server(p, end);
	p = add_string(p, end, "<");
	p = add_string(p, end, input_fn());
	p = add_string(p, end, " >");
	p = add_string(p, end, output_fn());
	*p = 0;
	printf("Executing: %s\n", cmd);
	success = (system(cmd) == 0);
	unlink(input_fn());
	if (!success) {
		printf("Output is:\n");
		cat(output_fn(), stdout);
		unlink(output_fn());
		fprintf(stderr, "Failed command\n");
		exit(1);
	}
	if (get_output)
		output = read_file(output_fn());
	unlink(output_fn());
	return output;
}

void
tsql(const char *input_data)
{
	tsql_generic(input_data, false);
}

char *
tsql_out(const char *input_data)
{
	return tsql_generic(input_data, true);
}

static const char *
get_fn(char *buf, const char *prefix)
{
	if (!buf[0])
		sprintf(buf, "%s.%d", prefix, (int) getpid());
	return buf;
}

const char *
input_fn(void)
{
	static char buf[32] = { 0 };

	return get_fn(buf, "input");
}

const char *
output_fn(void)
{
	static char buf[32] = { 0 };

	return get_fn(buf, "output");
}

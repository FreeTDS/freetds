/* Base tests utilities
 * Copyright (C) 2025 Aaron M. Ucko
 * Copyright (C) 2025 Frediano Ziglio
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

#include <freetds/utils/test_base.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#  include <freetds/windows.h>
#  include <crtdbg.h>
#  include <wchar.h>

static LONG WINAPI
seh_handler(EXCEPTION_POINTERS* ep TDS_UNUSED)
{
	/* Always terminate the test. */
	return EXCEPTION_EXECUTE_HANDLER;
}

static void
suppress_diag_popup_messages(void)
{
	/* Check environment variable for silent abort app at error */
	const char* value = getenv("DIAG_SILENT_ABORT");
	if (value  &&  (*value == 'Y'  ||  *value == 'y')) {
		/* Windows GPF errors */
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
			     SEM_NOOPENFILEERRORBOX);

		/* Runtime library */
		_set_error_mode(_OUT_TO_STDERR);

		/* Debug library */
		_CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);

		/* Exceptions(!) */
		SetUnhandledExceptionFilter(seh_handler);
	}
}

static WCHAR*
tds_wcsrstr(const WCHAR *haystack, const WCHAR *needle)
{
	WCHAR *res, *p;

	res = wcsstr(haystack, needle);
	while (res && (p = wcsstr(res + 1, needle)) != NULL)
		res = p;
	return res;
}

/**
 * Get path of executable and update PATH to include library
 */
static void
update_path(void)
{
	static const struct {
		const WCHAR *dir;
		const char *dll;
	} *dir, dirs[] = {
		{ L"replacements", "replacements.lib" },
		{ L"utils", "tdsutils.lib" },
		{ L"tds", "tds.lib" },
		{ L"ctlib", "ct.dll" },
		{ L"dblib", "sybdb.dll" },
		{ L"odbc", "tdsodbc.dll" },
		{ L"apps", "tsql.exe" },
		{ NULL, NULL }
	};

	WCHAR fn[MAX_PATH], copy[MAX_PATH], *p;
	WCHAR *name, *unit, *src, *env;
	DWORD attributes;

	/* GetModuleFileHandle for process */
	assert(GetModuleFileNameW(NULL, fn, TDS_VECTOR_SIZE(fn)));

	for (p = fn; *p; ++p)
		if (*p == L'/')
			*p = L'\\';

	/* split directory */
	name = wcsrchr(fn, L'\\');
	assert(name);
	*name = 0;

	/* test existence (a bit of paranoia here) */
	attributes = GetFileAttributesW(fn);
	assert(attributes != INVALID_FILE_ATTRIBUTES);
	assert((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

	/* terminate with \ */
	wcscpy(name, L"\\");

	/* copy and convert to lower */
	wcscpy(copy, fn);
	for (p = copy; *p; ++p)
		if (*p >= L'A' && *p <= L'Z')
			*p += (L'a' - L'A');

	unit = tds_wcsrstr(copy, L"\\unittests\\");
	assert(unit && unit > copy);
	*unit = 0;
	src = tds_wcsrstr(copy, L"\\src\\");
	assert(src && src > copy);
	assert(wcschr(src + 5, L'\\') == NULL);
	*name = 0;
	memmove(fn + (unit - copy), fn + (unit -copy) + 10, wcslen(unit + 9) * sizeof(WCHAR));
	memmove(unit, unit + 10, wcslen(unit + 9) * sizeof(WCHAR));
	*unit = 0;

	for (dir = dirs; dir->dir; ++dir) {
		if (wcscmp(src + 5, dir->dir) != 0)
			continue;
		break;
	}
	assert(dir->dir);
	*unit = L'\\';

/*
 * find \src\ctlib\, \src\dblib\ and so on
 * understand where the DLL is
 */

	/* set PATH accordingly */
	env = _wgetenv(L"PATH");
	assert(env);
	p = tds_new(WCHAR, wcslen(fn) + wcslen(env) + 2);
	assert(p);
	_swprintf(p, L"%s;%s", fn, env);
	env = p;

	SetEnvironmentVariableW(L"PATH", env);
	free(env);
}
#endif

int
main(int argc, char ** argv)
{
#ifdef _WIN32
	suppress_diag_popup_messages();
	update_path();
#endif
	return test_main(argc, argv);
}


COMMON_PWD common_pwd;

const char *
read_login_info_base(COMMON_PWD *common_pwd, const char *default_path)
{
	const char *ret = try_read_login_info_base(common_pwd, default_path);

	if (!ret)
		fprintf(stderr, "Cannot open PWD file %s\n\n", default_path);
	return ret;
}

const char *
try_read_login_info_base(COMMON_PWD *common_pwd, const char *default_path)
{
	FILE *in = NULL;
	char line[512];
	char *s1, *s2;
	const char *path;

	if (common_pwd->initialized)
		return default_path;

	if (!common_pwd->tried_env) {
		common_pwd->tried_env = true;
		s1 = getenv("TDSPWDFILE");
		if (s1 && s1[0]) {
			in = fopen(s1, "r");
			if (in)
				path = s1;
		}
	}
	if (!in) {
		in = fopen(default_path, "r");
		if (in) {
			path = default_path;
		} else {
			return NULL;
		}
	}

	while (fgets(line, sizeof(line), in)) {
		s1 = strtok(line, "=");
		s2 = strtok(NULL, "\n");
		if (!s1 || !s2) {
			continue;
		}
		switch (s1[0]) {
		case 'U':
			if (!common_pwd->user[0] && !strcmp(s1, "UID"))
				strcpy(common_pwd->user, s2);
			break;
		case 'S':
			if (!common_pwd->server[0] && !strcmp(s1, "SRV"))
				strcpy(common_pwd->server, s2);
			break;
		case 'P':
			if (!common_pwd->password[0] && !strcmp(s1, "PWD"))
				strcpy(common_pwd->password, s2);
			break;
		case 'D':
			if (!common_pwd->database[0] && !strcmp(s1, "DB"))
				strcpy(common_pwd->database, s2);
			break;
		default:
			break;
		}
	}
	fclose(in);
	common_pwd->initialized = true;
	return path;
}

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

#ifdef _WIN32
#  include <crtdbg.h>
#  include <windows.h>

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
#endif

int
main(int argc, char ** argv)
{
#ifdef _WIN32
	suppress_diag_popup_messages();
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

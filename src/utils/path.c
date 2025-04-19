/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2023  Frediano Ziglio
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

#include <config.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/windows.h>
#include <freetds/macros.h>
#include <freetds/sysdep_private.h>
#include <freetds/utils/path.h>

/**
 * Return filename from HOME directory
 * @return allocated string or NULL if error
 */
tds_dir_char *
tds_get_home_file(const tds_dir_char *file)
{
	tds_dir_char *home, *path;

	home = tds_get_homedir();
	if (!home)
		return NULL;
	path = tds_join_path(home, file);
	free(home);
	return path;
}

tds_dir_char*
tds_join_path(const tds_dir_char *dir, const tds_dir_char *file)
{
	tds_dir_char *ret;

	ret = tds_new(tds_dir_char, tds_dir_len(dir) + tds_dir_len(file) + 4);
	if (!ret)
		return ret;

	if (dir[0] == '\0') {
		ret[0] = '\0';
	} else
#ifndef _WIN32
	{
		strcpy(ret, dir);
		strcat(ret, TDS_SDIR_SEPARATOR);
	}
	strcat(ret, file);
#else
	{
		wcscpy(ret, dir);
		wcscat(ret, TDS_SDIR_SEPARATOR);
	}
	wcscat(ret, file);
#endif
	return ret;
}

#ifdef _WIN32
tds_dir_char *
tds_dir_from_cstr(const char *path)
{
	/* include NUL terminator so output string will be terminated and MultiByteToWideChar won't
	 * return 0 on succesful empty strings */
	size_t len = strlen(path) + 1;
	tds_dir_char *res = tds_new(tds_dir_char, len);
	if (res) {
		/* first try UTF-8 */
		int out_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, len, res, len);
		/* if it fails try current CP setting */
		if (!out_len)
			out_len = MultiByteToWideChar(CP_ACP, 0, path, len, res, len);
		if (out_len <= 0 || out_len > len)
			TDS_ZERO_FREE(res);
		else
			/* ensure NUL terminated (pretty paranoid) */
			res[out_len - 1] = 0;
	}
	return res;
}
#endif

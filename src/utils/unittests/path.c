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

/*
 * Purpose: test path utilities.
 */

#include <freetds/utils/test_base.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/macros.h>
#include <freetds/sysdep_private.h>
#include <freetds/utils/path.h>

#ifdef _WIN32
enum { is_windows = 1 };
#else
enum { is_windows = 0 };
#endif

TEST_MAIN()
{
	tds_dir_char *path;
	size_t len;
	FILE *f;
	char c_buf[64];
	tds_dir_char path_buf[64];

	/* tds_dir_len */
	len = tds_dir_len(TDS_DIR("this is a filename"));
	assert(len == 18);

	/* tds_dir_dup, tds_dir_cmp */
	path = tds_dir_dup(TDS_DIR("filename"));
	assert(path);
	assert(tds_dir_cmp(path, TDS_DIR("filename")) == 0);
	free(path);

	/* tds_join_path, tds_dir_cmp */
	path = tds_join_path(TDS_DIR("a") , TDS_DIR("b"));
	assert(path);
	assert(tds_dir_cmp(path, is_windows ? TDS_DIR("a\\b") : TDS_DIR("a/b")) == 0);
	free(path);

	/* tds_join_path, tds_dir_cmp */
	path = tds_join_path(TDS_DIR("") , TDS_DIR("b"));
	assert(path);
	assert(tds_dir_cmp(path, TDS_DIR("b")) == 0);
	free(path);

	/* tds_dir_open */
	unlink("path.txt");
	f = fopen("path.txt", "r");
	assert(f == NULL);
	f = tds_dir_open(TDS_DIR("path.txt"), TDS_DIR("w"));
	assert(f != NULL);
	fclose(f);
	f = fopen("path.txt", "r");
	assert(f != NULL);
	fclose(f);
	unlink("path.txt");

	/* tdsPRIdir */
	snprintf(c_buf, sizeof(c_buf), "x%" tdsPRIdir "y", TDS_DIR("abc"));
	assert(strcmp(c_buf, "xabcy") == 0);

	/* tds_dir_snprintf */
	tds_dir_snprintf(path_buf, TDS_VECTOR_SIZE(path_buf), TDS_DIR("x%sy"), TDS_DIR("abc"));
	assert(tds_dir_cmp(path_buf, TDS_DIR("xabcy")) == 0);

	/* tds_dir_from_cstr */
	path = tds_dir_from_cstr("filename");
	assert(tds_dir_cmp(path, TDS_DIR("filename")) == 0);
	free(path);

	/* specific to Windows implementation */
#ifdef _WIN32
	/* utf-8 */
	path = tds_dir_from_cstr("abc\xc2\xab");
	assert(tds_dir_cmp(path, L"abc\u00ab") == 0);
	free(path);

	/* no utf-8 */
	path = tds_dir_from_cstr("abc\xab");
	assert(tds_dir_cmp(path, L"abc\u00ab") == 0 || tds_dir_cmp(path, L"abc?") == 0);
	free(path);
#endif

	return 0;
}


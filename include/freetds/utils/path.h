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

#ifndef _tdsguard_eI8iNo9FExd6aRlc3im79S_
#define _tdsguard_eI8iNo9FExd6aRlc3im79S_

#ifdef _WIN32
#include <wchar.h>
#endif

#include <freetds/pushvis.h>

#ifdef _WIN32
#define TDS_SDIR_SEPARATOR L"\\"
typedef wchar_t tds_dir_char;
#define tds_dir_open _wfopen
#define tds_dir_getenv _wgetenv
#define tds_dir_dup _wcsdup
#define tds_dir_len wcslen
#define tds_dir_cmp wcscmp
#define tds_dir_snprintf _snwprintf
#define TDS_DIR(s) L ## s
#define tdsPRIdir "ls"
tds_dir_char *tds_dir_from_cstr(const char *path);
#else
#define TDS_SDIR_SEPARATOR "/"
typedef char tds_dir_char;
#define tds_dir_open fopen
#define tds_dir_getenv getenv
#define tds_dir_dup strdup
#define tds_dir_len strlen
#define tds_dir_cmp strcmp
#define tds_dir_snprintf snprintf
#define TDS_DIR(s) s
#define tdsPRIdir "s"
#define tds_dir_from_cstr(s) strdup(s)
#endif

tds_dir_char *tds_get_homedir(void);
tds_dir_char* tds_join_path(const tds_dir_char *dir, const tds_dir_char *file);
tds_dir_char *tds_get_home_file(const tds_dir_char *file);

#include <freetds/popvis.h>

#endif /* _tdsguard_eI8iNo9FExd6aRlc3im79S_ */

/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2014  Frediano Ziglio
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

#ifndef _freetds_data_h_
#define _freetds_data_h_

#ifndef _tds_h_
# error Include tds.h before data.h
#endif

#include <freetds/pushvis.h>

#define TDS_COMMON_FUNCS(name) \
{ \
	tds_ ## name ## _get_info, \
	tds_ ## name ## _get, \
	tds_ ## name ## _row_len, \
	tds_ ## name ## _put_info_len, \
	tds_ ## name ## _put_info, \
	tds_ ## name ## _put, \
	TDS_EXTRA_CHECK(tds_ ## name ## _check) \
}

tds_func_get_info tds_invalid_get_info;
tds_func_row_len  tds_invalid_row_len;
tds_func_get_data tds_invalid_get;
tds_func_put_info_len tds_invalid_put_info_len;
tds_func_put_info tds_invalid_put_info;
tds_func_put_data tds_invalid_put;
tds_func_check    tds_invalid_check;

tds_func_get_info tds_generic_get_info;
tds_func_row_len  tds_generic_row_len;
tds_func_get_data tds_generic_get;
tds_func_put_info_len tds_generic_put_info_len;
tds_func_put_info tds_generic_put_info;
tds_func_put_data tds_generic_put;
tds_func_check    tds_generic_check;

tds_func_get_info tds_numeric_get_info;
tds_func_row_len  tds_numeric_row_len;
tds_func_get_data tds_numeric_get;
tds_func_put_info_len tds_numeric_put_info_len;
tds_func_put_info tds_numeric_put_info;
tds_func_put_data tds_numeric_put;
tds_func_check    tds_numeric_check;

#define tds_variant_get_info tds_generic_get_info
#define tds_variant_row_len  tds_generic_row_len
tds_func_get_data tds_variant_get;
#define tds_variant_put_info_len tds_generic_put_info_len
tds_func_put_info tds_variant_put_info;
tds_func_put_data tds_variant_put;
tds_func_check    tds_variant_check;

tds_func_get_info tds_msdatetime_get_info;
tds_func_row_len  tds_msdatetime_row_len;
tds_func_get_data tds_msdatetime_get;
#define tds_msdatetime_put_info_len tds_generic_put_info_len
tds_func_put_info tds_msdatetime_put_info;
tds_func_put_data tds_msdatetime_put;
tds_func_check    tds_msdatetime_check;

tds_func_get_info tds_clrudt_get_info;
tds_func_row_len  tds_clrudt_row_len;
#define tds_clrudt_get tds_generic_get
#define tds_clrudt_put_info_len tds_generic_put_info_len
tds_func_put_info tds_clrudt_put_info;
#define tds_clrudt_put tds_generic_put
tds_func_check    tds_clrudt_check;

tds_func_get_info tds_sybbigtime_get_info;
tds_func_row_len  tds_sybbigtime_row_len;
tds_func_get_data tds_sybbigtime_get;
tds_func_put_info_len tds_sybbigtime_put_info_len;
tds_func_put_info tds_sybbigtime_put_info;
tds_func_put_data tds_sybbigtime_put;
tds_func_check    tds_sybbigtime_check;

/**
 * If TDS_DONT_DEFINE_DEFAULT_FUNCTIONS is no defined
 * define default implementations for these tables
 */
#ifndef TDS_DONT_DEFINE_DEFAULT_FUNCTIONS
#  define TDS_DEFINE_DEFAULT_FUNCS(name) \
	const TDSCOLUMNFUNCS tds_ ## name ## _funcs = TDS_COMMON_FUNCS(name)

TDS_DEFINE_DEFAULT_FUNCS(invalid);
TDS_DEFINE_DEFAULT_FUNCS(generic);
TDS_DEFINE_DEFAULT_FUNCS(numeric);
TDS_DEFINE_DEFAULT_FUNCS(variant);
TDS_DEFINE_DEFAULT_FUNCS(msdatetime);
TDS_DEFINE_DEFAULT_FUNCS(clrudt);
TDS_DEFINE_DEFAULT_FUNCS(sybbigtime);
#endif

#include <freetds/popvis.h>

#endif

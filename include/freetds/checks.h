/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2004 Frediano Ziglio
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

#ifndef _tdsguard_ejjdEEiHR3F7xWi9vVKOIa_
#define _tdsguard_ejjdEEiHR3F7xWi9vVKOIa_

#include <freetds/pushvis.h>

#if ENABLE_EXTRA_CHECKS
#define CHECK_STRUCT_EXTRA(func,s) func(s)
#else
#define CHECK_STRUCT_EXTRA(func,s)
#endif

#define CHECK_TDS_EXTRA(tds)              CHECK_STRUCT_EXTRA(tds_check_tds_extra,tds)
#define CHECK_CONTEXT_EXTRA(ctx)          CHECK_STRUCT_EXTRA(tds_check_context_extra,ctx)
#define CHECK_TDSENV_EXTRA(env)           CHECK_STRUCT_EXTRA(tds_check_env_extra,env)
#define CHECK_COLUMN_EXTRA(column)        CHECK_STRUCT_EXTRA(tds_check_column_extra,column)
#define CHECK_RESULTINFO_EXTRA(res_info)  CHECK_STRUCT_EXTRA(tds_check_resultinfo_extra,res_info)
#define CHECK_PARAMINFO_EXTRA(res_info)   CHECK_STRUCT_EXTRA(tds_check_resultinfo_extra,res_info)
#define CHECK_CURSOR_EXTRA(cursor)        CHECK_STRUCT_EXTRA(tds_check_cursor_extra,cursor)
#define CHECK_DYNAMIC_EXTRA(dynamic)      CHECK_STRUCT_EXTRA(tds_check_dynamic_extra,dynamic)
#define CHECK_FREEZE_EXTRA(freeze)        CHECK_STRUCT_EXTRA(tds_check_freeze_extra,freeze)
#define CHECK_CONN_EXTRA(conn)

#if ENABLE_EXTRA_CHECKS
void tds_check_tds_extra(const TDSSOCKET * tds);
void tds_check_context_extra(const TDSCONTEXT * ctx);
void tds_check_env_extra(const TDSENV * env);
void tds_check_column_extra(const TDSCOLUMN * column);
void tds_check_resultinfo_extra(const TDSRESULTINFO * res_info);
void tds_check_cursor_extra(const TDSCURSOR * cursor);
void tds_check_dynamic_extra(const TDSDYNAMIC * dynamic);
void tds_check_freeze_extra(const TDSFREEZE * freeze);
#endif

#if defined(HAVE_VALGRIND_MEMCHECK_H) && ENABLE_EXTRA_CHECKS
#  include <valgrind/memcheck.h>
#  define TDS_MARK_UNDEFINED(ptr, len) VALGRIND_MAKE_MEM_UNDEFINED(ptr, len)
#else
#  define TDS_MARK_UNDEFINED(ptr, len) do {} while(0)
#endif

#if ENABLE_EXTRA_CHECKS
void tds_extra_assert_check(const char *fn, int line, int cond, const char *cond_str);
#  define tds_extra_assert(cond) \
	tds_extra_assert_check(__FILE__, __LINE__, cond, #cond)
#else
#  define tds_extra_assert(cond) do { } while(0)
#endif

#include <freetds/popvis.h>

#endif /* _tdsguard_ejjdEEiHR3F7xWi9vVKOIa_ */

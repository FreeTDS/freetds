/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002  Brian Bruns
 * Copyright (C) 2004, 2005 Frediano Ziglio
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

#ifndef ODBC_CHECKS_H
#define ODBC_CHECKS_H

/* $Id: odbc_checks.h,v 1.5 2005-02-08 12:14:14 freddy77 Exp $ */

#if ENABLE_EXTRA_CHECKS
/* macro */
#define CHECK_ENV_EXTRA(env) odbc_check_env_extra(env)
#define CHECK_DBC_EXTRA(dbc) odbc_check_dbc_extra(dbc)
#define CHECK_STMT_EXTRA(stmt) odbc_check_stmt_extra(stmt)
#define CHECK_DESC_EXTRA(desc) odbc_check_desc_extra(desc)
/* declarations*/
void odbc_check_env_extra(TDS_ENV * env);
void odbc_check_dbc_extra(TDS_DBC * dbc);
void odbc_check_stmt_extra(TDS_STMT * stmt);
void odbc_check_desc_extra(TDS_DESC * desc);
#else
/* macro */
#define CHECK_ENV_EXTRA(env)
#define CHECK_DBC_EXTRA(dbc)
#define CHECK_STMT_EXTRA(stmt)
#define CHECK_DESC_EXTRA(desc)
#endif

#endif /* ODBC_CHECKS_H */

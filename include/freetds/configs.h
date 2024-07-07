/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#ifndef _tdsguard_eQj9hBZh39rorFpOUns5xS_
#define _tdsguard_eQj9hBZh39rorFpOUns5xS_

#include <freetds/utils/path.h>

#ifndef _WIN32
#include <freetds/sysconfdir.h>
#else
#define FREETDS_SYSCONFDIR TDS_DIR("c:")
#endif

#ifndef _tdsguard_hfOrWb5znoUCWdBPoNQvqN_
#error freetds/tds.h must be included before freetds/configs.h
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

#define FREETDS_SYSCONFFILE FREETDS_SYSCONFDIR TDS_SDIR_SEPARATOR TDS_DIR("freetds.conf")
#define FREETDS_POOLCONFFILE FREETDS_SYSCONFDIR TDS_SDIR_SEPARATOR TDS_DIR("pool.conf")
#define FREETDS_LOCALECONFFILE FREETDS_SYSCONFDIR TDS_SDIR_SEPARATOR TDS_DIR("locales.conf")

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif /* _tdsguard_eQj9hBZh39rorFpOUns5xS_ */

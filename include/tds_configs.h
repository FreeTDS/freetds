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

#ifndef _tds_configs_h_
#define _tds_configs_h_

static char rcsid_tds_configs_h[] = "$Id: tds_configs.h,v 1.19 2004-05-02 07:30:40 freddy77 Exp $";
static void *no_unused_tds_configs_h_warn[] = { rcsid_tds_configs_h, no_unused_tds_configs_h_warn };

#include "freetds_sysconfdir.h"

#ifndef _tds_h_
#error tds.h must be included before tds_configs.h
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

#define FREETDS_SYSCONFFILE FREETDS_SYSCONFDIR TDS_SDIR_SEPARATOR "freetds.conf"
#define FREETDS_POOLCONFFILE FREETDS_SYSCONFDIR TDS_SDIR_SEPARATOR "pool.conf"
#define FREETDS_LOCALECONFFILE FREETDS_SYSCONFDIR TDS_SDIR_SEPARATOR "locales.conf"

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif /* _tds_configs_h_ */

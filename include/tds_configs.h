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

static char rcsid_tds_configs_h[]=
	"$Id: tds_configs.h,v 1.14 2002-10-13 17:52:28 castellano Exp $";
static void *no_unused_tds_configs_h_warn[] = {
	rcsid_tds_configs_h,
	no_unused_tds_configs_h_warn};

#ifdef __cplusplus
extern "C" {
#endif

#define FREETDS_SYSCONFFILE FREETDS_SYSCONFDIR ## "freetds.conf"
#define FREETDS_POOLCONFFILE FREETDS_SYSCONFDIR ## "pool.conf"
#define FREETDS_LOCALECONFFILE FREETDS_SYSCONFDIR ## "locales.conf"

#ifdef __cplusplus
}
#endif 

#endif /* _tds_configs_h_ */

/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002  Brian Bruns
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

#ifndef SQL2TDS_H
#define SQL2TDS_H

/* $Id: sql2tds.h,v 1.10 2004-12-08 20:30:06 freddy77 Exp $ */

SQLRETURN sql2tds(TDS_DBC * dbc, const struct _drecord *drec_ipd, const struct _drecord *drec_apd, TDSPARAMINFO * info, int nparam,
		  int compute_row);

#endif /* SQL2TDS_H */

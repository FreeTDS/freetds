/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
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

#ifndef CONNECTPARAMS_H
#define CONNECTPARAMS_H

/* $Id: connectparams.h,v 1.17 2005-02-09 16:15:16 jklowden Exp $ */

/**
 * Parses a connection string for SQLDriverConnect().
 * \param connect_string      point to connection string
 * \param connect_string_end  point to end of connection string
 * \param connection          structure where to store informations
 * \return 0 if error, 1 otherwise
 */
int odbc_parse_connect_string(const char *connect_string, const char *connect_string_end, TDSCONNECTION * connection);

int odbc_get_dsn_info(const char *DSN, TDSCONNECTION * connection);

#endif

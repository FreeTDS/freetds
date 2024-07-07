/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008 Frediano Ziglio
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

#ifndef _tdsguard_cKNGa1szdpreKWqKpKKcAA_
#define _tdsguard_cKNGa1szdpreKWqKpKKcAA_

#include <freetds/pushvis.h>
void hmac_md5(const unsigned char key[16],
              const unsigned char* data, size_t data_len,
              unsigned char* digest);
#include <freetds/popvis.h>

#endif

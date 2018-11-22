/* TDSPool - Connection pooling for TDS based databases
 * Copyright (C) 2001 Brian Bruns
 * Copyright (C) 2005 Frediano Ziglio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "pool.h"
#include <freetds/configs.h>

#define POOL_STR_SERVER	"server"
#define POOL_STR_PORT	"port"
#define POOL_STR_USER	"user"
#define POOL_STR_PASSWORD	"password"
#define POOL_STR_DATABASE	"database"
#define POOL_STR_SERVER_USER	"server user"
#define POOL_STR_SERVER_PASSWORD	"server password"
#define POOL_STR_MAX_MBR_AGE	"max member age"
#define POOL_STR_MAX_POOL_CONN	"max pool conn"
#define POOL_STR_MIN_POOL_CONN	"min pool conn"
#define POOL_STR_MAX_POOL_USERS	"max pool users"

typedef struct {
	TDS_POOL *pool;
	char **err;
} conf_params;

static void pool_parse(const char *option, const char *value, void *param);
static bool pool_read_conf_file(const char *path, const char *poolname, conf_params *params);

bool
pool_read_conf_files(const char *path, const char *poolname, TDS_POOL * pool, char **err)
{
	bool found = false;
	conf_params params = { pool, err };

	if (path && !found)
		return pool_read_conf_file(path, poolname, &params);

	if (!found) {
		char *path = tds_get_home_file(".pool.conf");
		if (path) {
			found = pool_read_conf_file(path, poolname, &params);
			free(path);
		}
	}

	if (!found)
		found = pool_read_conf_file(FREETDS_POOLCONFFILE, poolname, &params);

	return found;
}

static bool
pool_read_conf_file(const char *path, const char *poolname, conf_params *params)
{
	FILE *in;
	bool found = false;

	in = fopen(path, "r");
	if (in) {
		tdsdump_log(TDS_DBG_INFO1, "Found conf file %s reading sections\n", path);
		tds_read_conf_section(in, "global", pool_parse, params);
		rewind(in);
		found = tds_read_conf_section(in, poolname, pool_parse, params);
		fclose(in);
	}

	return found;
}

/**
 * Parse an unsigned number, returns -1 on error.
 * Returns a signed int to make possible to return negative
 * values for include numbers (we don't need big numbers)
 */
static int
pool_get_uint(const char *value)
{
	char *end;
	unsigned long int val;

	errno = 0;
	val = strtoul(value, &end, 0);
	if (errno != 0 || end == value || val > INT_MAX)
		return -1;
	return (int) val;
}

static void
pool_parse(const char *option, const char *value, void *param)
{
	conf_params *params = (conf_params *) param;
	TDS_POOL *pool = params->pool;
	int val = 0;

	if (!strcmp(option, POOL_STR_PORT)) {
		val = pool_get_uint(value);
		if (val < 1 || val >= 65536)
			val = -1;
		pool->port = val;
	} else if (!strcmp(option, POOL_STR_SERVER)) {
		free(pool->server);
		pool->server = strdup(value);
	} else if (!strcmp(option, POOL_STR_USER)) {
		free(pool->user);
		pool->user = strdup(value);
	} else if (!strcmp(option, POOL_STR_DATABASE)) {
		free(pool->database);
		pool->database = strdup(value);
	} else if (!strcmp(option, POOL_STR_PASSWORD)) {
		free(pool->password);
		pool->password = strdup(value);
	} else if (!strcmp(option, POOL_STR_SERVER_USER)) {
		free(pool->server_user);
		pool->server_user = strdup(value);
	} else if (!strcmp(option, POOL_STR_SERVER_PASSWORD)) {
		free(pool->server_password);
		pool->server_password = strdup(value);
	} else if (!strcmp(option, POOL_STR_MAX_MBR_AGE)) {
		val = pool_get_uint(value);
		pool->max_member_age = val;
	} else if (!strcmp(option, POOL_STR_MAX_POOL_CONN)) {
		val = pool_get_uint(value);
		pool->max_open_conn = val;
	} else if (!strcmp(option, POOL_STR_MIN_POOL_CONN)) {
		val = pool_get_uint(value);
		pool->min_open_conn = val;
	}
	if (val < 0) {
		free(*params->err);
		if (asprintf(params->err, "Invalid value '%s' specified for %s", value, option) < 0)
			*params->err = "Memory error parsing options";
	}
}

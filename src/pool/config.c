/* TDSPool - Connection pooling for TDS based databases
 * Copyright (C) 2001 Brian Bruns
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "pool.h"
#include <tds_configs.h>
#include "tdsutil.h"

static char  software_version[]   = "$Id: config.c,v 1.4 2002-09-27 03:09:53 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


#define POOL_STR_SERVER	"server"
#define POOL_STR_PORT	"port"
#define POOL_STR_USER	"user"
#define POOL_STR_PASSWORD	"password"
#define POOL_STR_DATABASE	"database"
#define POOL_STR_MAX_MBR_AGE	"max member age"
#define POOL_STR_MAX_POOL_CONN	"max pool conn"
#define POOL_STR_MIN_POOL_CONN	"min pool conn"
#define POOL_STR_MAX_POOL_USERS	"max pool users"

static int pool_read_conf_sections(FILE *in, char *poolname, TDS_POOL *pool);
static int pool_read_conf_section(FILE *in, char *section, TDS_POOL *pool);

int pool_read_conf_file(char *poolname, TDS_POOL *pool)
{
FILE *in;
int found = 0; 

	in = fopen(FREETDS_POOLCONFFILE, "r");
	if (in) {
		fprintf(stderr, "Found conf file in %s reading sections\n",FREETDS_POOLCONFFILE);
		found = pool_read_conf_sections(in, poolname, pool);
		fclose(in);
	}
	
	return found;
}
static int pool_read_conf_sections(FILE *in, char *poolname, TDS_POOL *pool)
{
char *section;
int i, found = 0;

	pool_read_conf_section(in, "global", pool);
	rewind(in);
	section = strdup(poolname);
	for (i=0;i<strlen(section);i++) section[i]=tolower(section[i]);
	found = pool_read_conf_section(in, section, pool);
	free(section);

	return found;
}

#if 0
static int pool_config_boolean(char *value) 
{
	if (!strcmp(value, "yes") ||
		!strcmp(value, "on") ||
		!strcmp(value, "true") ||
		!strcmp(value, "1")) {
		return 1;
	} else {
		return 0;
	}
}
#endif

static int pool_read_conf_section(FILE *in, char *section, TDS_POOL *pool)
{
char line[256], option[256], value[256], *s;
int i;
char p;
int insection = 0;
int found = 0;

	while (fgets(line, 256, in)) {
		s = line;

		/* skip leading whitespace */
		while (*s && isspace(*s)) s++;

		/* skip it if it's a comment line */
		if (*s==';' || *s=='#') continue;

		/* read up to the = ignoring duplicate spaces */
		p = 0; i = 0;
		while (*s && *s!='=') {
			if (!isspace(*s) && isspace(p)) 
				option[i++]=' ';
			if (!isspace(*s)) 
				option[i++]=tolower(*s);
			p = *s;
			s++;
		}
		option[i]='\0';

		/* skip the = */
		if(*s) s++;

		/* skip leading whitespace */
		while (*s && isspace(*s)) s++;

		/* read up to a # ; or null ignoring duplicate spaces */
		p = 0; i = 0;
		while (*s && *s!=';' && *s!='#') {
			if (!isspace(*s) && isspace(p)) 
				value[i++]=' ';
			if (!isspace(*s)) 
				value[i++]=tolower(*s);
			p = *s;
			s++;
		}
		value[i]='\0';
		
		if (!strlen(option)) 
			continue;

		if (option[0]=='[') {
			s = &option[1];
			while (*s) {
				if (*s==']') *s='\0';
				s++;
			}
			if (!strcmp(section, &option[1])) {
				tdsdump_log(TDS_DBG_INFO1, "%L Found matching section\n");
				insection=1;
				found=1;
			} else {
				insection=0;
			}
		} else if (insection) {
			/* fprintf(stderr,"option = '%s' value = '%s'\n", option, value); */
			if (!strcmp(option,POOL_STR_PORT)) {
				if (atoi(value)) 
					pool->port = atoi(value);
			} else if (!strcmp(option,POOL_STR_SERVER)) {
				if (pool->server) free(pool->server);
				pool->server = strdup(value);
			} else if (!strcmp(option,POOL_STR_USER)) {
				if (pool->user) free(pool->user);
				pool->user = strdup(value);
			} else if (!strcmp(option,POOL_STR_DATABASE)) {
				if (pool->database) free(pool->database);
				pool->database = strdup(value);
			} else if (!strcmp(option,POOL_STR_PASSWORD)) {
				if (pool->password) free(pool->password);
				pool->password = strdup(value);
			} else if (!strcmp(option,POOL_STR_MAX_MBR_AGE)) {
				if (atoi(value)) 
					pool->max_member_age = atoi(value);
			} else if (!strcmp(option,POOL_STR_MAX_POOL_CONN)) {
				if (atoi(value)) 
					pool->max_open_conn = atoi(value);
			} else if (!strcmp(option,POOL_STR_MIN_POOL_CONN)) {
				if (atoi(value)) 
					pool->min_open_conn = atoi(value);
			}
		}

	}
	return found;
}

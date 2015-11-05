/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011  Frediano Ziglio
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <assert.h>
#include <ctype.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_LIMITS_H
#include <limits.h>
#endif 

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#ifdef _WIN32
#include <process.h>
#endif

#include "tds.h"
#include "tds_configs.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: config.c,v 1.162.2.3 2011-08-12 16:29:36 freddy77 Exp $");

static void tds_config_login(TDSCONNECTION * connection, TDSLOGIN * login);
static void tds_config_env_tdsdump(TDSCONNECTION * connection);
static void tds_config_env_tdsver(TDSCONNECTION * connection);
static void tds_config_env_tdsport(TDSCONNECTION * connection);
static void tds_config_env_tdshost(TDSCONNECTION * connection);
static int tds_read_conf_sections(FILE * in, const char *server, TDSCONNECTION * connection);
static int tds_read_interfaces(const char *server, TDSCONNECTION * connection);
static int parse_server_name_for_port(TDSCONNECTION * connection, TDSLOGIN * login);
static int tds_lookup_port(const char *portname);
static void tds_config_encryption(const char * value, TDSCONNECTION * connection);

static char *interf_file = NULL;

#define TDS_ISSPACE(c) isspace((unsigned char ) (c))

#if !defined(_WIN32) && !defined(DOS32X)
       const char STD_DATETIME_FMT[] = "%b %e %Y %I:%M%p";
static const char pid_config_logpath[] = "/tmp/tdsconfig.log.%d";
static const char freetds_conf[] = "%s/etc/freetds.conf";
static const char location[] = "(from $FREETDS/etc)";
static const char pid_logpath[] = "/tmp/freetds.log.%d";
static const char interfaces_path[] = "/etc/freetds";
#else
       const char STD_DATETIME_FMT[] = "%b %d %Y %I:%M%p"; /* msvcr80.dll does not support %e */
static const char pid_config_logpath[] = "c:\\tdsconfig.log.%d";
static const char freetds_conf [] = "%s\\freetds.conf";
static const char location[] = "(from $FREETDS)";
static const char pid_logpath[] = "c:\\freetds.log.%d";
static const char interfaces_path[] = "c:\\";
#endif

int
tds_default_port(int major, int minor)
{
	switch(major) {
	case 4:
		if (minor == 6)
			break;
	case 5:
		return 4000;
	}
	return 1433;
}

/**
 * \ingroup libtds
 * \defgroup config Configuration
 * Handle reading of configuration
 */

/**
 * \addtogroup config
 * @{ 
 */

/**
 * tds_read_config_info() will fill the tds connection structure based on configuration 
 * information gathered in the following order:
 * 1) Program specified in TDSLOGIN structure
 * 2) The environment variables TDSVER, TDSDUMP, TDSPORT, TDSQUERY, TDSHOST
 * 3) A config file with the following search order:
 *    a) a readable file specified by environment variable FREETDSCONF
 *    b) a readable file in ~/.freetds.conf
 *    c) a readable file in $prefix/etc/freetds.conf
 * 3) ~/.interfaces if exists
 * 4) $SYBASE/interfaces if exists
 * 5) TDS_DEF_* default values
 *
 * .tdsrc and freetds.conf have been added to make the package easier to 
 * integration with various Linux and *BSD distributions.
 */
TDSCONNECTION *
tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale)
{
	TDSCONNECTION *connection;
	char *s;
	char *path;
	pid_t pid;
	int opened = 0, found;

	/* allocate a new structure with hard coded and build-time defaults */
	connection = tds_alloc_connection(locale);
	if (!connection)
		return NULL;

	s = getenv("TDSDUMPCONFIG");
	if (s) {
		if (*s) {
			opened = tdsdump_open(s);
		} else {
			pid = getpid();
			if (asprintf(&path, pid_config_logpath, pid) >= 0) {
				if (*path) {
					opened = tdsdump_open(path);
				}
				free(path);
			}
		}
	}

	tdsdump_log(TDS_DBG_INFO1, "Getting connection information for [%s].\n", 
			    tds_dstr_cstr(&login->server_name));	/* (The server name is set in login.c.) */

	/* Read the config files. */
	tdsdump_log(TDS_DBG_INFO1, "Attempting to read conf files.\n");
	found = tds_read_conf_file(connection, tds_dstr_cstr(&login->server_name));
	if (!found) {
		if (parse_server_name_for_port(connection, login)) {
			char ip_addr[256];

			found = tds_read_conf_file(connection, tds_dstr_cstr(&connection->server_name));
			/* do it again to really override what found in freetds.conf */
			if (found) {
				parse_server_name_for_port(connection, login);
			} else if (tds_lookup_host(tds_dstr_cstr(&connection->server_name), ip_addr) == TDS_SUCCEED) {
				tds_dstr_dup(&connection->server_host_name, &connection->server_name);
				tds_dstr_copy(&connection->ip_addr, ip_addr);
				found = 1;
			}
		}
	}
	if (!found) {
		/* fallback to interfaces file */
		tdsdump_log(TDS_DBG_INFO1, "Failed in reading conf file.  Trying interface files.\n");
		if (!tds_read_interfaces(tds_dstr_cstr(&login->server_name), connection)) {
			tdsdump_log(TDS_DBG_INFO1, "Failed to find [%s] in configuration files; trying '%s' instead.\n", 
						   tds_dstr_cstr(&login->server_name), tds_dstr_cstr(&connection->server_name));
			if (tds_dstr_isempty(&connection->ip_addr))
				tdserror(tds->tds_ctx, tds, TDSEINTF, 0);
		}
	}

	/* Override config file settings with environment variables. */
	tds_fix_connection(connection);

	/* And finally apply anything from the login structure */
	tds_config_login(connection, login);
	
	if (opened) {
		tdsdump_log(TDS_DBG_INFO1, "Final connection parameters:\n");
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_name", tds_dstr_cstr(&connection->server_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_host_name", tds_dstr_cstr(&connection->server_host_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "ip_addr", tds_dstr_cstr(&connection->ip_addr));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "instance_name", tds_dstr_cstr(&connection->instance_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "port", connection->port);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "major_version", TDS_MAJOR(connection));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "minor_version", TDS_MINOR(connection));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "block_size", connection->block_size);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "language", tds_dstr_cstr(&connection->language));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_charset", tds_dstr_cstr(&connection->server_charset));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "connect_timeout", connection->connect_timeout);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "client_host_name", tds_dstr_cstr(&connection->client_host_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "client_charset", tds_dstr_cstr(&connection->client_charset));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "app_name", tds_dstr_cstr(&connection->app_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "user_name", tds_dstr_cstr(&connection->user_name));
		/* tdsdump_log(TDS_DBG_PASSWD, "\t%20s = %s\n", "password", tds_dstr_cstr(&connection->password)); 
			(no such flag yet) */
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "library", tds_dstr_cstr(&connection->library));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "bulk_copy", (int)connection->bulk_copy);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "suppress_language", (int)connection->suppress_language);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "encrypt level", (int)connection->encryption_level);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "query_timeout", connection->query_timeout);
		/* tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "capabilities", tds_dstr_cstr(&connection->capabilities)); 
			(not null terminated) */
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "database", tds_dstr_cstr(&connection->database));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "dump_file", tds_dstr_cstr(&connection->dump_file));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %x\n", "debug_flags", connection->debug_flags);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "text_size", connection->text_size);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "broken_dates", connection->broken_dates);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "emul_little_endian", connection->emul_little_endian);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_realm_name", tds_dstr_cstr(&connection->server_realm_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "application_intent", connection->application_intent);

		tdsdump_close();
	}

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&connection->dump_file) && !tdsdump_isopen()) {
		if (connection->debug_flags)
			tds_debug_flags = connection->debug_flags;
		tdsdump_open(tds_dstr_cstr(&connection->dump_file));
	}

	return connection;
}

/**
 * Fix configuration after reading it. 
 * Currently this read some environment variables and replace some options.
 */
void
tds_fix_connection(TDSCONNECTION * connection)
{
	/* Now check the environment variables */
	tds_config_env_tdsver(connection);
	tds_config_env_tdsdump(connection);
	tds_config_env_tdsport(connection);
	tds_config_env_tdshost(connection);
}

static int
tds_try_conf_file(const char *path, const char *how, const char *server, TDSCONNECTION * connection)
{
	int found = 0;
	FILE *in;

	if ((in = fopen(path, "r")) == NULL) {
		tdsdump_log(TDS_DBG_INFO1, "Could not open '%s' (%s).\n", path, how);
		return found;
	}

	tdsdump_log(TDS_DBG_INFO1, "Found conf file '%s' %s.\n", path, how);
	found = tds_read_conf_sections(in, server, connection);

	if (found) {
		tdsdump_log(TDS_DBG_INFO1, "Success: [%s] defined in %s.\n", server, path);
	} else {
		tdsdump_log(TDS_DBG_INFO2, "[%s] not found.\n", server);
	}

	fclose(in);

	return found;
}


/**
 * Return filename from HOME directory
 * @return allocated string or NULL if error
 */
static char *
tds_get_home_file(const char *file)
{
	char *home, *path;

	home = tds_get_homedir();
	if (!home)
		return NULL;
	if (asprintf(&path, "%s/%s", home, file) < 0)
		path = NULL;
	free(home);
	return path;
}

/**
 * Read configuration info for given server
 * return 0 on error
 * @param connection where to store configuration
 * @param server       section of file configuration that hold 
 *                     configuration for a server
 */
int
tds_read_conf_file(TDSCONNECTION * connection, const char *server)
{
	char *path = NULL;
	char *eptr = NULL;
	int found = 0;

	if (interf_file) {
		found = tds_try_conf_file(interf_file, "set programmatically", server, connection);
	}

	/* FREETDSCONF env var, pkleef@openlinksw.com 01/21/02 */
	if (!found) {
		path = getenv("FREETDSCONF");
		if (path) {
			found = tds_try_conf_file(path, "(from $FREETDSCONF)", server, connection);
		} else {
			tdsdump_log(TDS_DBG_INFO2, "... $FREETDSCONF not set.  Trying $FREETDS/etc.\n");
		}
	}

	/* FREETDS env var, Bill Thompson 16/07/03 */
	if (!found) {
		eptr = getenv("FREETDS");
		if (eptr) {
			if (asprintf(&path, freetds_conf, eptr) >= 0) {
				found = tds_try_conf_file(path, location, server, connection);
				free(path);
			}
		} else {
			tdsdump_log(TDS_DBG_INFO2, "... $FREETDS not set.  Trying $HOME.\n");
		}
	}

	if (!found) {
		path = tds_get_home_file(".freetds.conf");
		if (path) {
			found = tds_try_conf_file(path, "(.freetds.conf)", server, connection);
			free(path);
		} else {
			tdsdump_log(TDS_DBG_INFO2, "... Error getting ~/.freetds.conf.  Trying %s.\n", FREETDS_SYSCONFFILE);
		}
	}

	if (!found) {
		found = tds_try_conf_file(FREETDS_SYSCONFFILE, "(default)", server, connection);
	}

	return found;
}

static int
tds_read_conf_sections(FILE * in, const char *server, TDSCONNECTION * connection)
{
	DSTR default_instance;
	int default_port;

	int found;

	tds_read_conf_section(in, "global", tds_parse_conf_section, connection);

	if (!server[0])
		return 0;
	rewind(in);

	tds_dstr_init(&default_instance);
	tds_dstr_dup(&default_instance, &connection->instance_name);
	default_port = connection->port;

	found = tds_read_conf_section(in, server, tds_parse_conf_section, connection);

	/* 
	 * If both instance and port are specified and neither one came from the default, it's an error 
	 * TODO: If port/instance is specified in the non-default, it has priority over the default setting. 
	 * TODO: test this. 
	 */
	if (!tds_dstr_isempty(&connection->instance_name) && connection->port &&
	    !(!tds_dstr_isempty(&default_instance) || default_port)) {
		tdsdump_log(TDS_DBG_ERROR, "error: cannot specify both port %d and instance %s.\n", 
						connection->port, tds_dstr_cstr(&connection->instance_name));
		/* tdserror(tds->tds_ctx, tds, TDSEPORTINSTANCE, 0); */
	}
	tds_dstr_free(&default_instance);
	return found;
}

static const struct {
	char value[9];
	unsigned char to_return;
} boolean_values[] = {
	{ "yes",	1 },
	{ "no",		0 },
	{ "on",		1 },
	{ "off",	0 },
	{ "true",	1 },
	{ "false",	0 },
	{ "ReadOnly", 1},
	{ "ReadWrite", 0}
};

int
tds_config_boolean(const char *value)
{
	int p;

	for (p = 0; p < TDS_VECTOR_SIZE(boolean_values); ++p) {
		if (!strcasecmp(value, boolean_values[p].value))
			return boolean_values[p].to_return;
	}
	tdsdump_log(TDS_DBG_INFO1, "UNRECOGNIZED boolean value: '%s'. Treating as 'no'.\n", value);
	return 0;
}

static void
tds_config_encryption(const char * value, TDSCONNECTION * connection)
{
	TDS_ENCRYPTION_LEVEL lvl = TDS_ENCRYPTION_OFF;

	if (!strcasecmp(value, TDS_STR_ENCRYPTION_OFF))
		;
	else if (!strcasecmp(value, TDS_STR_ENCRYPTION_REQUEST))
		lvl = TDS_ENCRYPTION_REQUEST;
	else if (!strcasecmp(value, TDS_STR_ENCRYPTION_REQUIRE))
		lvl = TDS_ENCRYPTION_REQUIRE;
	else
		tdsdump_log(TDS_DBG_INFO1, "UNRECOGNIZED option value '%s'...ignoring.\n", value);

	connection->encryption_level = lvl;
}

/**
 * Read a section of configuration file (INI style file)
 * @param in             configuration file
 * @param section        section to read
 * @param tds_conf_parse callback that receive every entry in section
 * @param param          parameter to pass to callback function
 */
int
tds_read_conf_section(FILE * in, const char *section, TDSCONFPARSE tds_conf_parse, void *param)
{
	char line[256], *value;
#define option line
	char *s;
	char p;
	int i;
	int insection = 0;
	int found = 0;

	tdsdump_log(TDS_DBG_INFO1, "Looking for section %s.\n", section);
	while (fgets(line, sizeof(line), in)) {
		s = line;

		/* skip leading whitespace */
		while (*s && TDS_ISSPACE(*s))
			s++;

		/* skip it if it's a comment line */
		if (*s == ';' || *s == '#')
			continue;

		/* read up to the = ignoring duplicate spaces */
		p = 0;
		i = 0;
		while (*s && *s != '=') {
			if (!TDS_ISSPACE(*s)) {
				if (TDS_ISSPACE(p))
					option[i++] = ' ';
				option[i++] = tolower((unsigned char) *s);
			}
			p = *s;
			s++;
		}
		option[i] = '\0';

		/* skip if empty option */
		if (!i)
			continue;

		/* skip the = */
		if (*s)
			s++;

		/* terminate the option, must be done after skipping = */
		option[i] = '\0';

		/* skip leading whitespace */
		while (*s && TDS_ISSPACE(*s))
			s++;

		/* read up to a # ; or null ignoring duplicate spaces */
		value = s;
		p = 0;
		i = 0;
		while (*s && *s != ';' && *s != '#') {
			if (!TDS_ISSPACE(*s)) {
				if (TDS_ISSPACE(p))
					value[i++] = ' ';
				value[i++] = *s;
			}
			p = *s;
			s++;
		}
		value[i] = '\0';

		if (option[0] == '[') {
			s = strchr(option, ']');
			if (s)
				*s = '\0';
			tdsdump_log(TDS_DBG_INFO1, "\tFound section %s.\n", &option[1]);

			if (!strcasecmp(section, &option[1])) {
				tdsdump_log(TDS_DBG_INFO1, "Got a match.\n");
				insection = 1;
				found = 1;
			} else {
				insection = 0;
			}
		} else if (insection) {
			tds_conf_parse(option, value, param);
		}

	}
	tdsdump_log(TDS_DBG_INFO1, "\tReached EOF\n");
	return found;
#undef option
}

/* Also used to scan ODBC.INI entries */
void
tds_parse_conf_section(const char *option, const char *value, void *param)
{
	TDSCONNECTION *connection = (TDSCONNECTION *) param;

	tdsdump_log(TDS_DBG_INFO1, "\t%s = '%s'\n", option, value);

	if (!strcmp(option, TDS_STR_VERSION)) {
		tds_config_verstr(value, connection);
		tdsdump_log(TDS_DBG_FUNC, "Setting TDS Version to  '%s' .\n", value);
	} else if (!strcmp(option, TDS_STR_BLKSZ)) {
		int val = atoi(value);
		if (val >= 512 && val < 65536)
			connection->block_size = val;
	} else if (!strcmp(option, TDS_STR_SWAPDT)) {
		connection->broken_dates = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_GSSAPI_DELEGATION)) {
		/* gssapi flag addition */
		connection->gssapi_use_delegation = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_DUMPFILE)) {
		tds_dstr_copy(&connection->dump_file, value);
	} else if (!strcmp(option, TDS_STR_DEBUGFLAGS)) {
		char *end;
		long flags;
		flags = strtol(value, &end, 0);
		if (*value != '\0' && *end == '\0' && flags != LONG_MIN && flags != LONG_MAX)
			connection->debug_flags = flags;
	} else if (!strcmp(option, TDS_STR_TIMEOUT) || !strcmp(option, TDS_STR_QUERY_TIMEOUT)) {
		if (atoi(value))
			connection->query_timeout = atoi(value);
	} else if (!strcmp(option, TDS_STR_CONNTIMEOUT)) {
		if (atoi(value))
			connection->connect_timeout = atoi(value);
	} else if (!strcmp(option, TDS_STR_HOST)) {
		char tmp[256];

		tdsdump_log(TDS_DBG_INFO1, "Found host entry %s.\n", value);
		tds_dstr_copy(&connection->server_host_name, value);
		tds_lookup_host(value, tmp);
		tds_dstr_copy(&connection->ip_addr, tmp);
		tdsdump_log(TDS_DBG_INFO1, "IP addr is %s.\n", tds_dstr_cstr(&connection->ip_addr));
	} else if (!strcmp(option, TDS_STR_PORT)) {
		if (atoi(value))
			connection->port = atoi(value);
	} else if (!strcmp(option, TDS_STR_EMUL_LE)) {
		connection->emul_little_endian = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_TEXTSZ)) {
		if (atoi(value))
			connection->text_size = atoi(value);
	} else if (!strcmp(option, TDS_STR_CHARSET)) {
		tds_dstr_copy(&connection->server_charset, value);
		tdsdump_log(TDS_DBG_INFO1, "%s is %s.\n", option, tds_dstr_cstr(&connection->server_charset));
	} else if (!strcmp(option, TDS_STR_CLCHARSET)) {
		tds_dstr_copy(&connection->client_charset, value);
		tdsdump_log(TDS_DBG_INFO1, "tds_parse_conf_section: %s is %s.\n", option, tds_dstr_cstr(&connection->client_charset));
	} else if (!strcmp(option, TDS_STR_LANGUAGE)) {
		tds_dstr_copy(&connection->language, value);
	} else if (!strcmp(option, TDS_STR_APPENDMODE)) {
		tds_g_append_mode = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_INSTANCE)) {
		tds_dstr_copy(&connection->instance_name, value);
	} else if (!strcmp(option, TDS_STR_ENCRYPTION)) {
		tds_config_encryption(value, connection);
	} else if (!strcmp(option, TDS_STR_ASA_DATABASE)) {
		tds_dstr_copy(&connection->server_name, value);
	} else if (!strcmp(option, TDS_STR_USENTLMV2)) {
		connection->use_ntlmv2 = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_REALM)) {
		tds_dstr_copy(&connection->server_realm_name, value);
	} else if (!strcmp(option, TDS_STR_APPLICATION_INTENT)) {
		connection->application_intent = tds_config_boolean(value);
		tdsdump_log(TDS_DBG_FUNC, "Setting Application Intent to  '%s' .\n", value);
	}
	else {
		tdsdump_log(TDS_DBG_INFO1, "UNRECOGNIZED option '%s' ... ignoring.\n", option);
	}
}

static void
tds_config_login(TDSCONNECTION * connection, TDSLOGIN * login)
{
	if (!tds_dstr_isempty(&login->server_name)) {
		if (1 || tds_dstr_isempty(&connection->server_name)) 
			tds_dstr_dup(&connection->server_name, &login->server_name);
	}
	if (login->tds_version)
		connection->tds_version = login->tds_version;
	if (!tds_dstr_isempty(&login->language)) {
		tds_dstr_dup(&connection->language, &login->language);
	}
	if (!tds_dstr_isempty(&login->server_charset)) {
		tds_dstr_dup(&connection->server_charset, &login->server_charset);
	}
	if (!tds_dstr_isempty(&login->client_charset)) {
		tds_dstr_dup(&connection->client_charset, &login->client_charset);
		tdsdump_log(TDS_DBG_INFO1, "tds_config_login: %s is %s.\n", "client_charset",
			    tds_dstr_cstr(&connection->client_charset));
	}
	if (!tds_dstr_isempty(&login->database)) {
		tds_dstr_dup(&connection->database, &login->database);
		tdsdump_log(TDS_DBG_INFO1, "tds_config_login: %s is %s.\n", "database_name",
			    tds_dstr_cstr(&connection->database));
	}
	if (!tds_dstr_isempty(&login->client_host_name)) {
		tds_dstr_dup(&connection->client_host_name, &login->client_host_name);
	}
	if (!tds_dstr_isempty(&login->app_name)) {
		tds_dstr_dup(&connection->app_name, &login->app_name);
	}
	if (!tds_dstr_isempty(&login->user_name)) {
		tds_dstr_dup(&connection->user_name, &login->user_name);
	}
	if (!tds_dstr_isempty(&login->password)) {
		/* for security reason clear memory */
		tds_dstr_zero(&connection->password);
		tds_dstr_dup(&connection->password, &login->password);
	}
	if (!tds_dstr_isempty(&login->library)) {
		tds_dstr_dup(&connection->library, &login->library);
	}
	if (login->encryption_level) {
		connection->encryption_level = login->encryption_level;
	}
	if (login->suppress_language) {
		connection->suppress_language = 1;
	}
	if (login->bulk_copy) {
		connection->bulk_copy = 1;
	}
	if (login->block_size) {
		connection->block_size = login->block_size;
	}
	if (login->port)
		connection->port = login->port;
	if (login->connect_timeout)
		connection->connect_timeout = login->connect_timeout;

	if (login->query_timeout)
		connection->query_timeout = login->query_timeout;

	if(login->application_intent)
		connection->application_intent = login->application_intent;

	/* copy other info not present in configuration file */
	memcpy(connection->capabilities, login->capabilities, TDS_MAX_CAPABILITY);
}

static void
tds_config_env_tdsdump(TDSCONNECTION * connection)
{
	char *s;
	char *path;
	pid_t pid = 0;

	if ((s = getenv("TDSDUMP"))) {
		if (!strlen(s)) {
			pid = getpid();
			if (asprintf(&path, pid_logpath, pid) >= 0)
				tds_dstr_set(&connection->dump_file, path);
		} else {
			tds_dstr_copy(&connection->dump_file, s);
		}
		tdsdump_log(TDS_DBG_INFO1, "Setting 'dump_file' to '%s' from $TDSDUMP.\n", tds_dstr_cstr(&connection->dump_file));
	}
}
static void
tds_config_env_tdsport(TDSCONNECTION * connection)
{
	char *s;

	if ((s = getenv("TDSPORT"))) {
		connection->port = tds_lookup_port(s);
		tds_dstr_copy(&connection->instance_name, "");
		tdsdump_log(TDS_DBG_INFO1, "Setting 'port' to %s from $TDSPORT.\n", s);
	}
	return;
}
static void
tds_config_env_tdsver(TDSCONNECTION * connection)
{
	char *tdsver;

	if ((tdsver = getenv("TDSVER"))) {
		tds_config_verstr(tdsver, connection);
		tdsdump_log(TDS_DBG_INFO1, "Setting 'tdsver' to %s from $TDSVER.\n", tdsver);

	}
	return;
}

/* TDSHOST env var, pkleef@openlinksw.com 01/21/02 */
static void
tds_config_env_tdshost(TDSCONNECTION * connection)
{
	char *tdshost;
	char tmp[256];

	if ((tdshost = getenv("TDSHOST"))) {
		tds_dstr_copy(&connection->server_host_name, tdshost);
		tds_lookup_host(tdshost, tmp);
		tds_dstr_copy(&connection->ip_addr, tmp);
		tdsdump_log(TDS_DBG_INFO1, "Setting 'ip_addr' to %s (%s) from $TDSHOST.\n", tmp, tdshost);
	}
}

/**
 * Set TDS version from given string
 * @param tdsver tds string version
 * @param connection where to store information
 * @return as encoded hex value: high nybble major, low nybble minor.
 */
TDS_USMALLINT
tds_config_verstr(const char *tdsver, TDSCONNECTION * connection)
{
	TDS_USMALLINT version;

	if (!strcmp(tdsver, "42") || !strcmp(tdsver, "4.2"))
		version = 0x402;
	else if (!strcmp(tdsver, "46") || !strcmp(tdsver, "4.6"))
		version = 0x406;
	else if (!strcmp(tdsver, "50") || !strcmp(tdsver, "5.0"))
		version = 0x500;
	else if (!strcmp(tdsver, "70") || !strcmp(tdsver, "7.0"))
		version = 0x700;
	else if (!strcmp(tdsver, "80") || !strcmp(tdsver, "8.0") || !strcmp(tdsver, "7.1"))
		version = 0x701;
	else if (!strcmp(tdsver, "7.2"))
		version = 0x702;
	else if (!strcmp(tdsver, "0.0"))
		version = 0;
	else 
		return 0;

	if (connection)
		connection->tds_version = version;

	tdsdump_log(TDS_DBG_INFO1, "Setting tds version to %s (0x%0x) from $TDSVER.\n", tdsver, version);

	return version;
}

/**
 * Set the full name of interface file
 * @param interf file name
 */
int
tds_set_interfaces_file_loc(const char *interf)
{
	/* Free it if already set */
	if (interf_file != NULL)
		TDS_ZERO_FREE(interf_file);
	/* If no filename passed, leave it NULL */
	if ((interf == NULL) || (interf[0] == '\0')) {
		return TDS_SUCCEED;
	}
	/* Set to new value */
	if ((interf_file = strdup(interf)) == NULL) {
		return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

/**
 * Get the IP address for a hostname. Store server's IP address 
 * in the string 'ip' in dotted-decimal notation.  (The "hostname" might itself
 * be a dotted-decimal address.  
 *
 * If we can't determine the IP address then 'ip' will be set to empty
 * string.
 */
/* TODO callers seem to set always connection info... change it */
int
tds_lookup_host(const char *servername,	/* (I) name of the server                  */
		char *ip	/* (O) dotted-decimal ip address of server */
	)
{
	struct hostent *host = NULL;
	unsigned int ip_addr = 0;

	/* Storage for reentrant getaddrby* calls */
	struct hostent result;
	char buffer[4096];
	int h_errnop;

	/*
	 * Call gethostbyname(3) only if servername is not an ip address. 
	 * This call takes a while and is useless for an ip address.
	 * mlilback 3/2/02
	 */
	ip_addr = inet_addr(servername);
	if (ip_addr != INADDR_NONE) {
		tds_strlcpy(ip, servername, 17);
		return TDS_SUCCEED;
	}

	host = tds_gethostbyname_r(servername, &result, buffer, sizeof(buffer), &h_errnop);

	ip[0] = '\0';
	if (host) {
		struct in_addr *ptr = (struct in_addr *) host->h_addr;

		tds_inet_ntoa_r(*ptr, ip, 17);
		return TDS_SUCCEED;
	}
	return TDS_FAIL;
}

/**
 * Given a portname lookup the port.
 *
 * If we can't determine the port number then return 0.
 */
static int
tds_lookup_port(const char *portname)
{
	int num = 0;

	if (portname) {
		num = atoi(portname);
		if (!num) {
			char buffer[4096];
			struct servent serv_result;
			struct servent *service = tds_getservbyname_r(portname, "tcp", &serv_result, buffer, sizeof(buffer));

			if (service)
				num = ntohs(service->s_port);
		}
	}
	return num;
}

/* TODO same code in convert.c ?? */
static int
hexdigit(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	/* ascii optimization, 'A' -> 'a', 'a' -> 'a' */
	c |= 0x20;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0;	/* bad hex digit */
}

static int
hex2num(char *hex)
{
	return hexdigit(hex[0]) * 16 + hexdigit(hex[1]);
}

/**
 * Open and read the file 'file' searching for a logical server
 * by the name of 'host'.  If one is found then lookup
 * the IP address and port number and store them in 'connection'
 *
 * \param dir name of base directory for interface file
 * \param file name of the interface file
 * \param host logical host to search for
 * \return 0 if not fount 1 if found
 */
static int
search_interface_file(TDSCONNECTION * connection, const char *dir, const char *file, const char *host)
{
	char *pathname;
	char line[255];
	char tmp_ip[sizeof(line)];
	char tmp_port[sizeof(line)];
	char tmp_ver[sizeof(line)];
	FILE *in;
	char *field;
	int found = 0;
	int server_found = 0;
	char *lasts;

	line[0] = '\0';
	tmp_ip[0] = '\0';
	tmp_port[0] = '\0';
	tmp_ver[0] = '\0';

	tdsdump_log(TDS_DBG_INFO1, "Searching interfaces file %s/%s.\n", dir, file);
	pathname = (char *) malloc(strlen(dir) + strlen(file) + 10);
	if (!pathname)
		return 0;

	/*
	 * create the full pathname to the interface file
	 */
	if (file[0] == '\0') {
		pathname[0] = '\0';
	} else {
		if (dir[0] == '\0') {
			pathname[0] = '\0';
		} else {
			strcpy(pathname, dir);
			strcat(pathname, TDS_SDIR_SEPARATOR);
		}
		strcat(pathname, file);
	}


	/*
	 * parse the interfaces file and find the server and port
	 */
	if ((in = fopen(pathname, "r")) == NULL) {
		tdsdump_log(TDS_DBG_INFO1, "Couldn't open %s.\n", pathname);
		free(pathname);
		return 0;
	}
	tdsdump_log(TDS_DBG_INFO1, "Interfaces file %s opened.\n", pathname);

	while (fgets(line, sizeof(line) - 1, in)) {
		if (line[0] == '#')
			continue;	/* comment */

		if (!TDS_ISSPACE(line[0])) {
			field = strtok_r(line, "\n\t ", &lasts);
			if (!strcmp(field, host)) {
				found = 1;
				tdsdump_log(TDS_DBG_INFO1, "Found matching entry for host %s.\n", host);
			} else
				found = 0;
		} else if (found && TDS_ISSPACE(line[0])) {
			field = strtok_r(line, "\n\t ", &lasts);
			if (field != NULL && !strcmp(field, "query")) {
				field = strtok_r(NULL, "\n\t ", &lasts);	/* tcp or tli */
				if (!strcmp(field, "tli")) {
					tdsdump_log(TDS_DBG_INFO1, "TLI service.\n");
					field = strtok_r(NULL, "\n\t ", &lasts);	/* tcp */
					field = strtok_r(NULL, "\n\t ", &lasts);	/* device */
					field = strtok_r(NULL, "\n\t ", &lasts);	/* host/port */
					if (strlen(field) >= 18) {
						sprintf(tmp_port, "%d", hex2num(&field[6]) * 256 + hex2num(&field[8]));
						sprintf(tmp_ip, "%d.%d.%d.%d", hex2num(&field[10]),
							hex2num(&field[12]), hex2num(&field[14]), hex2num(&field[16]));
						tdsdump_log(TDS_DBG_INFO1, "tmp_port = %s. tmp_ip = %s.\n", tmp_port, tmp_ip);
					}
				} else {
					field = strtok_r(NULL, "\n\t ", &lasts);	/* ether */
					strcpy(tmp_ver, field);
					field = strtok_r(NULL, "\n\t ", &lasts);	/* host */
					strcpy(tmp_ip, field);
					tdsdump_log(TDS_DBG_INFO1, "host field %s.\n", tmp_ip);
					field = strtok_r(NULL, "\n\t ", &lasts);	/* port */
					strcpy(tmp_port, field);
				}	/* else */
				server_found = 1;
			}	/* if */
		}		/* else if */
	}			/* while */
	fclose(in);
	free(pathname);


	/*
	 * Look up the host and service
	 */
	if (server_found) {
		tds_dstr_copy(&connection->server_host_name, tmp_ip);
		tds_lookup_host(tmp_ip, line);
		tdsdump_log(TDS_DBG_INFO1, "Resolved IP as '%s'.\n", line);
		tds_dstr_copy(&connection->ip_addr, line);
		if (tmp_port[0])
			connection->port = tds_lookup_port(tmp_port);
		if (tmp_ver[0])
			tds_config_verstr(tmp_ver, connection);
	}
	return server_found;
}				/* search_interface_file()  */

/**
 * Try to find the IP number and port for a (possibly) logical server name.
 *
 * @note This function uses only the interfaces file and is deprecated.
 */
static int
tds_read_interfaces(const char *server, TDSCONNECTION * connection)
{
	int found = 0;

	/* read $SYBASE/interfaces */

	if (!server || !server[0]) {
		server = getenv("TDSQUERY");
		if (!server || !server[0])
			server = "SYBASE";
		tdsdump_log(TDS_DBG_INFO1, "Setting server to %s from $TDSQUERY.\n", server);

	}
	tdsdump_log(TDS_DBG_INFO1, "Looking for server %s....\n", server);

	/*
	 * Look for the server in the interf_file iff interf_file has been set.
	 */
	if (interf_file) {
		tdsdump_log(TDS_DBG_INFO1, "Looking for server in file %s.\n", interf_file);
		found = search_interface_file(connection, "", interf_file, server);
	}

	/*
	 * if we haven't found the server yet then look for a $HOME/.interfaces file
	 */
	if (!found) {
		char *path = tds_get_home_file(".interfaces");

		if (path) {
			tdsdump_log(TDS_DBG_INFO1, "Looking for server in %s.\n", path);
			found = search_interface_file(connection, "", path, server);
			free(path);
		}
	}

	/*
	 * if we haven't found the server yet then look in $SYBBASE/interfaces file
	 */
	if (!found) {
		const char *sybase = getenv("SYBASE");
#ifdef __VMS
		/* We've got to be in unix syntax for later slash-joined concatenation. */
		#include <unixlib.h>
		const char *unixspec = decc$translate_vms(sybase);
		if ( (int)unixspec != 0 && (int)unixspec != -1 ) sybase = unixspec;
#endif
		if (!sybase || !sybase[0])
			sybase = interfaces_path;

		tdsdump_log(TDS_DBG_INFO1, "Looking for server in %s/interfaces.\n", sybase);
		found = search_interface_file(connection, sybase, "interfaces", server);
	}

	/*
	 * If we still don't have the server and port then assume the user
	 * typed an actual server host name.
	 */
	if (!found) {
		char ip_addr[255];
		int ip_port;
		const char *env_port;

		/*
		 * Make a guess about the port number
		 */

		if (connection->port == 0) {
			/*
			 * Not set in the [global] section of the
			 * configure file, take a guess.
			 */
			ip_port = TDS_DEF_PORT;
		} else {
			/*
			 * Preserve setting from the [global] section
			 * of the configure file.
			 */
			ip_port = connection->port;
		}
		if ((env_port = getenv("TDSPORT")) != NULL) {
			ip_port = tds_lookup_port(env_port);
			tdsdump_log(TDS_DBG_INFO1, "Setting 'ip_port' to %s from $TDSPORT.\n", env_port);
		} else
			tdsdump_log(TDS_DBG_INFO1, "Setting 'ip_port' to %d as a guess.\n", ip_port);

		/*
		 * look up the host
		 */
		tds_lookup_host(server, ip_addr);
		if (ip_addr[0]) {
			tds_dstr_copy(&connection->server_host_name, server);
			tds_dstr_copy(&connection->ip_addr, ip_addr);
		}
		if (ip_port)
			connection->port = ip_port;
	}

	return found;
}

/**
 * Check the server name to find port info first
 * Warning: connection-> & login-> are all modified when needed
 * \return 1 when found, else 0
 */
static int
parse_server_name_for_port(TDSCONNECTION * connection, TDSLOGIN * login)
{
	const char *pSep;
	const char *server;

	/* seek the ':' in login server_name */
	server = tds_dstr_cstr(&login->server_name);
	pSep = strrchr(server, ':');

	if (pSep && pSep != server) {	/* yes, i found it! */
		/* modify connection-> && login->server_name & ->port */
		login->port = connection->port = atoi(pSep + 1);
		tds_dstr_copy(&connection->instance_name, "");
	} else {
		/* handle instance name */
		pSep = strrchr(server, '\\');
		if (!pSep || pSep == server)
			return 0;

		tds_dstr_copy(&connection->instance_name, pSep + 1);
		connection->port = 0;
	}

	if (!tds_dstr_copyn(&connection->server_name, server, pSep - server))
		return 0;

	return 1;
}

/**
 * Return a structure capturing the compile-time settings provided to the
 * configure script.  
 */

const TDS_COMPILETIME_SETTINGS *
tds_get_compiletime_settings(void)
{
	static const TDS_COMPILETIME_SETTINGS settings = {
		  TDS_VERSION_NO
		, FREETDS_SYSCONFDIR
		, "unknown"	/* need fancy script in makefile */
#		ifdef MSDBLIB
			, 1
#		else
			, 0
#		endif
#		ifdef TDS_SYBASE_COMPAT
			, 1
#		else
			, 0
#		endif
#		ifdef _REENTRANT
			, 1
#		else
			, 0
#		endif
#		ifdef HAVE_ICONV
			, 1
#		else
			, 0
#		endif
#		ifdef TDS46
			, "4.6"
#		elif TDS50
			, "5.0"
#		elif TDS70
			, "7.0"
#		elif TDS71
			, "7.1"
#		elif TDS72
			, "7.2"
#		else
			, "4.2"
#		endif
#		ifdef IODBC
			, 1
#		else
			, 0
#		endif
#		ifdef UNIXODBC
			, 1
#		else
			, 0
#		endif
	};

	assert(settings.tdsver);

	return &settings;
}

/** @} */

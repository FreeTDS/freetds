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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

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

#include "tds.h"
#include "tds_configs.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: config.c,v 1.83 2003-09-30 19:06:45 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };


static void tds_config_login(TDSCONNECTINFO * connect_info, TDSLOGIN * login);
static void tds_config_env_tdsquery(TDSCONNECTINFO * connect_info);
static void tds_config_env_tdsdump(TDSCONNECTINFO * connect_info);
static void tds_config_env_tdsver(TDSCONNECTINFO * connect_info);
static void tds_config_env_tdsport(TDSCONNECTINFO * connect_info);
static void tds_config_env_tdshost(TDSCONNECTINFO * connect_info);
static int tds_read_conf_sections(FILE * in, const char *server, TDSCONNECTINFO * connect_info);
static void tds_parse_conf_section(const char *option, const char *value, void *param);
static void tds_read_interfaces(const char *server, TDSCONNECTINFO * connect_info);
static int tds_config_boolean(const char *value);
static int parse_server_name_for_port(TDSCONNECTINFO * connect_info, TDSLOGIN * login);
static int tds_lookup_port(const char *portname);

extern int tds_g_append_mode;

static char *interf_file = NULL;

/**
 * \ingroup libtds
 * \defgroup config Configuration
 * Handle reading of configuration
 */

/** \addtogroup config
 *  \@{ 
 */

/**
 * tds_read_config_info() will fill the tds connect_info structure based on configuration 
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
TDSCONNECTINFO *
tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale)
{
	TDSCONNECTINFO *connect_info;
	char *s;
	char *path;
	pid_t pid;
	int opened = 0;

	/* allocate a new structure with hard coded and build-time defaults */
	connect_info = tds_alloc_connect(locale);
	if (!connect_info)
		return NULL;

	s = getenv("TDSDUMPCONFIG");
	if (s) {
		if (*s) {
			opened = tdsdump_open(s);
		} else {
			pid = getpid();
			if (asprintf(&path, "/tmp/tdsconfig.log.%d", pid) >= 0) {
				if (*path) {
					opened = tdsdump_open(path);
				}
				free(path);
			}
		}
	}

	tdsdump_log(TDS_DBG_INFO1, "%L Attempting to read conf files.\n");
	if (!tds_read_conf_file(connect_info, tds_dstr_cstr(&login->server_name))) {
		/* fallback to interfaces file */
		tdsdump_log(TDS_DBG_INFO1, "%L Failed in reading conf file.  Trying interface files.\n");
		tds_read_interfaces(tds_dstr_cstr(&login->server_name), connect_info);
	}

	if (parse_server_name_for_port(connect_info, login)) {
		tdsdump_log(TDS_DBG_INFO1, "%L Parsed servername, now %s on %d.\n", connect_info->server_name, login->port);
	}

	tds_fix_connect(connect_info);

	/* And finally the login structure */
	tds_config_login(connect_info, login);

	if (opened) {
		tdsdump_close();
	}
	return connect_info;
}

/**
 * Fix configuration after reading it. 
 * Currently this read some environment variables and replace some options.
 */
void
tds_fix_connect(TDSCONNECTINFO * connect_info)
{
	/* Now check the environment variables */
	tds_config_env_tdsver(connect_info);
	tds_config_env_tdsdump(connect_info);
	tds_config_env_tdsport(connect_info);
	tds_config_env_tdsquery(connect_info);
	tds_config_env_tdshost(connect_info);
}

static int
tds_try_conf_file(const char *path, const char *how, const char *server, TDSCONNECTINFO * connect_info)
{
	int found = 0;
	FILE *in;

	if ((in = fopen(path, "r")) != NULL) {
		tdsdump_log(TDS_DBG_INFO1, "%L Found conf file '%s' %s. Reading section '%s'.\n", path, how, server);
		found = tds_read_conf_sections(in, server, connect_info);

		if (found) {
			tdsdump_log(TDS_DBG_INFO1, "%L ...Success.\n");
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ...'%s' not found.\n", server);
		}

		fclose(in);
	}
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
 * @param connect_info where to store configuration
 * @param server       section of file configuration that hold 
 *                     configuration for a server
 */
int
tds_read_conf_file(TDSCONNECTINFO * connect_info, const char *server)
{
	char *path = NULL;
	int found = 0;

	if (interf_file) {
		found = tds_try_conf_file(interf_file, "set programmatically", server, connect_info);
	}

	/* FREETDSCONF env var, pkleef@openlinksw.com 01/21/02 */
	if (!found) {
		path = getenv("FREETDSCONF");
		if (path) {
			found = tds_try_conf_file(path, "(from $FREETDSCONF)", server, connect_info);
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ...$FREETDSCONF not set.  Trying $HOME.\n");
		}
	}

	if (!found) {
		path = tds_get_home_file(".freetds.conf");
		if (path) {
			found = tds_try_conf_file(path, "(.freetds.conf)", server, connect_info);
			free(path);
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ...Error getting ~/.freetds.conf.  Trying %s.\n", FREETDS_SYSCONFFILE);
		}
	}

	if (!found) {
		found = tds_try_conf_file(FREETDS_SYSCONFFILE, "(default)", server, connect_info);
	}

	return found;
}

static int
tds_read_conf_sections(FILE * in, const char *server, TDSCONNECTINFO * connect_info)
{
	tds_read_conf_section(in, "global", tds_parse_conf_section, connect_info);
	rewind(in);
	return tds_read_conf_section(in, server, tds_parse_conf_section, connect_info);
}

static int
tds_config_boolean(const char *value)
{
	if (!strcmp(value, "yes") || !strcmp(value, "on") || !strcmp(value, "true") || !strcmp(value, "1")) {
		tdsdump_log(TDS_DBG_INFO1, "%L %s is a 'yes/on/true'.\n", value);
		return 1;
	} else {
		tdsdump_log(TDS_DBG_INFO1, "%L %s is a 'no/off/false'.\n", value);
		return 0;
	}
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
	char line[256], option[256], value[256];
	char *s;
	char p;
	int i;
	int insection = 0;
	int found = 0;

	tdsdump_log(TDS_DBG_INFO1, "%L Looking for section %s.\n", section);
	while (fgets(line, 256, in)) {
		s = line;

		/* skip leading whitespace */
		while (*s && isspace(*s))
			s++;

		/* skip it if it's a comment line */
		if (*s == ';' || *s == '#')
			continue;

		/* read up to the = ignoring duplicate spaces */
		p = 0;
		i = 0;
		while (*s && *s != '=') {
			if (!isspace(*s) && isspace(p))
				option[i++] = ' ';
			if (!isspace(*s))
				option[i++] = tolower(*s);
			p = *s;
			s++;
		}
		option[i] = '\0';

		/* skip the = */
		if (*s)
			s++;

		/* skip leading whitespace */
		while (*s && isspace(*s))
			s++;

		/* read up to a # ; or null ignoring duplicate spaces */
		p = 0;
		i = 0;
		while (*s && *s != ';' && *s != '#') {
			if (!isspace(*s) && isspace(p))
				value[i++] = ' ';
			if (!isspace(*s))
				value[i++] = *s;
			p = *s;
			s++;
		}
		value[i] = '\0';

		if (!strlen(option))
			continue;

		if (option[0] == '[') {
			s = &option[1];
			while (*s) {
				if (*s == ']')
					*s = '\0';
				*s = tolower(*s);
				s++;
			}
			tdsdump_log(TDS_DBG_INFO1, "%L ... Found section %s.\n", &option[1]);

			if (!strcasecmp(section, &option[1])) {
				tdsdump_log(TDS_DBG_INFO1, "%L Got a match.\n");
				insection = 1;
				found = 1;
			} else {
				insection = 0;
			}
		} else if (insection) {
			tds_conf_parse(option, value, param);
		}

	}
	return found;
}

static void
tds_parse_conf_section(const char *option, const char *value, void *param)
{
	TDSCONNECTINFO *connect_info = (TDSCONNECTINFO *) param;
	char tmp[256];

	/* fprintf(stderr,"option = '%s' value = '%s'\n", option, value); */
	tdsdump_log(TDS_DBG_INFO1, "%L option = '%s' value = '%s'.\n", option, value);

	if (!strcmp(option, TDS_STR_VERSION)) {
		tds_config_verstr(value, connect_info);
	} else if (!strcmp(option, TDS_STR_BLKSZ)) {
		if (atoi(value))
			connect_info->block_size = atoi(value);
	} else if (!strcmp(option, TDS_STR_SWAPDT)) {
		connect_info->broken_dates = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_SWAPMNY)) {
		connect_info->broken_money = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_TRYSVR)) {
		connect_info->try_server_login = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_TRYDOM)) {
		connect_info->try_domain_login = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_DOMAIN)) {
		tds_dstr_copy(&connect_info->default_domain, value);
	} else if (!strcmp(option, TDS_STR_XDOMAUTH)) {
		connect_info->xdomain_auth = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_DUMPFILE)) {
		tds_dstr_copy(&connect_info->dump_file, value);
	} else if (!strcmp(option, TDS_STR_DEBUGLVL)) {
		if (atoi(value))
			connect_info->debug_level = atoi(value);
	} else if (!strcmp(option, TDS_STR_TIMEOUT)) {
		if (atoi(value))
			connect_info->timeout = atoi(value);
	} else if (!strcmp(option, TDS_STR_CONNTMOUT)) {
		if (atoi(value))
			connect_info->connect_timeout = atoi(value);
	} else if (!strcmp(option, TDS_STR_HOST)) {
		tdsdump_log(TDS_DBG_INFO1, "%L Found host entry %s.\n", value);
		tds_lookup_host(value, tmp);
		tds_dstr_copy(&connect_info->ip_addr, tmp);
		tdsdump_log(TDS_DBG_INFO1, "%L IP addr is %s.\n", connect_info->ip_addr);
	} else if (!strcmp(option, TDS_STR_PORT)) {
		if (atoi(value))
			connect_info->port = atoi(value);
	} else if (!strcmp(option, TDS_STR_EMUL_LE)) {
		connect_info->emul_little_endian = tds_config_boolean(value);
	} else if (!strcmp(option, TDS_STR_TEXTSZ)) {
		if (atoi(value))
			connect_info->text_size = atoi(value);
	} else if (!strcmp(option, TDS_STR_CHARSET)) {
		tds_dstr_copy(&connect_info->server_charset, value);
		tdsdump_log(TDS_DBG_INFO1, "%L %s is %s.\n", option, connect_info->server_charset);
	} else if (!strcmp(option, TDS_STR_CLCHARSET)) {
		tds_dstr_copy(&connect_info->client_charset, value);
		tdsdump_log(TDS_DBG_INFO1, "%L tds_config_login:%d: %s is %s.\n", __LINE__, option, connect_info->client_charset);
	} else if (!strcmp(option, TDS_STR_LANGUAGE)) {
		tds_dstr_copy(&connect_info->language, value);
	} else if (!strcmp(option, TDS_STR_APPENDMODE)) {
		tds_g_append_mode = tds_config_boolean(value);
	} else
		tdsdump_log(TDS_DBG_INFO1, "%L UNRECOGNIZED option '%s'...ignoring.\n", option);
}

static void
tds_config_login(TDSCONNECTINFO * connect_info, TDSLOGIN * login)
{
	if (!tds_dstr_isempty(&login->server_name)) {
		tds_dstr_copy(&connect_info->server_name, tds_dstr_cstr(&login->server_name));
	}
	if (login->major_version || login->minor_version) {
		connect_info->major_version = login->major_version;
		connect_info->minor_version = login->minor_version;
	}
	if (!tds_dstr_isempty(&login->language)) {
		tds_dstr_copy(&connect_info->language, tds_dstr_cstr(&login->language));
	}
	if (!tds_dstr_isempty(&login->server_charset)) {
		tds_dstr_copy(&connect_info->server_charset, tds_dstr_cstr(&login->server_charset));
	}
	if (!tds_dstr_isempty(&login->client_charset)) {
		tds_dstr_copy(&connect_info->client_charset, tds_dstr_cstr(&login->client_charset));
		tdsdump_log(TDS_DBG_INFO1, "%L tds_config_login:%d: %s is %s.\n", __LINE__, "client_charset",
			    connect_info->client_charset);
	}
	if (!tds_dstr_isempty(&login->host_name)) {
		tds_dstr_copy(&connect_info->host_name, tds_dstr_cstr(&login->host_name));
		/* DBSETLHOST and it's equivilants are commentary fields
		 * ** they don't affect connect_info->ip_addr (the server) but they show
		 * ** up in an sp_who as the *clients* hostname.  (bsb, 11/10) 
		 */
		/* should work with IP (mlilback, 11/7/01) */
		/*
		 * if (connect_info->ip_addr) free(connect_info->ip_addr);
		 * connect_info->ip_addr = calloc(sizeof(char),18);
		 * tds_lookup_host(connect_info->host_name, NULL, connect_info->ip_addr, NULL);
		 */
	}
	if (!tds_dstr_isempty(&login->app_name)) {
		tds_dstr_copy(&connect_info->app_name, tds_dstr_cstr(&login->app_name));
	}
	if (!tds_dstr_isempty(&login->user_name)) {
		tds_dstr_copy(&connect_info->user_name, tds_dstr_cstr(&login->user_name));
	}
	if (!tds_dstr_isempty(&login->password)) {
		/* for security reason clear memory */
		tds_dstr_zero(&connect_info->password);
		tds_dstr_copy(&connect_info->password, tds_dstr_cstr(&login->password));
	}
	if (!tds_dstr_isempty(&login->library)) {
		tds_dstr_copy(&connect_info->library, tds_dstr_cstr(&login->library));
	}
	if (login->encrypted) {
		connect_info->encrypted = 1;
	}
	if (login->suppress_language) {
		connect_info->suppress_language = 1;
	}
	if (login->bulk_copy) {
		connect_info->bulk_copy = 1;
	}
	if (login->block_size) {
		connect_info->block_size = login->block_size;
	}
	if (login->port) {
		connect_info->port = login->port;
	}
	if (login->connect_timeout)
		connect_info->connect_timeout = login->connect_timeout;

	/* copy other info not present in configuration file */
	connect_info->query_timeout = login->query_timeout;
	connect_info->longquery_timeout = login->longquery_timeout;
	connect_info->longquery_func = login->longquery_func;
	connect_info->longquery_param = login->longquery_param;
	memcpy(connect_info->capabilities, login->capabilities, TDS_MAX_CAPABILITY);
}

static void
tds_config_env_tdsquery(TDSCONNECTINFO * connect_info)
{
	char *s;

	if ((s = getenv("TDSQUERY")) != NULL && s[0]) {
		tds_dstr_copy(&connect_info->server_name, s);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'server_name' to '%s' from $TDSQUERY.\n", s);
		return;
	}
	if ((s = getenv("DSQUERY")) != NULL && s[0]) {
		tds_dstr_copy(&connect_info->server_name, s);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'server_name' to '%s' from $DSQUERY.\n", s);
	}
}
static void
tds_config_env_tdsdump(TDSCONNECTINFO * connect_info)
{
	char *s;
	char *path;
	pid_t pid = 0;

	if ((s = getenv("TDSDUMP"))) {
		if (!strlen(s)) {
			pid = getpid();
			if (asprintf(&path, "/tmp/freetds.log.%d", pid) >= 0)
				tds_dstr_set(&connect_info->dump_file, path);
		} else {
			tds_dstr_copy(&connect_info->dump_file, s);
		}
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'dump_file' to '%s' from $TDSDUMP.\n", connect_info->dump_file);
	}
}
static void
tds_config_env_tdsport(TDSCONNECTINFO * connect_info)
{
	char *s;

	if ((s = getenv("TDSPORT"))) {
		connect_info->port = atoi(s);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'port' to %s from $TDSPORT.\n", s);
	}
	return;
}
static void
tds_config_env_tdsver(TDSCONNECTINFO * connect_info)
{
	char *tdsver;

	if ((tdsver = getenv("TDSVER"))) {
		tds_config_verstr(tdsver, connect_info);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'tdsver' to %s from $TDSVER.\n", tdsver);

	}
	return;
}

/* TDSHOST env var, pkleef@openlinksw.com 01/21/02 */
static void
tds_config_env_tdshost(TDSCONNECTINFO * connect_info)
{
	char *tdshost;
	char tmp[256];

	if ((tdshost = getenv("TDSHOST"))) {
		tds_lookup_host(tdshost, tmp);
		tds_dstr_copy(&connect_info->ip_addr, tmp);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'ip_addr' to %s (%s) from $TDSHOST.\n", tmp, tdshost);

	}
	return;
}

/**
 * Set TDS version from given string
 * @param tdsver tds string version
 * @param connect_info where to store information
 */
void
tds_config_verstr(const char *tdsver, TDSCONNECTINFO * connect_info)
{
	if (!strcmp(tdsver, "42") || !strcmp(tdsver, "4.2")) {
		connect_info->major_version = 4;
		connect_info->minor_version = 2;
		return;
	} else if (!strcmp(tdsver, "46") || !strcmp(tdsver, "4.6")) {
		connect_info->major_version = 4;
		connect_info->minor_version = 6;
		return;
	} else if (!strcmp(tdsver, "50") || !strcmp(tdsver, "5.0")) {
		connect_info->major_version = 5;
		connect_info->minor_version = 0;
		return;
	} else if (!strcmp(tdsver, "70") || !strcmp(tdsver, "7.0")) {
		connect_info->major_version = 7;
		connect_info->minor_version = 0;
		return;
	} else if (!strcmp(tdsver, "80") || !strcmp(tdsver, "8.0")) {
		connect_info->major_version = 8;
		connect_info->minor_version = 0;
		return;
	}
}

/**
 * Set the full name of interface file
 * @param interf file name
 */
int
tds_set_interfaces_file_loc(const char *interf)
{
	/* Free it if already set */
	if (interf_file != NULL) {
		free(interf_file);
		interf_file = NULL;
	}
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
 * Given a servername and port name or number, lookup the
 * hostname and service.  The server ip will be stored in the
 * string 'servername' in dotted-decimal notation.  The service port
 * number will be stored in string form in the 'port' parameter.
 *
 * If we can't determine both the IP address and port number then
 * 'ip' and 'port' will be set to empty strings.
 */
/* TODO callers seem to set always connection info... change it */
void
tds_lookup_host(const char *servername,	/* (I) name of the server                  */
		char *ip	/* (O) dotted-decimal ip address of server */
	)
{				/* (O) port number of the service          */
	struct hostent *host = NULL;
	unsigned int ip_addr = 0;

	/* Storage for reentrant getaddrby* calls */
	struct hostent result;
	char buffer[4096];
	int h_errnop;

	/* Only call gethostbyname if servername is not an ip address. 
	 * This call take a while and is useless for an ip address.
	 * mlilback 3/2/02 */
	ip_addr = inet_addr(servername);
	if (ip_addr == INADDR_NONE)
		host = tds_gethostbyname_r(servername, &result, buffer, sizeof(buffer), &h_errnop);

/* TODO this should go in configure */
#ifndef NOREVERSELOOKUPS
/* froy@singleentry.com 12/21/2000 */
	if (host == NULL) {
		char addr[4];
		int a0, a1, a2, a3;

		/* FIXME check results, this code should be executed only if it's an address */
		sscanf(servername, "%d.%d.%d.%d", &a0, &a1, &a2, &a3);
		addr[0] = a0;
		addr[1] = a1;
		addr[2] = a2;
		addr[3] = a3;
		host = tds_gethostbyaddr_r(addr, 4, AF_INET, &result, buffer, sizeof(buffer), &h_errnop);
	}
/* end froy */
#endif
	if (!host) {
		/* if servername is ip, copy to ip. */
		if (INADDR_NONE != ip_addr)
			strncpy(ip, servername, 17);
		else
			ip[0] = '\0';
	} else {
		struct in_addr *ptr = (struct in_addr *) host->h_addr;

#ifdef HAVE_INET_NTOA_R
		inet_ntoa_r(*ptr, ip, 17);
#else
		strncpy(ip, inet_ntoa(*ptr), 17);
#endif
	}
}				/* tds_lookup_host()  */

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
hexdigit(char c)
{
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else if (c >= '0' && c <= '9') {
		return c - '0';
	} else {
		return 0;	/* bad hex digit */
	}
}
static int
hex2num(char *hex)
{
	return hexdigit(hex[0]) * 16 + hexdigit(hex[1]);
}

/* ========================= search_interface_file() =========================
 *
 * Def:  Open and read the file 'file' searching for a logical server
 *       by the name of 'host'.  If one is found then lookup
 *       the IP address and port number and store them in 'ip_addr', and
 *       'ip_port'.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
static int
search_interface_file(TDSCONNECTINFO * connect_info, const char *dir,	/* (I) Name of base directory for interface file */
		      const char *file,	/* (I) Name of the interface file                */
		      const char *host	/* (I) Logical host to search for                */
	)
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

	tdsdump_log(TDS_DBG_INFO1, "%L Searching interfaces file %s/%s.\n", dir, file);
	pathname = (char *) malloc(strlen(dir) + strlen(file) + 10);
	if (!pathname)
		return 0;

	/*
	 * * create the full pathname to the interface file
	 */
	if (file[0] == '\0') {
		pathname[0] = '\0';
	} else {
		if (dir[0] == '\0') {
			pathname[0] = '\0';
		} else {
			strcpy(pathname, dir);
			strcat(pathname, "/");
		}
		strcat(pathname, file);
	}


	/*
	 * *  parse the interfaces file and find the server and port
	 */
	if ((in = fopen(pathname, "r")) == NULL) {
		tdsdump_log(TDS_DBG_INFO1, "%L Couldn't open %s.\n", pathname);
		free(pathname);
		return 0;
	}
	tdsdump_log(TDS_DBG_INFO1, "%L Interfaces file %s opened.\n", pathname);

	while (fgets(line, sizeof(line) - 1, in)) {
		if (line[0] == '#')
			continue;	/* comment */

		if (!isspace(line[0])) {
			field = strtok_r(line, "\n\t ", &lasts);
			if (!strcmp(field, host)) {
				found = 1;
				tdsdump_log(TDS_DBG_INFO1, "%L Found matching entry for host %s.\n", host);
			} else
				found = 0;
		} else if (found && isspace(line[0])) {
			field = strtok_r(line, "\n\t ", &lasts);
			if (field != NULL && !strcmp(field, "query")) {
				field = strtok_r(NULL, "\n\t ", &lasts);	/* tcp or tli */
				if (!strcmp(field, "tli")) {
					tdsdump_log(TDS_DBG_INFO1, "%L TLI service.\n");
					field = strtok_r(NULL, "\n\t ", &lasts);	/* tcp */
					field = strtok_r(NULL, "\n\t ", &lasts);	/* device */
					field = strtok_r(NULL, "\n\t ", &lasts);	/* host/port */
					if (strlen(field) >= 18) {
						sprintf(tmp_port, "%d", hex2num(&field[6]) * 256 + hex2num(&field[8]));
						sprintf(tmp_ip, "%d.%d.%d.%d", hex2num(&field[10]),
							hex2num(&field[12]), hex2num(&field[14]), hex2num(&field[16]));
						tdsdump_log(TDS_DBG_INFO1, "%L tmp_port = %d.mtp_ip = %s.\n", tmp_port, tmp_ip);
					}
				} else {
					field = strtok_r(NULL, "\n\t ", &lasts);	/* ether */
					strcpy(tmp_ver, field);
					field = strtok_r(NULL, "\n\t ", &lasts);	/* host */
					strcpy(tmp_ip, field);
					tdsdump_log(TDS_DBG_INFO1, "%L host field %s.\n", tmp_ip);
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
		tds_lookup_host(tmp_ip, line);
		tdsdump_log(TDS_DBG_INFO1, "%L Resolved IP as '%s'.\n", line);
		tds_dstr_copy(&connect_info->ip_addr, line);
		if (tmp_port[0])
			connect_info->port = tds_lookup_port(tmp_port);
		if (tmp_ver[0])
			tds_config_verstr(tmp_ver, connect_info);
	}
	return server_found;
}				/* search_interface_file()  */

/**
 * Try to find the IP number and port for a (possibly) logical server name.
 *
 * @note This function uses only the interfaces file and is deprecated.
 *
 * ===========================================================================
 */
static void
tds_read_interfaces(const char *server, TDSCONNECTINFO * connect_info)
{
	int founded = 0;

	/* read $SYBASE/interfaces */

	if (!server || strlen(server) == 0) {
		server = getenv("TDSQUERY");
		if (!server || strlen(server) == 0) {
			server = "SYBASE";
		}
		tdsdump_log(TDS_DBG_INFO1, "%L Setting server to %s from $TDSQUERY.\n", server);

	}
	tdsdump_log(TDS_DBG_INFO1, "%L Looking for server %s....\n", server);

	/*
	 * Look for the server in the interf_file iff interf_file has been set.
	 */
	if (interf_file) {
		tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in file %s.\n", interf_file);
		founded = search_interface_file(connect_info, "", interf_file, server);
	}

	/*
	 * if we haven't found the server yet then look for a $HOME/.interfaces file
	 */
	if (!founded) {
		char *path = tds_get_home_file(".interfaces");

		if (path) {
			tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in %s.\n", path);
			founded = search_interface_file(connect_info, "", path, server);
			free(path);
		}
	}

	/*
	 * if we haven't found the server yet then look in $SYBBASE/interfaces file
	 */
	if (!founded) {
		const char *sybase = getenv("SYBASE");

		if (!sybase || !sybase[0])
			sybase = "/etc/freetds";
		tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in %s/interfaces.\n", sybase);
		founded = search_interface_file(connect_info, sybase, "interfaces", server);
	}

	/*
	 * If we still don't have the server and port then assume the user
	 * typed an actual server name.
	 */
	if (!founded) {
		char ip_addr[255];
		int ip_port;
		const char *env_port;

		/*
		 * Make a guess about the port number
		 */

#ifdef TDS50
		ip_port = 4000;
#else
		ip_port = 1433;
#endif
		if ((env_port = getenv("TDSPORT")) != NULL) {
			ip_port = tds_lookup_port(env_port);
			tdsdump_log(TDS_DBG_INFO1, "%L Setting 'ip_port' to %s from $TDSPORT.\n", env_port);
		} else
			tdsdump_log(TDS_DBG_INFO1, "%L Setting 'ip_port' to %d as a guess.\n", ip_port);

		/*
		 * lookup the host
		 */
		tds_lookup_host(server, ip_addr);
		if (ip_addr[0])
			tds_dstr_copy(&connect_info->ip_addr, ip_addr);
		if (ip_port)
			connect_info->port = ip_port;
	}
}

/**
 * Check the server name to find port info first
 * return 1 when found, else 0
 * Warning: connect_info-> & login-> are all modified when needed
 */
static int
parse_server_name_for_port(TDSCONNECTINFO * connect_info, TDSLOGIN * login)
{
	char *pSep, *pEnd;
	char *server;

	/* seek the ':' in login server_name */
	server = tds_dstr_cstr(&login->server_name);
	pEnd = server + strlen(server);
	for (pSep = server; pSep < pEnd; pSep++)
		if (*pSep == ':')
			break;

	if ((pSep < pEnd) && (pSep != server)) {	/* yes, i found it! */
		if (!tds_dstr_copyn(&connect_info->server_name, server, pSep - server))	/* end the server_name before the ':' */
			return 0;	/* FALSE */

		/* modify connect_info-> && login->server_name & ->port */
		login->port = connect_info->port = atoi(pSep + 1);
		*pSep = 0;

		/* connect_info->ip_addr needed */
		{
			char tmp[256];

			tds_lookup_host(tds_dstr_cstr(&connect_info->server_name), tmp);
			if (!tds_dstr_copy(&connect_info->ip_addr, tmp))
				return 0;	/* FALSE */
		}

		return 1;	/* TRUE */
	} else
		return 0;	/* FALSE */
}


/**
 * Return a structure capturing the compile-time settings provided to the
 * configure script.  
 */

const TDS_COMPILETIME_SETTINGS *
tds_get_compiletime_settings(void)
{
	static TDS_COMPILETIME_SETTINGS settings = {
		TDS_VERSION_NO, "unknown"	/* need fancy script in makefile */
#		ifdef MSDBLIB
			, 1
#		else
			, 0
#		endif
			, -1	/* unknown: sybase_compat only a makefile setting, so far. */
#		ifdef THREAD_SAFE
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
#		else
#		ifdef TDS50
			, "5.0"
#		else
#		ifdef TDS70
			, "7.0"
#		else
#		ifdef TDS80
			, "8.0"
#		else
			, "4.2"
#		endif
#		endif
#		endif
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

/** \@} */

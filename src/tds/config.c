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
#include "tdsutil.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: config.c,v 1.45 2002-10-17 20:45:45 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


static void tds_config_login(TDSCONNECTINFO *connect_info, TDSLOGIN *login);
static void tds_config_env_dsquery(TDSCONNECTINFO *connect_info);
static void tds_config_env_tdsdump(TDSCONNECTINFO *connect_info);
static void tds_config_env_tdsver(TDSCONNECTINFO *connect_info);
static void tds_config_env_tdsport(TDSCONNECTINFO *connect_info);
static void tds_config_env_tdshost(TDSCONNECTINFO *connect_info);
static int tds_read_conf_file(char *server, TDSCONNECTINFO *connect_info);
static int tds_read_conf_sections(FILE *in, char *server, TDSCONNECTINFO *connect_info);
static int tds_read_conf_section(FILE *in, char *section, TDSCONNECTINFO *connect_info);
static void tds_read_interfaces(char *server, TDSCONNECTINFO *connect_info);
static void tds_config_verstr(char *tdsver, TDSCONNECTINFO *connect_info);
static int tds_config_boolean(char *value);
static void lookup_host(const char *servername, const char *portname, char *ip, char *port);
static int parse_server_name_for_port( TDSCONNECTINFO *connect_info, TDSLOGIN *login );
static int get_server_info(char *server, char *ip_addr, char *ip_port, char *tds_ver);

extern int g_append_mode;

static char *interf_file = NULL;

/*
** tds_read_config_info() will fill the tds connect_info structure based on configuration 
** information gathered in the following order:
** 1) Program specified in TDSLOGIN structure
** 2) The environment variables TDSVER, TDSDUMP, TDSPORT, TDSQUERY, TDSHOST
** 3) A config file with the following search order:
**    a) a readable file specified by environment variable FREETDSCONF
**    b) a readable file in ~/.freetds.conf
**    c) a readable file in $prefix/etc/freetds.conf
** 3) ~/.interfaces if exists
** 4) $SYBASE/interfaces if exists
** 5) TDS_DEF_* default values
**
** .tdsrc and freetds.conf have been added to make the package easier to 
** integration with various Linux and *BSD distributions.
*/ 
TDSCONNECTINFO *tds_read_config_info(TDSSOCKET *tds, TDSLOGIN *login, TDSLOCINFO *locale)
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
	if (! tds_read_conf_file(login->server_name, connect_info)) {
		/* fallback to interfaces file */
		tdsdump_log(TDS_DBG_INFO1, "%L Failed in reading conf file.  Trying interface files.\n");
		tds_read_interfaces(login->server_name, connect_info);
	}

	if( parse_server_name_for_port( connect_info, login ) ) {
		tdsdump_log(TDS_DBG_INFO1, "Parsed servername, now %s on %d.\n", connect_info->server_name, login->port);
	}
	
	/* Now check the environment variables */
	tds_config_env_tdsver(connect_info);
	tds_config_env_tdsdump(connect_info);
	tds_config_env_tdsport(connect_info);
	tds_config_env_dsquery(connect_info);
	tds_config_env_tdshost(connect_info);
	
	/* And finally the login structure */
	tds_config_login(connect_info, login);

        if (opened) {
		tdsdump_close();
	}
	return connect_info;
}        

static int tds_try_conf_file(char *path, char *how, char *server, TDSCONNECTINFO *connect_info)
{
int found = 0;
FILE *in;

	if ((in = fopen (path, "r")) != NULL) {
		tdsdump_log(TDS_DBG_INFO1, 
			"%L Found conf file '%s' %s. Reading section '%s'.\n",
			path, how, server);
		found = tds_read_conf_sections (in, server, connect_info);
		
		if(found) {
			tdsdump_log(TDS_DBG_INFO1, "%L ...Success.\n");
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ...'%s' not found.\n", server);
		}
		
		fclose (in);
	}
	return found;
}

static int
tds_read_conf_file(char *server, TDSCONNECTINFO *connect_info)
{
char  *home, *path = NULL;
int found = 0; 

	if (interf_file) {
		found = tds_try_conf_file(interf_file,"set programmatically", server, connect_info);
	}

	/* FREETDSCONF env var, pkleef@openlinksw.com 01/21/02 */
	if (!found) {
		path = getenv ("FREETDSCONF");
		if (path) {
			found = tds_try_conf_file(path, "(from $FREETDSCONF)", server, connect_info);
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ...$FREETDSCONF not set.  Trying $HOME.\n");
		}
	}

	if (!found) {
		/* FIXME use getpwent for security */
		home = getenv("HOME");
		if (home && home[0]!='\0') {
			if (asprintf(&path,"%s/.freetds.conf",home) < 0) {
				/* out of memory condition; don't attempt a log message */
				/* should we try the OS log? */
				fprintf(stderr, "config.c (line %d): no memory\n", __LINE__);
				return 0;
			}
			found = tds_try_conf_file(path, "(.freetds.conf)", server, connect_info);
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ...$HOME not set.  Trying %s.\n", FREETDS_SYSCONFFILE);
		}
	}

	if (!found) {
		found = tds_try_conf_file(FREETDS_SYSCONFFILE, "(default)", server, connect_info);
	} 

	return found;
}
static int tds_read_conf_sections(FILE *in, char *server, TDSCONNECTINFO *connect_info)
{
unsigned char *section;
int i, found = 0;

	tds_read_conf_section(in, "global", connect_info);
	rewind(in);
	section = strdup(server);
	for (i = 0; i < strlen(section); i++)
		section[i] = tolower(section[i]);
	found = tds_read_conf_section(in, section, connect_info);
	free(section);

	return found;
}
static int tds_config_boolean(char *value) 
{
	if (!strcmp(value, "yes") ||
		!strcmp(value, "on") ||
		!strcmp(value, "true") ||
		!strcmp(value, "1")) {
                tdsdump_log(TDS_DBG_INFO1, "%L %s is a 'yes/on/true'.\n",value);
                return 1;
	} else {
                tdsdump_log(TDS_DBG_INFO1, "%L %s is a 'no/off/false'.\n",value);
                return 0;
	}
}

static int tds_read_conf_section(FILE *in, char *section, TDSCONNECTINFO *connect_info)
{
char line[256], option[256], value[256];
unsigned char *s;
unsigned char p;
int i;
int insection = 0;
char tmp[256];
int found = 0;

        tdsdump_log(TDS_DBG_INFO1, "%L Looking for section %s.\n", section);
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
				value[i++]=*s;
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
                                *s = tolower(*s);
				s++;
			}
                        tdsdump_log(TDS_DBG_INFO1, "%L ... Found section %s.\n", &option[1]);

			if (!strcmp(section, &option[1])) {
				tdsdump_log(TDS_DBG_INFO1, "%L Got a match.\n");
				insection=1;
				found=1;
			} else {
				insection=0;
			}
		} else if (insection) {
                    /* fprintf(stderr,"option = '%s' value = '%s'\n", option, value); */
                        tdsdump_log(TDS_DBG_INFO1, "%L option = '%s' value = '%s'.\n", option, value);
			if (!strcmp(option,TDS_STR_VERSION)) {
				tds_config_verstr(value, connect_info);
			} else if (!strcmp(option,TDS_STR_BLKSZ)) {
				if (atoi(value)) 
					connect_info->block_size = atoi(value);
			} else if (!strcmp(option,TDS_STR_SWAPDT)) {
				connect_info->broken_dates = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_SWAPMNY)) {
				connect_info->broken_money = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_TRYSVR)) {
				connect_info->try_server_login = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_TRYDOM)) {
				connect_info->try_domain_login = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_DOMAIN)) {
				if (connect_info->default_domain) free(connect_info->default_domain);
				connect_info->default_domain = strdup(value);
			} else if (!strcmp(option,TDS_STR_XDOMAUTH)) {
				connect_info->xdomain_auth = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_DUMPFILE)) {
				if (connect_info->dump_file) free(connect_info->dump_file);
				connect_info->dump_file = strdup(value);
			} else if (!strcmp(option,TDS_STR_DEBUGLVL)) {
				if (atoi(value)) 
					connect_info->debug_level = atoi(value);
			} else if (!strcmp(option,TDS_STR_TIMEOUT )) {
				if (atoi(value)) 
					connect_info->timeout = atoi(value);
			} else if (!strcmp(option,TDS_STR_CONNTMOUT)) {
				if (atoi(value)) 
					connect_info->connect_timeout = atoi(value);
			} else if (!strcmp(option,TDS_STR_HOST)) {
				tdsdump_log(TDS_DBG_INFO1, "%L Found host entry %s.\n",value);
   				lookup_host(value, NULL, tmp, NULL);
				if (connect_info->ip_addr) free(connect_info->ip_addr);
				connect_info->ip_addr = strdup(tmp);
				tdsdump_log(TDS_DBG_INFO1, "%L IP addr is %s.\n",connect_info->ip_addr);
			} else if (!strcmp(option,TDS_STR_PORT)) {
				if (atoi(value)) 
					connect_info->port = atoi(value);
			} else if (!strcmp(option,TDS_STR_EMUL_LE)) {
				connect_info->emul_little_endian = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_TEXTSZ)) {
				if (atoi(value)) 
					connect_info->text_size = atoi(value);
			} else if (!strcmp(option,TDS_STR_CHARSET)) {
				if (connect_info->char_set) free(connect_info->char_set);
				connect_info->char_set = strdup(value);
			} else if (!strcmp(option,TDS_STR_CLCHARSET)) {
				if (connect_info->client_charset) free(connect_info->client_charset);
				connect_info->client_charset = strdup(value);
			} else if (!strcmp(option,TDS_STR_LANGUAGE)) {
				if (connect_info->language) free(connect_info->language);
				connect_info->language = strdup(value);
			} else if (!strcmp(option,TDS_STR_APPENDMODE)) {
				g_append_mode = tds_config_boolean(value);
			}
                        else tdsdump_log(TDS_DBG_INFO1, "%L UNRECOGNIZED option '%s'...ignoring.\n", option);
		}

	}
	return found;
}

static void tds_read_interfaces(char *server, TDSCONNECTINFO *connect_info)
{
char ip_addr[255], ip_port[255], tds_ver[255];

	/* read $SYBASE/interfaces */
	/* This needs to be cleaned up */
	get_server_info(server, ip_addr, ip_port, tds_ver);
	if (strlen(ip_addr)) {
		if (connect_info->ip_addr) free(connect_info->ip_addr);
		/* FIXME check result, use strdup */
		connect_info->ip_addr = (char *) malloc(strlen(ip_addr)+1);
		strcpy(connect_info->ip_addr, ip_addr);
	}
	if (atoi(ip_port)) {
		connect_info->port = atoi(ip_port);
	}
	if (strlen(tds_ver)) {
		tds_config_verstr(tds_ver, connect_info);
		/* if it doesn't match a known version do nothing with it */
	}	
}
static void tds_config_login(TDSCONNECTINFO *connect_info, TDSLOGIN *login)
{
	if (!tds_dstr_isempty(&login->server_name)) {
		if (connect_info->server_name) free(connect_info->server_name);
		connect_info->server_name = strdup(login->server_name);
	}	
	if (login->major_version || login->minor_version) {
		connect_info->major_version = login->major_version;
		connect_info->minor_version = login->minor_version;
	}
	if (!tds_dstr_isempty(&login->language)) {
		if (connect_info->language) free(connect_info->language);
		connect_info->language = strdup(login->language);
	}
	if (!tds_dstr_isempty(&login->char_set)) {
		if (connect_info->char_set) free(connect_info->char_set);
		connect_info->char_set = strdup(login->char_set);
	}
	if (!tds_dstr_isempty(&login->host_name)) {
		if (connect_info->host_name) free(connect_info->host_name);
		connect_info->host_name = strdup(login->host_name);
		/* DBSETLHOST and it's equivilants are commentary fields
		** they don't affect connect_info->ip_addr (the server) but they show
		** up in an sp_who as the *clients* hostname.  (bsb, 11/10) 
		*/
		/* should work with IP (mlilback, 11/7/01) */
		/*
		if (connect_info->ip_addr) free(connect_info->ip_addr);
		connect_info->ip_addr = calloc(sizeof(char),18);
		lookup_host(connect_info->host_name, NULL, connect_info->ip_addr, NULL);
		*/
	}
	if (!tds_dstr_isempty(&login->app_name)) {
		if (connect_info->app_name) free(connect_info->app_name);
		connect_info->app_name = strdup(login->app_name);
	}
	if (!tds_dstr_isempty(&login->user_name)) {
		if (connect_info->user_name) free(connect_info->user_name);
		connect_info->user_name = strdup(login->user_name);
	}
	if (!tds_dstr_isempty(&login->password)) {
		if (connect_info->password) {
			/* for security reason clear memory */
			memset(connect_info->password,0,strlen(connect_info->password));
			free(connect_info->password);
		}
		connect_info->password = strdup(login->password);
	}
	if (!tds_dstr_isempty(&login->library)) {
		if (connect_info->library) free(connect_info->library);
		connect_info->library = strdup(login->library);
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

}

static void tds_config_env_dsquery(TDSCONNECTINFO *connect_info)
{
char *s;

	if ((s=getenv("TDSQUERY"))) {
		if (s && strlen(s)) {
			if (connect_info->server_name) free(connect_info->server_name);
			connect_info->server_name = strdup(s);
			tdsdump_log(TDS_DBG_INFO1, "%L Setting 'server_name' to '%s' from $TDSQUERY.\n",s);
		} 
		return;
	}
	if ((s=getenv("DSQUERY"))) {
		if (s && strlen(s)) {
			if (connect_info->server_name) free(connect_info->server_name);
			connect_info->server_name = strdup(s);
			tdsdump_log(TDS_DBG_INFO1, "%L Setting 'server_name' to '%s' from $DSQUERY.\n",s);
		} 
	}
}
static void tds_config_env_tdsdump(TDSCONNECTINFO *connect_info)
{
char *s;
char *path;
pid_t pid=0;

        if ((s=getenv("TDSDUMP"))) {
                if (!strlen(s)) {
                        pid = getpid();
			if (connect_info->dump_file) free(connect_info->dump_file);
			/* FIXME check out of memory */
			asprintf(&path, "/tmp/freetds.log.%d", pid);
			connect_info->dump_file = path;
		} else {
			if (connect_info->dump_file) free(connect_info->dump_file);
			connect_info->dump_file = strdup(s);
		}
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'dump_file' to '%s' from $TDSDUMP.\n",connect_info->dump_file);
	}
}
static void tds_config_env_tdsport(TDSCONNECTINFO *connect_info)
{
char *s;

	if ((s=getenv("TDSPORT"))) {
		connect_info->port=atoi(s);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'port' to %s from $TDSPORT.\n",s);
	}
	return;
}
static void tds_config_env_tdsver(TDSCONNECTINFO *connect_info)
{
char *tdsver;

	if ((tdsver=getenv("TDSVER"))) {
		tds_config_verstr(tdsver, connect_info);
		tdsdump_log(TDS_DBG_INFO1, "%L Setting 'tdsver' to %s from $TDSVER.\n",tdsver);

	}
	return;
}
/* TDSHOST env var, pkleef@openlinksw.com 01/21/02 */
static void tds_config_env_tdshost(TDSCONNECTINFO *connect_info)
{
char *tdshost;
char tmp[256];

	if ((tdshost=getenv("TDSHOST"))) {
		lookup_host (tdshost, NULL, tmp, NULL);
		if (connect_info->ip_addr)
			free (connect_info->ip_addr);
		connect_info->ip_addr = strdup (tmp);
                tdsdump_log(TDS_DBG_INFO1, "%L Setting 'ip_addr' to %s (%s) from $TDSHOST.\n",tmp, tdshost);

	}
	return;
}

static void tds_config_verstr(char *tdsver, TDSCONNECTINFO *connect_info)
{
	if (!strcmp(tdsver,"42") || !strcmp(tdsver,"4.2")) {
		connect_info->major_version=4;
		connect_info->minor_version=2;
		return;
	} else if (!strcmp(tdsver,"46") || !strcmp(tdsver,"4.6")) {
		connect_info->major_version=4;
		connect_info->minor_version=6;
		return;
	} else if (!strcmp(tdsver,"50") || !strcmp(tdsver,"5.0")) {
		connect_info->major_version=5;
		connect_info->minor_version=0;
		return;
	} else if (!strcmp(tdsver,"70") || !strcmp(tdsver,"7.0")) {
		connect_info->major_version=7;
		connect_info->minor_version=0;
		return;
	} else if (!strcmp(tdsver,"80") || !strcmp(tdsver,"8.0")) {
		connect_info->major_version=8;
		connect_info->minor_version=0;
		return;
	}
}

int
tds_set_interfaces_file_loc(char *interf)
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

/* ============================== lookup_host() ==============================
 *
 * Def:  Given a servername and port name or number, lookup the
 *       hostname and service.  The server ip will be stored in the
 *       string 'servername' in dotted-decimal notation.  The service port
 *       number will be stored in string form in the 'port' parameter.
 *
 *       If we can't determine both the IP address and port number then
 *       'ip' and 'port' will be set to empty strings.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
static void lookup_host(
   const char  *servername,  /* (I) name of the server                  */
   const char  *portname,    /* (I) name or number of the port          */
   char        *ip,          /* (O) dotted-decimal ip address of server */
   char        *port)        /* (O) port number of the service          */
{
   struct hostent   *host    = NULL;
   struct servent   *service = NULL;
   int               num     = 0;
   unsigned int		ip_addr=0;

   /* Storage for reentrant getaddrby* calls */
   struct hostent result;
   char buffer[4096];
   int h_errnop;

   /* Storage for reentrant getservbyname */
   struct servent serv_result;

   /* Only call gethostbyname if servername is not an ip address. 
      This call take a while and is useless for an ip address.
      mlilback 3/2/02 */
   ip_addr = inet_addr(servername);
   if (ip_addr == INADDR_NONE)
      host = tds_gethostbyname_r(servername, &result, buffer, sizeof(buffer), &h_errnop);
	
#ifndef NOREVERSELOOKUPS
/* froy@singleentry.com 12/21/2000 */
	if (host==NULL) {
		char addr [4];
		int a0, a1, a2, a3;
		sscanf (servername, "%d.%d.%d.%d", &a0, &a1, &a2, &a3);
		addr [0] = a0;
		addr [1] = a1;
		addr [2] = a2;
		addr [3] = a3;
		host    = tds_gethostbyaddr_r (addr, 4, AF_INET, &result, buffer, sizeof(buffer), &h_errnop);
	}
/* end froy */ 
#endif
   if (!host) {
      /* if servername is ip, copy to ip. */
      if (INADDR_NONE != ip_addr)
         strncpy(ip, servername, 17); 
      else
         ip[0]   = '\0';
   } else {
      struct in_addr *ptr = (struct in_addr *) host->h_addr;
      strncpy(ip, inet_ntoa(*ptr), 17);
   }
   if (portname) {
   	 service = tds_getservbyname_r(portname, "tcp", &serv_result, buffer, sizeof(buffer));
      if (service==NULL) {
         num = atoi(portname);
      } else {
         num = ntohs(service->s_port);
      }
   }

   if (num==0) {
	if (port) port[0] = '\0';
   } else {
	sprintf(port, "%d", num);
   }
} /* lookup_host()  */

static int hexdigit(char c)
{
	if (c>='a' && c<='f') {
		return c - 'a' + 10;
	} else if (c>='A' && c<='F') {
		return c - 'A' + 10;
	} else if (c>='0' && c<='9') {
		return c - '0';
	} else {
		return 0; /* bad hex digit */
	}
}
static int hex2num(char *hex)
{
	return hexdigit(hex[0])*16 + hexdigit(hex[1]);	
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
static void search_interface_file(
   const char *dir,     /* (I) Name of base directory for interface file */
   const char *file,    /* (I) Name of the interface file                */
   const char *host,    /* (I) Logical host to search for                */
   char       *ip_addr, /* (O) dotted-decimal IP address                 */
   char       *ip_port, /* (O) Port number for database server           */
   char       *tds_ver) /* (O) Protocol version to use when connecting   */
{
char  *pathname;
unsigned char line[255];
char  tmp_ip[sizeof(line)];
char  tmp_port[sizeof(line)];
char  tmp_ver[sizeof(line)];
FILE *in;
char *field;
int   found=0;
char *lasts;

	ip_addr[0]  = '\0';
	ip_port[0]  = '\0';
	line[0]     = '\0';
	tmp_ip[0]   = '\0';
	tmp_port[0] = '\0';
	tmp_ver[0]  = '\0';

	tdsdump_log(TDS_DBG_INFO1, "%L Searching interfaces file %s/%s.\n",dir,file);
	/* FIXME check result or use open fchdir fopen ... */
	pathname = (char *) malloc(strlen(dir) + strlen(file) + 10);
   
	/*
	* create the full pathname to the interface file
	*/
	/* FIXME file and dir can't be NULL, used before in strlen */
	if (file==NULL || file[0]=='\0') {
		pathname[0] = '\0';
	} else {
		if (dir==NULL || dir[0]=='\0') {
			pathname[0] = '\0';
		} else {
			strcpy(pathname, dir);
			strcat(pathname, "/");
		}
		strcat(pathname, file);
	}


	/*
	*  parse the interfaces file and find the server and port
	*/
	if ((in = fopen(pathname,"r"))==NULL) {
                tdsdump_log(TDS_DBG_INFO1, "%L Couldn't open %s.\n", pathname);
                free(pathname);
		return;
	}
	tdsdump_log(TDS_DBG_INFO1, "%L Interfaces file %s opened.\n", pathname);

	while (fgets(line,sizeof(line)-1,in)) {
		if (line[0]=='#') continue; /* comment */

		if (!isspace(line[0])) {
			field = strtok_r(line, "\n\t ", &lasts);
			if (!strcmp(field,host)) {
				found=1;
				tdsdump_log(TDS_DBG_INFO1, "%L Found matching entry for host %s.\n,host");
			}
			else found=0;
		} else if (found && isspace(line[0])) {
			field = strtok_r(line, "\n\t ", &lasts);
			if (field!=NULL && !strcmp(field,"query")) {
				field = strtok_r(NULL,"\n\t ", &lasts); /* tcp or tli */
				if (!strcmp(field,"tli")) {
					tdsdump_log(TDS_DBG_INFO1, "%L TLI service.\n");
					field = strtok_r(NULL, "\n\t ", &lasts); /* tcp */
					field = strtok_r(NULL, "\n\t ", &lasts); /* device */
					field = strtok_r(NULL, "\n\t ", &lasts); /* host/port */
					if (strlen(field)>=18) {
						sprintf(tmp_port,"%d", hex2num(&field[6])*256 + 
							hex2num(&field[8]));
						sprintf(tmp_ip,"%d.%d.%d.%d", hex2num(&field[10]),
							hex2num(&field[12]), hex2num(&field[14]),
							hex2num(&field[16]));
					        tdsdump_log(TDS_DBG_INFO1, "%L tmp_port = %d.mtp_ip = %s.\n", tmp_port, tmp_ip);
					}
				} else {
					field = strtok_r(NULL, "\n\t ", &lasts); /* ether */
					strcpy(tmp_ver,field);
					field = strtok_r(NULL, "\n\t ", &lasts); /* host */
					strcpy(tmp_ip,field);
					tdsdump_log(TDS_DBG_INFO1, "%L host field %s.\n",tmp_ip);
					field = strtok_r(NULL, "\n\t ", &lasts); /* port */
					strcpy(tmp_port,field);
				} /* else */
			} /* if */
		} /* else if */
	} /* while */
fclose(in);
free(pathname);


   /*
    * Look up the host and service
    */
   lookup_host(tmp_ip, tmp_port, ip_addr, ip_port);
   tdsdump_log(TDS_DBG_INFO1, "%L Resolved IP as '%s'.\n",ip_addr);
   strcpy(tds_ver,tmp_ver);
} /* search_interface_file()  */


/* ============================ get_server_info() ============================
 *
 * Def:  Try to find the IP number and port for a (possibly) logical server
 *       name.
 *
 * Note: It is the callers responsibility to supply large enough buffers
 *       to hold the ip and port numbers.  ip_addr should be at least 17
 *       bytes long and ip_port should be at least 6 bytes long.
 *
 * Note: This function uses only the interfaces file and is deprecated.
 *
 * Ret:  True if it found the server, false otherwise.
 *
 * ===========================================================================
 */
int get_server_info(
   char *server,   /* (I) logical or physical server name      */
   char *ip_addr,  /* (O) string representation of IP address  */
   char *ip_port,  /* (O) string representation of port number */
   char *tds_ver)  /* (O) string value specifying which protocol version */
{
	ip_addr[0] = '\0';
	ip_port[0] = '\0';
	tds_ver[0] = '\0';

        tdsdump_log(TDS_DBG_INFO1, "%L Looking for server....\n");
	if (!server || strlen(server) == 0) {
		server = getenv("TDSQUERY");
		if(!server || strlen(server) == 0) {
			server = "SYBASE";
		}
                tdsdump_log(TDS_DBG_INFO1, "%L Setting server to %s from $TDSQUERY.\n",server);

	}

	/*
	* Look for the server in the interf_file iff interf_file has been set.
	*/
	if (ip_addr[0]=='\0' && interf_file) {
                tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in interf_file %s.\n",interf_file);
                search_interface_file("", interf_file, server, ip_addr,
                ip_port, tds_ver);
	}

	/*
	* if we haven't found the server yet then look for a $HOME/.interfaces file
	*/
	if (ip_addr[0]=='\0') {
		/* FIXME use getpwent, see above */
		char  *home = getenv("HOME");
		if (home!=NULL && home[0]!='\0') {
                        tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in %s/.interfaces.\n", home);
			search_interface_file(home, ".interfaces", server, ip_addr,
				ip_port, tds_ver);
		}
	}

	/*
	* if we haven't found the server yet then look in $SYBBASE/interfaces file
	*/
	if (ip_addr[0]=='\0') {
		char  *sybase = getenv("SYBASE");
		if (sybase!=NULL && sybase[0]!='\0') {
                        tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in %s/interfaces.\n", sybase);
			search_interface_file(sybase, "interfaces", server, ip_addr,
				ip_port, tds_ver);
		} else {
                        tdsdump_log(TDS_DBG_INFO1, "%L Looking for server in /etc/freetds/interfaces.\n");
			search_interface_file("/etc/freetds", "interfaces", server,
				ip_addr, ip_port, tds_ver);
		}
	}

 	/*
 	* If we still don't have the server and port then assume the user
 	* typed an actual server name.
 	*/
 	if (ip_addr[0]=='\0') {
 		char  *tmp_port;

		/*
		* Make a guess about the port number
		*/

#ifdef TDS50
		tmp_port = "4000";
#else
		tmp_port = "1433";
#endif
		/* FIX ME -- Need a symbolic constant for the environment variable */
		if (getenv("TDSPORT")!=NULL) {
			tmp_port = getenv("TDSPORT");
                        tdsdump_log(TDS_DBG_INFO1, "%L Setting 'tmp_port' to %s from $TDSPORT.\n",tmp_port);
		}
                else tdsdump_log(TDS_DBG_INFO1, "%L Setting 'tmp_port' to %s as a guess.\n",tmp_port);


		/*
		* lookup the host and service
		*/
		lookup_host(server, tmp_port, ip_addr, ip_port);

	}

	return ip_addr[0]!='\0' && ip_port[0]!='\0';
} /* get_server_info()  */

/**
 * Check the server name to find port info first
 * return 1 when found, else 0
 * Warning: connect_info-> & login-> are all modified when needed
 */
static int parse_server_name_for_port( TDSCONNECTINFO *connect_info, TDSLOGIN *login )
{
    char *pSep, *pEnd;
        
    if( ! login->server_name )
        return 0;/* FALSE */
    
    /* seek the ':' in login server_name */
    pEnd = login->server_name + strlen( login->server_name );
    for( pSep = login->server_name; pSep < pEnd; pSep ++ )
        if( *pSep == ':' ) break;
    
    if(( pSep < pEnd )&&( pSep != login->server_name ))/* yes, i found it! */
    {
		if( connect_info->server_name ) free( connect_info->server_name );
		connect_info->server_name = strdup( login->server_name );

		/* modify connect_info-> && login->server_name & ->port */
		login->port = connect_info->port = atoi( pSep + 1 );
		connect_info->server_name[pSep - login->server_name] = 0;/* end the server_name before the ':' */
        *pSep = 0;

		/* connect_info->ip_addr needed */
        {
            char tmp[256];

		lookup_host (connect_info->server_name, NULL, tmp, NULL);
		if (connect_info->ip_addr)
			free (connect_info->ip_addr);
		connect_info->ip_addr = strdup (tmp);
        }

        return 1;/* TRUE */
    }
    else
        return 0;/* FALSE */
}


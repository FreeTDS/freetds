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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#ifdef __DGUX__
#include <paths.h>
#endif
#ifdef __FreeBSD__
#include <sys/time.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#define PATH_MAX 255
#endif
#ifndef WIN32
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "tds.h"
#include "tdsutil.h"

static char  software_version[]   = "$Id: config.c,v 1.9 2002-01-22 03:28:17 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


static void tds_config_login(TDSCONFIGINFO *config, TDSLOGIN *login);
static void tds_config_env_dsquery(TDSCONFIGINFO *config);
static void tds_config_env_tdsdump(TDSCONFIGINFO *config);
static void tds_config_env_tdsver(TDSCONFIGINFO *config);
static void tds_config_env_tdsport(TDSCONFIGINFO *config);
static void tds_config_env_tdshost(TDSCONFIGINFO *config);
static int tds_read_conf_file(char *server, TDSCONFIGINFO *config);
static int tds_read_conf_sections(FILE *in, char *server, TDSCONFIGINFO *config);
static int tds_read_conf_section(FILE *in, char *section, TDSCONFIGINFO *config);
static void tds_read_interfaces(char *server, TDSCONFIGINFO *config);
static void tds_config_verstr(char *tdsver, TDSCONFIGINFO *config);
static int tds_config_boolean(char *value);
static void lookup_host(const char *servername, const char *portname, char *ip, char *port);

extern int g_append_mode;

static char interf_file[MAXPATH];

#define DEBUG_CONFIG 0

/*
** tds_get_config() will fill the tds config structure based on configuration 
** information gathered in the following order:
** 1) Program specified in TDSLOGIN structure
** 2) The environment variables TDSVER, TDSDUMP, TDSPORT
** 3) ~/.interfaces if exists
** 4) $SYBASE/interfaces if exists
** 5) TDS_DEF_* default values
**
** .tdsrc and freetds.conf have been added to make the package easier to 
** integration with various Linux and *BSD distributions.
*/ 
TDSCONFIGINFO *tds_get_config(TDSSOCKET *tds, TDSLOGIN *login)
{
TDSCONFIGINFO *config;


	/* allocate a new structure with hard coded and build-time defaults */
	config = tds_alloc_config();

#if DEBUG_CONFIG
	tdsdump_open("/tmp/tdsconfig.log");
#endif
	tdsdump_log(TDS_DBG_INFO1, "%L Attempting to read conf file\n");
	if (! tds_read_conf_file(login->server_name, config)) {
		/* fallback to interfaces file */
		tdsdump_log(TDS_DBG_INFO1, "%L Failed reading conf file.  Trying interfaces\n");
		tds_read_interfaces(login->server_name, config);
	}

	/* Now check the environment variables */
	tds_config_env_tdsver(config);
	tds_config_env_tdsdump(config);
	tds_config_env_tdsport(config);
	tds_config_env_dsquery(config);
	tds_config_env_tdshost(config);
	
	/* And finally the login structure */
	tds_config_login(config, login);

#if DEBUG_CONFIG
	tdsdump_close();
#endif

	return config;
}
static int tds_read_conf_file(char *server, TDSCONFIGINFO *config)
{
FILE *in;
char  *home, *path;
int found = 0; 

	in = fopen(FREETDS_SYSCONFFILE, "r");
	if (in) {
		tdsdump_log(TDS_DBG_INFO1, "%L Found conf file in %s reading sections\n",FREETDS_SYSCONFFILE);
		found = tds_read_conf_sections(in, server, config);
		fclose(in);
	}
	
	/* FREETDSCONF env var, pkleef@openlinksw.com 01/21/02 */
	path = getenv ("FREETDSCONF");
	if (!found && path) {
		if ((in = fopen (path, "r")) != NULL) {
			tdsdump_log(TDS_DBG_INFO1, 
				"%L Found conf file in %s reading sections\n",path);
			found = tds_read_conf_sections (in, server, config);
			fclose (in);
		}
	}

	if (!found) {
		home = getenv("HOME");
		if (home!=NULL && home[0]!='\0') {
			path = malloc(strlen(home) + 14 + 1); /* strlen("/.freetds.conf")=14 */
			sprintf(path,"%s/.freetds.conf",home);
			in = fopen(path, "r");
			if (in) {
				tdsdump_log(TDS_DBG_INFO1, "%L Found conf file in %s/.freetds.conf reading sections\n",home);
				found = tds_read_conf_sections(in, server, config);
				fclose(in);
			}
			free(path);
		}
	}

	return found;
}
static int tds_read_conf_sections(FILE *in, char *server, TDSCONFIGINFO *config)
{
char *section;
int i, found = 0;

	tds_read_conf_section(in, "global", config);
	rewind(in);
	section = strdup(server);
	for (i=0;i<strlen(section);i++) section[i]=tolower(section[i]);
	found = tds_read_conf_section(in, section, config);
	free(section);

	return found;
}
static int tds_config_boolean(char *value) 
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

static int tds_read_conf_section(FILE *in, char *section, TDSCONFIGINFO *config)
{
char line[256], option[256], value[256], *s;
int i;
char p;
int insection = 0;
char tmp[256];
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
			if (!strcmp(option,TDS_STR_VERSION)) {
				tds_config_verstr(value, config);
			} else if (!strcmp(option,TDS_STR_BLKSZ)) {
				if (atoi(value)) 
					config->block_size = atoi(value);
			} else if (!strcmp(option,TDS_STR_SWAPDT)) {
				config->broken_dates = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_SWAPMNY)) {
				config->broken_money = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_TRYSVR)) {
				config->try_server_login = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_TRYDOM)) {
				config->try_domain_login = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_DOMAIN)) {
				if (config->default_domain) free(config->default_domain);
				config->default_domain = strdup(value);
			} else if (!strcmp(option,TDS_STR_XDOMAUTH)) {
				config->xdomain_auth = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_DUMPFILE)) {
				if (config->dump_file) free(config->dump_file);
				config->dump_file = strdup(value);
			} else if (!strcmp(option,TDS_STR_DEBUGLVL)) {
				if (atoi(value)) 
					config->debug_level = atoi(value);
			} else if (!strcmp(option,TDS_STR_TIMEOUT )) {
				if (atoi(value)) 
					config->timeout = atoi(value);
			} else if (!strcmp(option,TDS_STR_CONNTMOUT)) {
				if (atoi(value)) 
					config->connect_timeout = atoi(value);
			} else if (!strcmp(option,TDS_STR_HOST)) {
				tdsdump_log(TDS_DBG_INFO1, "%L Found host entry %s\n",value);
   				lookup_host(value, NULL, tmp, NULL);
				if (config->ip_addr) free(config->ip_addr);
				config->ip_addr = strdup(tmp);
				tdsdump_log(TDS_DBG_INFO1, "%L IP addr is %s\n",config->ip_addr);
			} else if (!strcmp(option,TDS_STR_PORT)) {
				if (atoi(value)) 
					config->port = atoi(value);
			} else if (!strcmp(option,TDS_STR_EMUL_LE)) {
				config->emul_little_endian = tds_config_boolean(value);
			} else if (!strcmp(option,TDS_STR_TEXTSZ)) {
				if (atoi(value)) 
					config->text_size = atoi(value);
			} else if (!strcmp(option,TDS_STR_CHARSET)) {
				if (config->char_set) free(config->char_set);
				config->char_set = strdup(value);
			} else if (!strcmp(option,TDS_STR_CLCHARSET)) {
				if (config->client_charset) free(config->client_charset);
				config->client_charset = strdup(value);
			} else if (!strcmp(option,TDS_STR_LANGUAGE)) {
				if (config->language) free(config->language);
				config->language = strdup(value);
			} else if (!strcmp(option,TDS_STR_APPENDMODE)) {
				g_append_mode = tds_config_boolean(value);
			}
		}

	}
	return found;
}

static void tds_read_interfaces(char *server, TDSCONFIGINFO *config)
{
char ip_addr[255], ip_port[255], tds_ver[255];

	/* read $SYBASE/interfaces */
	/* This needs to be cleaned up */
	get_server_info(server, ip_addr, ip_port, tds_ver);
	if (strlen(ip_addr)) {
		if (config->ip_addr) free(config->ip_addr);
		config->ip_addr = (char *) malloc(strlen(ip_addr)+1);
		strcpy(config->ip_addr, ip_addr);
	}
	if (atoi(ip_port)) {
		config->port = atoi(ip_port);
	}
	if (strlen(tds_ver)) {
		tds_config_verstr(tds_ver, config);
		/* if it doesn't match a known version do nothing with it */
	}	
}
static void tds_config_login(TDSCONFIGINFO *config, TDSLOGIN *login)
{
	if (login->server_name && strlen(login->server_name)) {
		if (config->server_name) free(config->server_name);
		config->server_name = strdup(login->server_name);
	}	
	if (login->major_version || login->minor_version) {
		config->major_version = login->major_version;
		config->minor_version = login->minor_version;
	}
        if (login->language && strlen(login->language)) {
		if (config->language) free(config->language);
		config->language = strdup(login->language);
	}
        if (login->char_set && strlen(login->char_set)) {
		if (config->char_set) free(config->char_set);
		config->char_set = strdup(login->char_set);
	}
        if (login->host_name && strlen(login->host_name)) {
		if (config->host_name) free(config->host_name);
		config->host_name = strdup(login->host_name);
		/* DBSETLHOST and it's equivilants are commentary fields
		** they don't affect config->ip_addr (the server) but they show
		** up in an sp_who as the *clients* hostname.  (bsb, 11/10) 
		*/
		/* should work with IP (mlilback, 11/7/01) */
		/*
		if (config->ip_addr) free(config->ip_addr);
		config->ip_addr = calloc(sizeof(char),18);
		lookup_host(config->host_name, NULL, config->ip_addr, NULL);
		*/
	}
        if (login->app_name && strlen(login->app_name)) {
		if (config->app_name) free(config->app_name);
		config->app_name = strdup(login->app_name);
	}
        if (login->user_name && strlen(login->user_name)) {
		if (config->user_name) free(config->user_name);
		config->user_name = strdup(login->user_name);
	}
        if (login->password && strlen(login->password)) {
		if (config->password) free(config->password);
		config->password = strdup(login->password);
	}
        if (login->library && strlen(login->library)) {
		if (config->library) free(config->library);
		config->library = strdup(login->library);
	}
        if (login->encrypted) {
		config->encrypted = 1;
	}
        if (login->suppress_language) {
		config->suppress_language = 1;
	}
        if (login->bulk_copy) {
		config->bulk_copy = 1;
	}
        if (login->block_size) {
		config->block_size = login->block_size;
	}
        if (login->port) {
		config->port = login->port;
	}

}

static void tds_config_env_dsquery(TDSCONFIGINFO *config)
{
char *s;

        if ((s=getenv("DSQUERY"))) {
		if (s && strlen(s)) {
			if (config->server_name) free(config->server_name);
			config->server_name = strdup(s);
		}
	}
}
static void tds_config_env_tdsdump(TDSCONFIGINFO *config)
{
char *s;
char path[255];
pid_t pid=0;

        if ((s=getenv("TDSDUMP"))) {
                if (!strlen(s)) {
                        pid = getpid();
                        sprintf(path,"/tmp/freetds.log.%d",pid);
			if (config->dump_file) free(config->dump_file);
			config->dump_file = strdup(path);
                } else {
			if (config->dump_file) free(config->dump_file);
			config->dump_file = strdup(s);
                }
        }
}
static void tds_config_env_tdsport(TDSCONFIGINFO *config)
{
char *s;

	if ((s=getenv("TDSPORT"))) {
		config->port=atoi(s);
	}
	return;
}
static void tds_config_env_tdsver(TDSCONFIGINFO *config)
{
char *tdsver;

	if ((tdsver=getenv("TDSVER"))) {
		tds_config_verstr(tdsver, config);
	}
	return;
}
/* TDSHOST env var, pkleef@openlinksw.com 01/21/02 */
static void tds_config_env_tdshost(TDSCONFIGINFO *config)
{
char *tdshost;
char tmp[256];

	if (tdshost=getenv("TDSHOST")) {
		lookup_host (tdshost, NULL, tmp, NULL);
		if (config->ip_addr)
			free (config->ip_addr);
		config->ip_addr = strdup (tmp);
	}
	return;
}

static void tds_config_verstr(char *tdsver, TDSCONFIGINFO *config)
{
	if (!strcmp(tdsver,"42") || !strcmp(tdsver,"4.2")) {
		config->major_version=4;
		config->minor_version=2;
		return;
	} else if (!strcmp(tdsver,"46") || !strcmp(tdsver,"4.6")) {
		config->major_version=4;
		config->minor_version=6;
		return;
	} else if (!strcmp(tdsver,"50") || !strcmp(tdsver,"5.0")) {
		config->major_version=5;
		config->minor_version=0;
		return;
	} else if (!strcmp(tdsver,"70") || !strcmp(tdsver,"7.0")) {
		config->major_version=7;
		config->minor_version=0;
		return;
	} else if (!strcmp(tdsver,"80") || !strcmp(tdsver,"8.0")) {
		config->major_version=8;
		config->minor_version=0;
		return;
	}
}
int set_interfaces_file_loc(char *interf)
{
	if (strlen(interf)>MAXPATH) return 0;

	strcpy(interf_file,interf);
	return 1; /* SUCCEED */
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
   struct hostent   *host    = gethostbyname(servername);
   struct servent   *service = NULL;
   int               num     = 0;

/* froy@singleentry.com 12/21/2000 */
	if (host==NULL) {
		char addr [4];
		int a0, a1, a2, a3;
		sscanf (servername, "%d.%d.%d.%d", &a0, &a1, &a2, &a3);
		addr [0] = a0;
		addr [1] = a1;
		addr [2] = a2;
		addr [3] = a3;
		host    = gethostbyaddr (addr, 4, AF_INET);
	}
/* end froy */ 
   if (!host) {
      ip[0]   = '\0';
   } else {
      struct in_addr *ptr = (struct in_addr *) host->h_addr;
      strncpy(ip, inet_ntoa(*ptr), 17);
   }
   if (portname) {
   	 service = getservbyname(portname, "tcp");
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
char  line[255];
char  tmp_ip[sizeof(line)];
char  tmp_port[sizeof(line)];
char  tmp_ver[sizeof(line)];
FILE *in;
char *field;
int   found=0;

	ip_addr[0]  = '\0';
	ip_port[0]  = '\0';
	line[0]     = '\0';
	tmp_ip[0]   = '\0';
	tmp_port[0] = '\0';
	tmp_ver[0]  = '\0';

	tdsdump_log(TDS_DBG_INFO1, "%L Searching interfaces file %s/%s\n",dir,file);
	pathname = (char *) malloc(strlen(dir) + strlen(file) + 10);
   
	/*
	* create the full pathname to the interface file
	*/
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
		free(pathname);
		return;
	}
	tdsdump_log(TDS_DBG_INFO1, "%L Interfaces file opened\n");

	while (fgets(line,sizeof(line)-1,in)) {
		if (line[0]=='#') continue; /* comment */

		if (!isspace(line[0])) {
			field = strtok(line,"\n\t ");
			if (!strcmp(field,host)) {
				found=1;
				tdsdump_log(TDS_DBG_INFO1, "%L Found matching entry.\n");
			}
			else found=0;
		} else if (found && isspace(line[0])) {
			field = strtok(line,"\n\t ");
			if (field!=NULL && !strcmp(field,"query")) {
				field = strtok(NULL,"\n\t "); /* tcp or tli */
				if (!strcmp(field,"tli")) {
					tdsdump_log(TDS_DBG_INFO1, "%L TLI service.\n");
					field = strtok(NULL,"\n\t "); /* tcp */
					field = strtok(NULL,"\n\t "); /* device */
					field = strtok(NULL,"\n\t "); /* host/port */
					if (strlen(field)>=18) {
						sprintf(tmp_port,"%d", hex2num(&field[6])*256 + 
							hex2num(&field[8]));
						sprintf(tmp_ip,"%d.%d.%d.%d", hex2num(&field[10]),
							hex2num(&field[12]), hex2num(&field[14]),
							hex2num(&field[16]));
					}
				} else {
					field = strtok(NULL,"\n\t "); /* ether */
					strcpy(tmp_ver,field);
					field = strtok(NULL,"\n\t "); /* host */
					strcpy(tmp_ip,field);
					tdsdump_log(TDS_DBG_INFO1, "%L host field %s\n",tmp_ip);
					field = strtok(NULL,"\n\t "); /* port */
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
   tdsdump_log(TDS_DBG_INFO1, "%L Resolved IP %s\n",ip_addr);
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

	if (!server || strlen(server) == 0) {
		server = getenv("DSQUERY");
		if(!server || strlen(server) == 0) {
			server = "SYBASE";
		}
	}

	/*
	* Look for the server in the interf_file iff interf_file has been set.
	*/
	if (ip_addr[0]=='\0' && interf_file[0]!='\0') {
		search_interface_file("", interf_file, server, ip_addr, 
			ip_port, tds_ver);
	}

	/*
	* if we haven't found the server yet then look for a $HOME/.interfaces file
	*/
	if (ip_addr[0]=='\0') {
		char  *home = getenv("HOME");
		if (home!=NULL && home[0]!='\0') {
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
			search_interface_file(sybase, "interfaces", server, ip_addr, 
				ip_port, tds_ver);
		} else {
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
		}

		/*
		* lookup the host and service
		*/
		lookup_host(server, tmp_port, ip_addr, ip_port);

	}

	return ip_addr[0]!='\0' && ip_port[0]!='\0';
} /* get_server_info()  */


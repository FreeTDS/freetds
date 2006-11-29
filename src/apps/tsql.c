/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005 Brian Bruns
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
#endif /* HAVE_CONFIG_H */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* HAVE_READLINE */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

/* HP-UX require some constants defined by limits.h */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif /* HAVE_LIMITS_H */

#if defined(__hpux__) && !defined(_POSIX_PATH_MAX)
#define _POSIX_PATH_MAX 255
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif /* HAVE_LOCALE_H */

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif /* HAVE_LANGINFO_H */

#include "tds.h"
#include "tdsconvert.h"
#include "replacements.h"

TDS_RCSID(var, "$Id: tsql.c,v 1.97 2006-11-29 20:47:17 jklowden Exp $");

enum
{
	OPT_VERSION =  0x01,
	OPT_TIMER =    0x02,
	OPT_NOFOOTER = 0x04,
	OPT_NOHEADER = 0x08,
	OPT_QUIET =    0x10
};

static int istty = 0;
static int global_opt_flags = 0;
#define QUIET (global_opt_flags & OPT_QUIET)

static int do_query(TDSSOCKET * tds, char *buf, int opt_flags);
static void tsql_print_usage(const char *progname);
static int get_opt_flags(char *s, int *opt_flags);
static void populate_login(TDSLOGIN * login, int argc, char **argv);
static int tsql_handle_message(const TDSCONTEXT * context, TDSSOCKET * tds, TDSMESSAGE * msg);
static void slurp_input_file(char *fname, char **mybuf, int *bufsz, int *line);

static char *
tsql_readline(char *prompt)
{
	size_t sz, pos;
	char *line, *p;

#ifdef HAVE_READLINE
	if (istty)
		return readline(prompt);
#endif

	sz = 1024;
	pos = 0;
	line = (char*) malloc(sz);
	if (!line)
		return NULL;

	if (prompt && prompt[0])
		printf("%s", prompt);
	for (;;) {
		/* read a piece */
		if (fgets(line + pos, sz - pos, stdin) == NULL) {
			if (pos)
				return line;
			break;
		}

		/* got end-of-line ? */
		p = strchr(line + pos, '\n');
		if (p) {
			*p = 0;
			return line;
		}

		/* allocate space if needed */
		pos += strlen(line + pos);
		if (pos + 1024 >= sz) {
			sz += 1024;
			p = (char*) realloc(line, sz);
			if (!p)
				break;
			line = p;
		}
	}
	free(line);
	return NULL;
}

static void
tsql_add_history(const char *s)
{
#ifdef HAVE_READLINE
	if (istty)
		add_history(s);
#endif
}

static int
do_query(TDSSOCKET * tds, char *buf, int opt_flags)
{
	int rows = 0;
	int rc, i;
	TDSCOLUMN *col;
	int ctype;
	CONV_RESULT dres;
	unsigned char *src;
	TDS_INT srclen;
	TDS_INT resulttype;
	struct timeval start, stop;
	int print_rows = 1;
	char message[128];

	rc = tds_submit_query(tds, buf);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "tds_submit_query() failed\n");
		return 1;
	}

	while ((rc = tds_process_tokens(tds, &resulttype, NULL, TDS_TOKEN_RESULTS)) == TDS_SUCCEED) {
		if (opt_flags & OPT_TIMER) {
			gettimeofday(&start, NULL);
			print_rows = 0;
		}
		switch (resulttype) {
		case TDS_ROWFMT_RESULT:
			if ((!(opt_flags & OPT_NOHEADER)) && tds->current_results) {
				for (i = 0; i < tds->current_results->num_cols; i++) {
					fprintf(stdout, "%s\t", tds->current_results->columns[i]->column_name);
				}
				fprintf(stdout, "\n");
			}
			break;
		case TDS_COMPUTE_RESULT:
		case TDS_ROW_RESULT:
			rows = 0;
			while ((rc = tds_process_tokens(tds, &resulttype, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE|TDS_RETURN_ROW|TDS_RETURN_COMPUTE)) == TDS_SUCCEED) {
				if (resulttype != TDS_ROW_RESULT && resulttype != TDS_COMPUTE_RESULT)
					break;

				rows++;

				if (!tds->current_results)
					continue;

				for (i = 0; i < tds->current_results->num_cols; i++) {
					col = tds->current_results->columns[i];
					if (col->column_cur_size < 0) {
						if (print_rows)
							fprintf(stdout, "NULL\t");
						continue;
					}
					ctype = tds_get_conversion_type(col->column_type, col->column_size);

					src = col->column_data;
					if (is_blob_type(col->column_type))
						src = (unsigned char *) ((TDSBLOB *) src)->textvalue;
					srclen = col->column_cur_size;


					if (tds_convert(tds->tds_ctx, ctype, (TDS_CHAR *) src, srclen, SYBVARCHAR, &dres) < 0)
						continue;
					if (print_rows)
						fprintf(stdout, "%s\t", dres.c);
					free(dres.c);
				}
				if (print_rows)
					fprintf(stdout, "\n");

			}
			break;
		case TDS_STATUS_RESULT:
			if (!QUIET)
				printf("(return status = %d)\n", tds->ret_status);
			break;
		default:
			break;
		}

		if (opt_flags & OPT_VERSION) {
			char version[64];
			int line = 0;

			line = tds_version(tds, version);
			if (line) {
				sprintf(message, "using TDS version %s", version);
				tds_client_msg(tds->tds_ctx, tds, line, line, line, line, message);
			}
		}
		if (opt_flags & OPT_TIMER) {
			gettimeofday(&stop, NULL);
			sprintf(message, "Total time for processing %d rows: %ld msecs\n",
				rows, (long) ((stop.tv_sec - start.tv_sec) * 1000) + ((stop.tv_usec - start.tv_usec) / 1000));
			tds_client_msg(tds->tds_ctx, tds, 1, 1, 1, 1, message);
		}
	}
	return 0;
}

static void
tsql_print_usage(const char *progname)
{
	fprintf(stderr,
		"Usage:\t%s [-S <server> | -H <hostname> -p <port>] -U <username> [-P <password>] [-I <config file>] [-o <options>]\n"
		"\t%s -C\n"
		"Options:\n"
		"\tf\tDo not print footer\n"
		"\th\tDo not print header\n"
		"\tt\tPrint time informations\n"
		"\tv\tPrint TDS version\n"
		"\tq\tQuiet\n",
		progname, progname);
}

/*
 * The 'GO' command may be followed by options that apply to the batch.
 * If they don't appear to be right, assume the letters "go" are part of the
 * SQL, not a batch separator.  
 */
static int
get_opt_flags(char *s, int *opt_flags)
{
	char **argv, **first_arg;
	int argc;
	int opt;

	/* make sure we have enough elements */
	assert(s && opt_flags);
	argv = (char **) calloc(strlen(s) + 2, sizeof(char*));
	if (!argv)
		return 0;

	/* parse the command line and assign to argv */
	for (argc=0; (argv[argc] = strtok(s, " ")) != NULL; argc++)
		s = NULL;

	first_arg = argv;
	if (argc > 0 && strcasecmp(first_arg[0], "go") == 0) {
		argc--;
		first_arg++;
	}
		
	*opt_flags = 0;
	optind = 0;		/* reset getopt */
	opterr = 0;		/* suppress error messages */
	while ((opt = getopt(argc, first_arg, "fhqtv")) != -1) {
		switch (opt) {
		case 'f':
			*opt_flags |= OPT_NOFOOTER;
			break;
		case 'h':
			*opt_flags |= OPT_NOHEADER;
			break;
		case 't':
			*opt_flags |= OPT_TIMER;
			break;
		case 'v':
			*opt_flags |= OPT_VERSION;
			break;
		case 'q':
			*opt_flags |= OPT_QUIET;
			break;
		default:
			fprintf(stderr, "Warning: invalid option '%s' found: \"go\" treated as simple SQL\n", argv[optind-1]);
			free(argv);
			return 0;
		}
	}
	
	free(argv);
	return 1;
}

static void
populate_login(TDSLOGIN * login, int argc, char **argv)
{
	const TDS_COMPILETIME_SETTINGS *settings;
	char *hostname = NULL;
	char *servername = NULL;
	char *username = NULL;
	char *password = NULL;
	char *confile = NULL;
	int port = 0;
	int opt;
	const char *locale = NULL;
	char *charset = NULL;
	char *opt_flags_str = NULL;

	setlocale(LC_ALL, "");
	locale = setlocale(LC_ALL, NULL);

#if HAVE_LOCALE_CHARSET
	charset = locale_charset();
#endif
#if HAVE_NL_LANGINFO && defined(CODESET)
	if (!charset)
		charset = nl_langinfo(CODESET);
#endif


	while ((opt = getopt(argc, argv, "H:S:I:P:U:p:Co:")) != -1) {
		switch (opt) {
		case 'o':
			opt_flags_str = optarg;
			break;
		case 'H':
			hostname = (char *) malloc(strlen(optarg) + 1);
			strcpy(hostname, optarg);
			break;
		case 'S':
			servername = (char *) malloc(strlen(optarg) + 1);
			strcpy(servername, optarg);
			break;
		case 'U':
			username = (char *) malloc(strlen(optarg) + 1);
			strcpy(username, optarg);
			break;
		case 'P':
			password = (char *) malloc(strlen(optarg) + 1);
			strcpy(password, optarg);
			break;
		case 'I':
			confile = (char *) malloc(strlen(optarg) + 1);
			strcpy(confile, optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'C':
			settings = tds_get_compiletime_settings();
			printf("%s\n%35s: %s\n%35s: %s\n%35s: %s\n%35s: %s\n%35s: %s\n%35s: %s\n%35s: %s\n%35s: %s\n%35s: %s\n",
			       "Compile-time settings (established with the \"configure\" script)",
			       "Version", settings->freetds_version,
			       "freetds.conf directory", settings->sysconfdir, 
			       /* settings->last_update */
			       "MS db-lib source compatibility", settings->msdblib ? "yes" : "no",
			       "Sybase binary compatibility",
			       (settings->sybase_compat == -1 ? "unknown" : (settings->sybase_compat ? "yes" : "no")),
			       "Thread safety", settings->threadsafe ? "yes" : "no",
			       "iconv library", settings->libiconv ? "yes" : "no",
			       "TDS version", settings->tdsver,
			       "iODBC", settings->iodbc ? "yes" : "no", 
			       "unixodbc", settings->unixodbc ? "yes" : "no");
			exit(0);
			break;
		default:
			tsql_print_usage(argv[0]);
			exit(1);
			break;
		}
	}

	if (opt_flags_str != NULL) {
		char *minus_flags = malloc(strlen(opt_flags_str) + 5);
		if (minus_flags != NULL) {
			strcpy(minus_flags, "go -");
			strcat(minus_flags, opt_flags_str);
			get_opt_flags(minus_flags, &global_opt_flags);
			free(minus_flags);
		}
	}


	if (locale)
		if (!QUIET) printf("locale is \"%s\"\n", locale);
	if (charset) {
		if (!QUIET) printf("locale charset is \"%s\"\n", charset);
	} else {
		charset = "ISO-8859-1";
		if (!QUIET) printf("using default charset \"%s\"\n", charset);
	}

	/* validate parameters */
	if (!servername && !hostname) {
		fprintf(stderr, "Missing argument -S or -H\n");
		tsql_print_usage(argv[0]);
		exit(1);
	}
	if (hostname && !port) {
		fprintf(stderr, "Missing argument -p \n");
		tsql_print_usage(argv[0]);
		exit(1);
	}
	if (!username) {
		fprintf(stderr, "Missing argument -U \n");
		tsql_print_usage(argv[0]);
		exit(1);
	}
	if (!servername && !hostname) {
		tsql_print_usage(argv[0]);
		exit(1);
	}
	if (!password) {
		password = (char*) malloc(128);
		readpassphrase("Password: ", password, 128, RPP_ECHO_OFF);
	}

	/* all validated, let's do it */

	/* if it's a servername */

	if (servername) {
		tds_set_user(login, username);
		tds_set_app(login, "TSQL");
		tds_set_library(login, "TDS-Library");
		tds_set_server(login, servername);
		tds_set_client_charset(login, charset);
		tds_set_language(login, "us_english");
		tds_set_passwd(login, password);
		if (confile) {
			tds_set_interfaces_file_loc(confile);
		}
		/* else we specified hostname/port */
	} else {
		tds_set_user(login, username);
		tds_set_app(login, "TSQL");
		tds_set_library(login, "TDS-Library");
		tds_set_server(login, hostname);
		tds_set_port(login, port);
		tds_set_client_charset(login, charset);
		tds_set_language(login, "us_english");
		tds_set_passwd(login, password);
	}

	/* free up all the memory */
	if (hostname)
		free(hostname);
	if (username)
		free(username);
	if (password)
		free(password);
	if (servername)
		free(servername);
}

static int
tsql_handle_message(const TDSCONTEXT * context, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	if (msg->msgno == 0) {
		fprintf(stderr, "%s\n", msg->message);
		return 0;
	}

	if (msg->msgno != 5701 && msg->msgno != 5703
	    && msg->msgno != 20018) {
		fprintf(stderr, "Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
			msg->msgno, msg->severity, msg->state, msg->server, msg->line_number, msg->message);
	}

	return 0;
}

static void
slurp_input_file(char *fname, char **mybuf, int *bufsz, int *line)
{
	FILE *fp = NULL;
	register char *n;
	char linebuf[1024];
	char *s = NULL;

	if ((fp = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "Unable to open input file '%s': %s\n", fname, strerror(errno));
		return;
	}
	while ((s = fgets(linebuf, sizeof(linebuf), fp)) != NULL) {
		while (strlen(*mybuf) + strlen(s) + 2 > *bufsz) {
			*bufsz *= 2;
			*mybuf = (char *) realloc(*mybuf, *bufsz);
		}
		strcat(*mybuf, s);
		n = strrchr(s, '\n');
		if (n != NULL)
			*n = '\0';
		tsql_add_history(s);
		(*line)++;
	}
}


int
main(int argc, char **argv)
{
	char *s = NULL, *s2 = NULL, *cmd = NULL;
	char prompt[20];
	int line = 0;
	char *mybuf;
	int bufsz = 4096;
	TDSSOCKET *tds;
	TDSLOGIN *login;
	TDSCONTEXT *context;
	TDSCONNECTION *connection;
	int opt_flags = 0;
	pid_t timer_pid = 0;

	istty = isatty(0);

	if (INITSOCKET()) {
		fprintf(stderr, "Unable to initialize sockets\n");
		return 1;
	}

	/* grab a login structure */
	login = tds_alloc_login();

	context = tds_alloc_context(NULL);
	if (context->locale && !context->locale->date_fmt) {
		/* set default in case there's no locale file */
		context->locale->date_fmt = strdup("%b %e %Y %I:%M%p");
	}

	context->msg_handler = tsql_handle_message;
	context->err_handler = tsql_handle_message;

	/* process all the command line args into the login structure */
	populate_login(login, argc, argv);

	/* Try to open a connection */
	tds = tds_alloc_socket(context, 512);
	tds_set_parent(tds, NULL);
	connection = tds_read_config_info(NULL, login, context->locale);
	
	/* 
	 * If we're able to establish an ip address for the server, we'll try to connect to it. 
	 * If that machine is currently unreachable
	 * show a timer connecting to the server 
	 */
	if (connection && !QUIET) {
		timer_pid = fork();
		if (-1 == timer_pid) {
			perror("tsql: warning"); /* make a note */
		}
		if (0 == timer_pid) {
			int i=0;
			while(1) {
				if (sleep(1) == 0) {
					fprintf(stderr, "\r%2d", ++i);
					continue;
				}
				printf("sleep was interrupted\n");
			}
		}
	}
	if (!connection || tds_connect(tds, connection) == TDS_FAIL) {
		tds_free_socket(tds);
		tds_free_connection(connection);
		fprintf(stderr, "There was a problem connecting to the server\n");
		if (timer_pid > 0 ) {
			kill(timer_pid, SIGTERM);
		}
		exit(1);
	}
	tds_free_connection(connection);
	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) malloc(bufsz);
	mybuf[0] = '\0';

#ifdef HAVE_READLINE
	rl_inhibit_completion = 1;
#endif
	if (timer_pid > 0 ) {
		if (kill(timer_pid, SIGTERM) == -1 ) {
			perror("tsql: warning");
		} else {
			if (wait(0) == -1 ) {
				perror("tsql: warning");
			} else {
				fprintf(stderr, "\r");
			}
		}
	}

	for (;;) {
		sprintf(prompt, "%d> ", ++line);
		if (s)
			free(s);
		s = tsql_readline(QUIET ? NULL : prompt);
		if (s == NULL) 
			break;

		/* 
		 * 'GO' is special only at the start of a line
		 *  The rest of the line may include options that apply to the batch, 
		 *  and perhaps whitespace.  
		 */
		if (0 == strncasecmp(s, "go", 2) && (strlen(s) == 2 || isspace(s[2]))) {
			char *go_line = strdup(s);
			assert(go_line);
			line = 0;
			if (get_opt_flags(go_line + 2, &opt_flags)) {
				free(go_line);
				opt_flags ^= global_opt_flags;
				do_query(tds, mybuf, opt_flags);
				mybuf[0] = '\0';
				continue;
			}
			free(go_line);
		}
		
		/* skip leading whitespace */
		if (s2)
			free(s2);
		s2 = strdup(s);	/* copy to mangle with strtok() */
		cmd = strtok(s2, " \t");
		
		if (!cmd)
			continue;

		if (!strcasecmp(cmd, "exit") || !strcasecmp(cmd, "quit") || !strcasecmp(cmd, "bye"))
			break;
		if (!strcasecmp(cmd, "version")) {
			tds_version(tds, mybuf);
			printf("using TDS version %s\n", mybuf);
			line = 0;
			mybuf[0] = '\0';
			continue;
		}
		if (!strcasecmp(cmd, "reset")) {
			line = 0;
			mybuf[0] = '\0';
		} else if (!strcasecmp(cmd, ":r")) {
			slurp_input_file(strtok(NULL, " \t"), &mybuf, &bufsz, &line);
		} else {
			while (strlen(mybuf) + strlen(s) + 2 > bufsz) {
				bufsz *= 2;
				mybuf = (char *) realloc(mybuf, bufsz);
			}
			tsql_add_history(s);
			strcat(mybuf, s);
			/* preserve line numbering for the parser */
			strcat(mybuf, "\n");
		}
	}

	/* close up shop */
	tds_free_socket(tds);
	tds_free_login(login);
	tds_free_context(context);
	DONESOCKET();

	return 0;
}

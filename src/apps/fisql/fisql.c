/* Free ISQL - An isql for DB-Library (C) 2007 Nicholas S. Castellano
 *
 * This program  is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#include <freetds/bool.h>
#include <freetds/replacements.h>
#include <freetds/macros.h>

#include <sybfront.h>
#include <sybdb.h>
#include "terminal.h"
#include "edit.h"
#include "handlers.h"
#include "interrupt.h"

#define READPASSPHRASE_MAXLEN 128

#ifndef HAVE_READLINE
static FILE *rl_outstream = NULL;
static FILE *rl_instream = NULL;
static const char *rl_readline_name = NULL;

static char *
fisql_readline(char *prompt)
{
	size_t sz, pos;
	char *line, *p;

	sz = 1024;
	pos = 0;
	line = (char*) malloc(sz);
	if (!line)
		return NULL;

	if (prompt && prompt[0])
		fprintf(rl_outstream ? rl_outstream : stdout, "%s", prompt);
	for (;;) {
		/* read a piece */
		if (fgets(line + pos, sz - pos, rl_instream ? rl_instream : stdin) == NULL) {
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
fisql_add_history(const char *s TDS_UNUSED)
{
}

#define readline    fisql_readline
#define add_history fisql_add_history

#define rl_bind_key(c,f)      do {} while(0)
#define rl_reset_line_state() do {} while(0)
#define rl_on_new_line()      do {} while(0)

#endif

#if !HAVE_RL_ON_NEW_LINE && !defined(rl_on_new_line)
#define rl_on_new_line()      do {} while(0)
#endif

#if !HAVE_RL_RESET_LINE_STATE && !defined(rl_reset_line_state)
#define rl_reset_line_state() do {} while(0)
#endif

static void *xmalloc(size_t s);
static void *xrealloc(void *p, size_t s);
static int get_printable_size(int type, int size);
static int get_printable_column_size(DBPROCESS * dbproc, int col);

static void *
xmalloc(size_t s)
{
	void *p = malloc(s);

	if (!p) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	return p;
}

static void *
xrealloc(void *p, size_t s)
{
	p = realloc(p, s);
	if (!p) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	return p;
}

/* adapted from src/dblib/dblib.c (via src/apps/bsqldb.c) */
static int
get_printable_size(int type, int size)
{
	switch (type) {
	case SYBINTN:
		switch (size) {
		case 1:
			return 3;
		case 2:
			return 6;
		case 4:
			return 11;
		case 8:
			return 21;
		}
	case SYBINT1:
		return 3;
	case SYBINT2:
		return 6;
	case SYBINT4:
		return 11;
	case SYBINT8:
		return 21;
	case SYBVARCHAR:
	case SYBCHAR:
		return size;
	case SYBFLT8:
		return 11;	/* FIX ME -- we do not track precision */
	case SYBREAL:
		return 11;	/* FIX ME -- we do not track precision */
	case SYBMONEY:
		return 12;	/* FIX ME */
	case SYBMONEY4:
		return 12;	/* FIX ME */
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
		return 26;	/* FIX ME */
#if 0				/* seems not to be exported to sybdb.h */
	case SYBBITN:
#endif
	case SYBBIT:
		return 1;
		/* FIX ME -- not all types present */
	default:
		return 0;
	}
}

static int
get_printable_column_size(DBPROCESS * dbproc, int col)
{
	int collen;

	collen = get_printable_size(dbcoltype(dbproc, col), dbcollen(dbproc, col));
	if (strlen(dbcolname(dbproc, col)) > collen) {
		collen = strlen(dbcolname(dbproc, col));
	}
	return collen;
}

static bool
update_comment(bool in_comment, const char *line)
{
	const char *p = line;
	while (*p) {
		if (in_comment) {
			p = strstr(p, "*/");
			if (!p)
				break;
			p += 2;
			in_comment = false;
		}
		/* not comment */
		if (p[0] == '/' && p[1] == '*') {
			in_comment = true;
			p += 2;
		} else if (p[0] == '-' && p[1] == '-') {
			/* comment till the end of line then no comment
			 * on next line */
			break;
		} else if (p[0]) {
			++p;
		}
	}
	return in_comment;
}

static DBPROCESS *dbproc;
static const char *editor;
static char **ibuf = NULL;
static int ibuflines = 0;
static int no_prompt = 0;
static int default_exit = EXIT_SUCCESS;
static const char *cmdend = "go";

static void
reset_ibuf(void)
{
	int i;
	for (i = 0; ibuf && i < ibuflines; i++) {
		free(ibuf[i]);
	}
	ibuflines = 0;
	free(ibuf);
	ibuf = NULL;
}

static void
append_line(char *line)
{
	ibuf = (char **) xrealloc(ibuf, (ibuflines + 1) * sizeof(char *));
	ibuf[ibuflines++] = line;
}

static void
vi_cmd(const char *command)
{
	char tmpfn[256];
	FILE *fp;
	int i;
	char *sqlch;
	FILE *tmpfp;
	char *line;
	char foobuf[40];
	int tmpfd;
	mode_t old_mask;

	strcpy(tmpfn, "/tmp/fisqlXXXXXX");
	old_mask = umask(077);
	tmpfd = mkstemp(tmpfn);
	umask(old_mask);
	if (tmpfd < 0 || (fp = fdopen(tmpfd, "w")) == NULL) {
		perror("fisql");
		reset_term();
		dbexit();
		exit(2);
	}
	if (ibuflines) {
		for (i = 0; i < ibuflines; i++) {
			fputs(ibuf[i], fp);
			fputc('\n', fp);
			free(ibuf[i]);
		}
	} else {
		for (i = 0; ((sqlch = dbgetchar(dbproc, i)) != NULL); i++) {
			fputc(*sqlch, fp);
		}
	}
	fclose(fp);

	edit(command, tmpfn);

	ibuflines = 0;
	fp = fopen(tmpfn, "r");
	if (!fp) {
		perror("fisql");
		reset_term();
		dbexit();
		exit(2);
	}
	tmpfp = rl_instream;
	rl_instream = fp;
	strcpy(foobuf, "1>> ");
	while ((line = readline(foobuf)) != NULL) {
		append_line(line);
		sprintf(foobuf, "%d>> ", ibuflines + 1);
	}
	rl_instream = tmpfp;
	fclose(fp);
	fputc('\r', stdout);
	fflush(stdout);
	unlink(tmpfn);
}

static void
system_cmd(const char *cmd)
{
#if HAVE_SYSTEM
	int rv;
	switch (rv = system(cmd)) {
	case 0:
		break;
	case -1:
		fprintf(stderr,
		    "Failed to execute `%s'\n", cmd);
		break;
	default:
		fprintf(stderr, "Command `%s' exited "
		    "with code %d\n", cmd, rv);
		break;
	}
#else
	fprintf(stderr, "Your system cannot execute commands. Tried to execute `%s'\n", cmd);
#endif
}

static void
readfile_cmd(char *line)
{
	FILE *fp;
	FILE *tmpfp;
	FILE *tmpfp2;
	char *tfn;
	char *cp;

	for (cp = line + 2; *cp && (isspace((unsigned char) *cp)); cp++)
		continue;
	tfn = cp;
	for (; *cp && !(isspace((unsigned char) *cp)); cp++)
		continue;
	*cp = '\0';
	if ((fp = fopen(tfn, "r")) == NULL) {
		fprintf(stderr, "Operating system error: Failed to open %s.\n", tfn);
		return;
	}
	tmpfp = rl_instream;
	tmpfp2 = rl_outstream;
	rl_instream = fp;
	rl_outstream = fopen("/dev/null", "w");
	while ((line = readline("")) != NULL) {
		append_line(line);
	}
	rl_instream = tmpfp;
	fclose(rl_outstream);
	rl_outstream = tmpfp2;
	fclose(fp);
	fputc('\r', stdout);
	fflush(stdout);
}

static void
read_sql_lines(void)
{
	char *cp;
	char *firstword;
	char firstword_separator;
	char *line = NULL;
	char foobuf[40];
	bool in_comment = false;

	reset_ibuf();
	while (1) {
		if (no_prompt) {
			foobuf[0] = '\0';
		} else {
			sprintf(foobuf, "%d>> ", ibuflines + 1);
		}
		free(line);
		line = readline(foobuf);
		if (line == NULL) {
			reset_term();
			dbexit();
			exit(default_exit);
		}
		for (cp = line; *cp && isspace((unsigned char) *cp); cp++)
			continue;
		if (*cp) {
			add_history(line);
		}
		if (in_comment) {
			goto append_line;
		}
		if (!(strncasecmp(line, "!!", 2))) {
			system_cmd(line + 2);
			continue;
		}
		/* XXX: isql off-by-one line count error for :r not duplicated */
		if (!(strncasecmp(line, ":r", 2))) {
			readfile_cmd(line);
			continue;
		}
		firstword = line;
		firstword_separator = '\0';
		for (cp = firstword; *cp; cp++) {
			if (isspace((unsigned char) *cp)) {
				firstword_separator = *cp;
				*cp = '\0';
				break;
			}
		}
		if ((!(strcasecmp(firstword, "exit")))
		    || (!(strcasecmp(firstword, "quit")))) {
			reset_term();
			dbexit();
			exit(default_exit);
		}
		if (!(strcasecmp(firstword, "reset"))) {
			reset_ibuf();
			continue;
		}
		if (!(strcasecmp(firstword, cmdend))) {
			if (ibuflines == 0) {
				continue;
			}
			break;
		}
		if ((!(strcasecmp(firstword, "vi")))
		    || (!(strcasecmp(firstword, editor)))) {
			vi_cmd(firstword);
			continue;
		}
		firstword[strlen(firstword)] = firstword_separator;

append_line:
		in_comment = update_comment(in_comment, line);
		append_line(line);
		line = NULL;
	}
}

int
main(int argc, char *argv[])
{
	int echo = 0;

#ifdef notyet
	int print_statistics = 0;
#endif
	int fipsflagger = 0;
	int perfstats = 0;
	int use_encryption = 0;
	int chained_transactions = 0;
	int headers = 0;
	char *columnwidth = NULL;
	const char *colseparator = " ";
	const char *lineseparator = "\n";
	int timeout = 0;
	char *username = NULL;
	char *password = NULL;
	char *server = NULL;
	DBCHAR *char_set = NULL;
	char *hostname = NULL;
	char *interfaces_filename = NULL;
	char *input_filename = NULL;
	char *output_filename = NULL;
	int logintime = -1;
	char *language = NULL;
	int size = 0;
	char *sybenv;
	LOGINREC *login;
	int printedlines;
	int i;
	int dbrc;
	int c;
	int errflg = 0;
	char *prbuf;
	int prbuflen;
	int num_cols;
	int selcol;
	int col;
	int collen;
	DBINT colid;
	const char *opname;
	char adbuf[512];
	DBINT convlen;
	int printedcompute = 0;
	char adash;
	const char *database_name = NULL;

	setlocale(LC_ALL, "");

#ifdef __VMS
	/* Convert VMS-style arguments to Unix-style */
	parse_vms_args(&argc, &argv);
#endif

	editor = getenv("EDITOR");
	if (!editor) {
		editor = getenv("VISUAL");
	}
	if (!editor) {
		editor = "vi";
	}

	opterr = 0;
	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, "eFgpnvXYa:c:D:E:h:H:i:I:J:l:m:o:P:s:S:t:U:w:y:z:A:"))
	       != -1) {
		switch (c) {
		case 'e':
			echo = 1;
			break;
		case 'F':
			fipsflagger = 1;
			break;
		case 'g':
			errflg++;
			break;
		case 'p':
			errflg++;
			perfstats = 1;
			break;
		case 'n':
			no_prompt = 1;
			break;
		case 'v':
			puts("fisql, a free isql replacement by Nicholas S. Castellano");
			return EXIT_SUCCESS;
			break;
		case 'X':
			/* XXX: We get a different error message than isql gives; neither seems
			 * to work
			 */
			use_encryption = 1;
			break;
		case 'Y':
			chained_transactions = 1;
			break;
		case 'c':
			cmdend = optarg;
			break;
		case 'E':
			editor = optarg;
			break;
		case 'h':
			headers = atoi(optarg);
			break;
		case 'H':
			hostname = optarg;
			break;
		case 'i':
			input_filename = optarg;
			break;
		case 'I':
			interfaces_filename = optarg;
			break;
		case 'J':
			errflg++;
			break;
		case 'l':
			logintime = atoi(optarg);
			break;
		case 'm':
			global_errorlevel = atoi(optarg);
			break;
		case 'o':
			output_filename = optarg;
			break;
		case 'P':
			password = optarg;
			break;
		case 's':
			colseparator = optarg;
			break;
		case 'S':
			server = optarg;
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		case 'U':
			username = optarg;
			break;
		case 'w':
			columnwidth = optarg;
			break;
		case 'y':
			/* XXX: this doesn't seem to be what isql does with -y...it doesn't
			 * seem to do anything actually
			 */
			sybenv = (char *) xmalloc((strlen(optarg) + 8) * sizeof(char));
			strcpy(sybenv, "SYBASE=");
			strcat(sybenv, optarg);
			putenv(sybenv);
			break;
		case 'z':
			language = optarg;
			break;
		case 'A':
			size = atoi(optarg);
			break;
		case 'D':
			database_name = optarg;
			break;
		default:
			errflg++;
			break;
		}
	}

	if (errflg) {
		fprintf(stderr, "usage: fisql [-e] [-F] [-g] [-p] [-n] [-v] [-X] [-Y]\n");
		fprintf(stderr, "\t[-c cmdend] [-D database_name] [-E editor]\n");
		fprintf(stderr, "\t[-h headers] [-H hostname] [-i inputfile]\n");
		fprintf(stderr, "\t[-I interfaces_file] [-J client character set]\n");
		fprintf(stderr, "\t[-l login_timeout] [-m errorlevel]\n");
		fprintf(stderr, "\t[-o outputfile]\n");
		fprintf(stderr, "\t[-P password] [-s colseparator] [-S server]\n");
		fprintf(stderr, "\t[-t timeout] [-U username] [-w columnwidth]\n");
		fprintf(stderr, "\t[-y sybase_dir] [-z language]\n");
		return EXIT_FAILURE;
	}
	if (!(isatty(fileno(stdin)))) {
		no_prompt = 1;
		rl_outstream = fopen("/dev/null", "rw");
	}
	rl_readline_name = "fisql";
	rl_bind_key('\t', rl_insert);
	if (password == NULL) {
		password = (char *) xmalloc(READPASSPHRASE_MAXLEN);
		readpassphrase("Password: ", password, READPASSPHRASE_MAXLEN, RPP_ECHO_OFF);
	}
	if (input_filename) {
		if (freopen(input_filename, "r", stdin) == NULL) {
			/* XXX: sybase isql generates this error while parsing the options,
			 * but doesn't redirect the input until after the Password: prompt
			 */
			/* lack of newline for bug-compatibility with isql */
			fprintf(stderr, "Unable to open input file '%s'.", optarg);
			return EXIT_FAILURE;
		}
	}
	if (output_filename) {
		if (freopen(output_filename, "w", stdout) == NULL) {
			/* XXX: sybase isql generates this error while parsing the options,
			 * but doesn't redirect the output until after the Password: prompt
			 */
			/* lack of newline for bug-compatibility with isql */
			fprintf(stderr, "Unable to open output file '%s'.", output_filename);
			return EXIT_FAILURE;
		}
	}
	if (isatty(fileno(stdin))) {
		rl_outstream = stdout;
	}
	dbinit();
#if 0
#ifdef DBVERSION_100
	dbsetversion(DBVERSION_100);
#endif
#endif
	if ((login = dblogin()) == NULL) {
		reset_term();
		return EXIT_FAILURE;
	}
	dbmsghandle(msg_handler);
	dberrhandle(err_handler);
	DBSETLAPP(login, "fisql");
	if (username) {
		DBSETLUSER(login, username);
	}
	DBSETLPWD(login, password);
	memset(password, 0, strlen(password));

	if (char_set) {
		DBSETLCHARSET(login, char_set);
	}
	if (use_encryption) {
		DBSETLENCRYPT(login, TRUE);
	}
	if (hostname) {
		DBSETLHOST(login, hostname);
	}
	if (language) {
		DBSETLNATLANG(login, language);
	}
	if (size) {
		DBSETLPACKET(login, (short) size);
	}
	if (interfaces_filename) {
		dbsetifile(interfaces_filename);
	}
	dbsettime(timeout);
	if (logintime >= 0) {
		dbsetlogintime(logintime);
	}
	if (database_name) {
		DBSETLDBNAME(login, database_name);
	}
	if ((dbproc = dbopen(login, server)) == NULL) {
		fprintf(stderr, "fisql: dbopen() failed.\n");
		reset_term();
		return EXIT_FAILURE;
	}

	dbsetopt(dbproc, DBPRLINESEP, lineseparator, strlen(lineseparator));
	if (colseparator) {
		dbsetopt(dbproc, DBPRCOLSEP, colseparator, strlen(colseparator));
	}
	if (columnwidth) {
		dbsetopt(dbproc, DBPRLINELEN, columnwidth, 0);
	}
	if (chained_transactions) {
		dbsetopt(dbproc, DBCHAINXACTS, NULL, 0);
	}
	if (fipsflagger) {
		dbsetopt(dbproc, DBFIPSFLAG, NULL, 0);
	}
	if (perfstats) {
		dbsetopt(dbproc, DBSTAT, "time", 0);
	}

	while (1) {
		if (sigsetjmp(restart, 1)) {
			reset_ibuf();
			fputc('\n', stdout);
			rl_on_new_line();
			rl_reset_line_state();
		}
		dbcancel(dbproc);
		signal(SIGINT, inactive_interrupt_handler);

		read_sql_lines();

		dbfreebuf(dbproc);
		for (i = 0; i < ibuflines; i++) {
			if (echo) {
				puts(ibuf[i]);
			}
			dbcmd(dbproc, ibuf[i]);
			dbcmd(dbproc, "\n");
			free(ibuf[i]);
		}
		free(ibuf);
		ibuf = NULL;
		signal(SIGINT, active_interrupt_handler);
		dbsetinterrupt(dbproc, (void *) active_interrupt_pending, (void *) active_interrupt_servhandler);
		if (dbsqlexec(dbproc) == SUCCEED) {
			int status_printed = 0;

			maybe_handle_active_interrupt();
			while ((dbrc = dbresults(dbproc)) != NO_MORE_RESULTS) {
				printedlines = 0;
#define USE_DBPRROW 0
#if USE_DBPRROW
				dbprhead(dbproc);
				dbprrow(dbproc);
#else
				if ((dbrc == SUCCEED) && (DBROWS(dbproc) == SUCCEED)) {
					prbuflen = dbspr1rowlen(dbproc);
					prbuf = (char *) xmalloc(prbuflen * sizeof(char));
					dbsprhead(dbproc, prbuf, prbuflen);
					fputs(prbuf, stdout);
					fputc('\n', stdout);
					dbsprline(dbproc, prbuf, prbuflen, '-');
					fputs(prbuf, stdout);
					fputc('\n', stdout);
					maybe_handle_active_interrupt();
					while ((dbrc = dbnextrow(dbproc)) != NO_MORE_ROWS) {
						if (dbrc == FAIL) {
							break;
						}
						if (dbrc != REG_ROW) {
							num_cols = dbnumalts(dbproc, dbrc);
							for (selcol = col = 1; col <= num_cols; col++) {
								colid = dbaltcolid(dbproc, dbrc, col);
								while (selcol < colid) {
									collen = get_printable_column_size(dbproc, selcol);
									for (i = 0; i < collen; i++) {
										putchar(' ');
									}
									selcol++;
									printf("%s", colseparator);
								}
								opname = dbprtype(dbaltop(dbproc, dbrc, col));
								printf("%s", opname);
								collen = get_printable_column_size(dbproc, colid);
								collen -= strlen(opname);
								while (collen-- > 0) {
									putchar(' ');
								}
								selcol++;
								printf("%s", colseparator);
							}
							printf("%s", lineseparator);
							for (selcol = col = 1; col <= num_cols; col++) {
								colid = dbaltcolid(dbproc, dbrc, col);
								while (selcol < colid) {
									collen = get_printable_column_size(dbproc, selcol);
									for (i = 0; i < collen; i++) {
										putchar(' ');
									}
									selcol++;
									printf("%s", colseparator);
								}
								collen = get_printable_column_size(dbproc, colid);
								adash = '-';
								for (i = 0; i < collen; i++) {
									putchar(adash);
								}
								selcol++;
								printf("%s", colseparator);
							}
							printf("%s", lineseparator);
							for (selcol = col = 1; col <= num_cols; col++) {
								colid = dbaltcolid(dbproc, dbrc, col);
								while (selcol < colid) {
									collen = get_printable_column_size(dbproc, selcol);
									for (i = 0; i < collen; i++) {
										putchar(' ');
									}
									selcol++;
									printf("%s", colseparator);
								}
								convlen = dbconvert(dbproc,
										    dbalttype(dbproc, dbrc, col),
										    dbadata(dbproc, dbrc, col),
										    dbadlen(dbproc, dbrc, col),
										    SYBCHAR, (BYTE *) adbuf, sizeof(adbuf));
								printf("%.*s", (int) convlen, adbuf);
								collen = get_printable_column_size(dbproc, colid);
								collen -= convlen;
								while (collen-- > 0) {
									putchar(' ');
								}
								selcol++;
								printf("%s", colseparator);
							}
							printf("%s", lineseparator);
							printedcompute = 1;
							continue;
						}
						if (printedcompute || (headers && (printedlines >= headers)
								       && ((printedlines % headers) == 0))) {
							fputc('\n', stdout);
							dbsprhead(dbproc, prbuf, prbuflen);
							fputs(prbuf, stdout);
							fputc('\n', stdout);
							dbsprline(dbproc, prbuf, prbuflen, '-');
							fputs(prbuf, stdout);
							fputc('\n', stdout);
							printedcompute = 0;
						}
						printedlines++;
						dbspr1row(dbproc, prbuf, prbuflen);
						fputs(prbuf, stdout);
						fputc('\n', stdout);
						maybe_handle_active_interrupt();
					}
					fputc('\n', stdout);
					free(prbuf);
					maybe_handle_active_interrupt();
				}
#endif
				if (dbrc != FAIL) {
					if ((DBCOUNT(dbproc) >= 0) || dbhasretstat(dbproc)) {
						if (DBCOUNT(dbproc) >= 0) {
							printf("(%d rows affected", (int) DBCOUNT(dbproc));
							if (dbhasretstat(dbproc)) {
								status_printed = 1;
								dbrc = dbretstatus(dbproc);
								printf(", return status = %d", dbrc);
							}
							printf(")\n");
						} else {
							if (dbhasretstat(dbproc)) {
								status_printed = 1;
								dbrc = dbretstatus(dbproc);
								printf("(return status = %d)\n", dbrc);
							}
						}
					}
				}
			}
			if (!status_printed && dbhasretstat(dbproc)) {
				dbrc = dbretstatus(dbproc);
				printf("(return status = %d)\n", dbrc);
			}
		} else {
			/* Something failed, so change the default
			 * exit status to reflect that.
			 */
			default_exit = EXIT_FAILURE;
		}
	}
	reset_term();
	dbexit();
	return EXIT_FAILURE;
}

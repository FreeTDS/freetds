/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2004-2011  James K. Lowden
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

#ifdef MicrosoftsDbLib
#ifdef _WIN32
# pragma warning( disable : 4142 )
# include "win32.microsoft/have.h"
# include "../../include/replacements.win32.hacked.h"
int getopt(int argc, const char *argv[], char *optstring);

# ifndef DBNTWIN32
#  define DBNTWIN32

/*
 * As of Visual Studio .NET 2003, define WIN32_LEAN_AND_MEAN to avoid redefining LPCBYTE in sqlfront.h
 * Unless it was already defined, undefine it after windows.h has been included.
 * (windows.h includes a bunch of stuff needed by sqlfront.h.  Bleh.)
 */
#  ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN_DEFINED_HERE
#  endif
#  include <freetds/windows.h>
#  ifdef WIN32_LEAN_AND_MEAN_DEFINED_HERE
#   undef WIN32_LEAN_AND_MEAN_DEFINED_HERE
#   undef WIN32_LEAN_AND_MEAN
#  endif
#  include <process.h>
#  include <sqlfront.h>
#  include <sqldb.h>

#endif /* DBNTWIN32 */
# include "win32.microsoft/syb2ms.h"
#endif
#endif /* MicrosoftsDbLib */

#include <config.h>

#include <stdio.h>
#include <assert.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_LIBGEN_H
#include <libgen.h>
#endif

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#include <sybfront.h>
#include <sybdb.h>
#ifndef MicrosoftsDbLib
#include <freetds/replacements.h>
#else

#ifndef _WIN32
# include <freetds/replacements.h>
#endif
#endif /* MicrosoftsDbLib */

#include <freetds/sysdep_private.h>
#include <freetds/utils.h>
#include <freetds/bool.h>
#include <freetds/macros.h>

#ifndef MicrosoftsDbLib
static int err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
static int msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext,
		char *srvname, char *procname, int line);
#else
static int err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, const char dberrstr[], const char oserrstr[]);
static int msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, const char msgtext[],
		const char srvname[], const char procname[], unsigned short int line);
#endif /* MicrosoftsDbLib */

typedef struct _options
{
	int 	optind;
	char 	*servername,
		*database,
		*appname,
		 hostname[128],
		*input_filename,
		*output_filename;
} OPTIONS;

typedef struct _procedure
{
	char 	 name[512], owner[512];
} PROCEDURE;

typedef struct DDL {
	char *name;
	char *type;
	char *length;
	char *precision;
	char *scale;
	char *nullable;
} DDL;

static int print_ddl(DBPROCESS *dbproc, PROCEDURE *procedure);
static int print_results(DBPROCESS *dbproc);
static LOGINREC* get_login(int argc, char *argv[], OPTIONS *poptions);
static void parse_argument(const char argument[], PROCEDURE* procedure);
static void usage(const char invoked_as[]);
static char *rtrim(char *s);
static char *ltrim(char *s);

typedef struct tmp_buf {
	struct tmp_buf *next;
	char buf[1];
} tmp_buf;

static tmp_buf *tmp_list = NULL;

static void*
tmp_malloc(size_t len)
{
	tmp_buf *tmp = malloc(sizeof(tmp_buf*) + len);
	if (!tmp) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}
	tmp->next = tmp_list;
	tmp_list = tmp;
	return tmp->buf;
}

static void
tmp_free(void)
{
	while (tmp_list) {
		tmp_buf *next = tmp_list->next;
		free(tmp_list);
		tmp_list = next;
	}
}

static size_t
count_chars(const char *s, char c)
{
	size_t num = 0;
	if (c != 0) {
		--s;
		while ((s = strchr(s + 1, c)) != NULL)
			++num;
	}
	return num;
}

static char *
sql_quote(char *dest, const char *src, char quote_char)
{
	for (; *src; ++src) {
		if (*src == quote_char)
			*dest++ = *src;
		*dest++ = *src;
	}
	return dest;
}

static const char *
quote_id(const char *id)
{
	size_t n, len;
	char *s, *p;

	n = count_chars(id, ']');
	len = 1 + strlen(id) + n + 1 + 1;
	p = s = tmp_malloc(len);
	*p++ = '[';
	p = sql_quote(p, id, ']');
	*p++ = ']';
	*p = 0;
	return s;
}

static const char *
quote_str(const char *str)
{
	size_t n, len;
	char *s, *p;

	n = count_chars(str, '\'');
	if (!n)
		return str;
	len = strlen(str) + n + 1;
	s = tmp_malloc(len);
	p = sql_quote(s, str, '\'');
	*p = 0;
	return s;
}

/* global variables */
static OPTIONS options;
static char use_statement[512];
/* end global variables */


/**
 * The purpose of this program is to load or extract the text of a stored procedure.
 * This first cut does extract only.
 * TODO: support loading procedures onto the server.
 */
int
main(int argc, char *argv[])
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	PROCEDURE procedure;
	RETCODE erc;
	int i, nrows;

	setlocale(LC_ALL, "");

#ifdef __VMS
        /* Convert VMS-style arguments to Unix-style */
        parse_vms_args(&argc, &argv);
#endif

	/* Initialize db-lib */
#if _WIN32 && defined(MicrosoftsDbLib)
	LPCSTR msg = dbinit();
	if (msg == NULL) {
#else
	erc = dbinit();
	if (erc == FAIL) {
#endif /* MicrosoftsDbLib */
		fprintf(stderr, "%s:%d: dbinit() failed\n", options.appname, __LINE__);
		exit(1);
	}


	memset(&options, 0, sizeof(options));
	login = get_login(argc, argv, &options); /* get command-line parameters and call dblogin() */
	assert(login != NULL);

	/* Install our error and message handlers */
	dberrhandle(err_handler);
	dbmsghandle(msg_handler);

	/*
	 * Override stdin, stdout, and stderr, as required
	 */
	if (options.input_filename) {
		if (freopen(options.input_filename, "rb", stdin) == NULL) {
			fprintf(stderr, "%s: unable to open %s: %s\n", options.appname, options.input_filename, strerror(errno));
			exit(1);
		}
	}

	if (options.output_filename) {
		if (freopen(options.output_filename, "wb", stdout) == NULL) {
			fprintf(stderr, "%s: unable to open %s: %s\n", options.appname, options.output_filename, strerror(errno));
			exit(1);
		}
	}

	/* Select the specified database, if any */
	if (options.database)
		DBSETLDBNAME(login, options.database);

	/*
	 * Connect to the server
	 */
	dbproc = dbopen(login, options.servername);
	if (!dbproc) {
		fprintf(stderr, "There was a problem connecting to the server.\n");
		exit(1);
	}

	/*
	 * Read the procedure names and move their texts.
	 */
	for (i=options.optind; i < argc; i++) {
#ifndef MicrosoftsDbLib
		static const char query[] = " select	c.text"
#else
		static const char query[] = " select	cast(c.text as text)"
#endif /* MicrosoftsDbLib */
					 ", number "
					 " from syscomments c,"
					 "      sysobjects o"
					 " where	o.id = c.id"
					 " and		o.name = '%s'"
					 " and		o.uid = user_id('%s')"
					 " and		o.type not in ('U', 'S')" /* no user or system tables */
					 " order by 	c.number, %sc.colid"
					;
		static const char query_table[] = " execute sp_help '%s.%s' ";

		parse_argument(argv[i], &procedure);

		erc = dbfcmd(dbproc, query, quote_str(procedure.name), quote_str(procedure.owner),
			     (DBTDS(dbproc) == DBTDS_5_0) ? "c.colid2, ":"");
		tmp_free();

		/* Send the query to the server (we could use dbsqlexec(), instead) */
		erc = dbsqlsend(dbproc);
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbsqlsend() failed\n", options.appname, __LINE__);
			exit(1);
		}

		/* Wait for it to execute */
		erc = dbsqlok(dbproc);
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbsqlok() failed\n", options.appname, __LINE__);
			exit(1);
		}

		/* Write the output */
		nrows = print_results(dbproc);

		if (0 == nrows) {
			erc = dbfcmd(dbproc, query_table, quote_str(procedure.owner), quote_str(procedure.name));
			tmp_free();
			assert(SUCCEED == erc);
			erc = dbsqlexec(dbproc);
			if (erc == FAIL) {
				fprintf(stderr, "%s:%d: dbsqlexec() failed\n", options.appname, __LINE__);
				exit(1);
			}
			nrows = print_ddl(dbproc, &procedure);
		}

		switch (nrows) {
		case -1:
			return 1;
		case 0:
			fprintf(stderr, "%s: error: %s.%s.%s.%s not found\n", options.appname,
					options.servername, options.database, procedure.owner, procedure.name);
			return 2;
		default:
			break;
		}
	}

	return 0;
}

static void
parse_argument(const char argument[], PROCEDURE* procedure)
{
	const char *s = strchr(argument, '.');

	if (s) {
		size_t len = s - argument;
		if (len > sizeof(procedure->owner) - 1)
			len = sizeof(procedure->owner) - 1;
		memcpy(procedure->owner, argument, len);
		procedure->owner[len] = '\0';

		strlcpy(procedure->name, s+1, sizeof(procedure->name));
	} else {
		strcpy(procedure->owner, "dbo");
		strlcpy(procedure->name, argument, sizeof(procedure->name));
	}
}

static char *
rtrim(char *s)
{
	char *p = strchr(s, '\0');

	while (--p >= s && *p == ' ')
		;
	*(p + 1) = '\0';

	return s;
}

static char *
ltrim(char *s)
{
	char *p = s;

	while (*p == ' ')
		++p;
	memmove(s, p, strlen(p) + 1);

	return s;
}

static bool
is_in(const char *item, const char *list)
{
	const size_t item_len = strlen(item);
	for (;;) {
		size_t len = strlen(list);
		if (len == 0)
			return false;
		if (len == item_len && strcasecmp(item, list) == 0)
			return true;
		list += len + 1;
	}
}

static void
search_columns(DBPROCESS *dbproc, int *colmap, const char *const *colmap_names, int num_cols)
{
	int i;

	assert(dbproc && colmap && colmap_names);
	assert(num_cols > 0);

	/* Find the columns we need */
	for (i = 0; i < num_cols; ++i)
		colmap[i] = -1;
	for (i = 1; i <= dbnumcols(dbproc); ++i) {
		const char *name = dbcolname(dbproc, i);
		int j;

		for (j = 0; j < num_cols; ++j) {
			if (is_in(name, colmap_names[j])) {
				colmap[j] = i;
				break;
			}
		}
	}
	for (i = 0; i < num_cols; ++i) {
		if (colmap[i] == -1) {
			fprintf(stderr, "Expected column name %s not found\n", colmap_names[i]);
			exit(1);
		}
	}
}

static int
find_column_name(const char *start, const DDL *columns, int num_columns)
{
	size_t start_len = strlen(start);
	size_t found_len = 0;
	int i, found = -1;

	for (i = 0; i < num_columns; ++i) {
		const char *const name = columns[i].name;
		const size_t name_len = strlen(name);
		if (name_len <= start_len && name_len > found_len
		    && (start[name_len] == 0 || strncmp(start+name_len, ", ", 2) == 0)
		    && memcmp(name, start, name_len) == 0) {
			found_len = name_len;
			found = i;
		}
	}
	return found;
}

/* This function split and quote index keys.
 * Index keys are separate by a command and a space (", ") however the
 * separator (very unlikely but possible can contain that separator)
 * so we use search for column names taking the longer we can.
 */
static char *
quote_index_keys(char *index_keys, const DDL *columns, int num_columns)
{
	size_t num_commas = count_chars(index_keys, ',');
	size_t num_quotes = count_chars(index_keys, ']');
	size_t max_len = strlen(index_keys) + num_quotes + (num_commas + 1) * 2 + 1;
	char *const new_index_keys = malloc(max_len);
	char *dest;
	bool first = true;
	assert(new_index_keys);
	dest = new_index_keys;
	while (*index_keys) {
		int icol = find_column_name(index_keys, columns, num_columns);
		/* Sybase put a space at the beginning, handle it */
		if (icol < 0 && index_keys[0] == ' ') {
			icol = find_column_name(index_keys + 1, columns, num_columns);
			if (icol >= 0)
				++index_keys;
		}
		if (!first)
			*dest++ = ',';
		*dest++ = '[';
		if (icol >= 0) {
			/* found a column matching, use the name */
			dest = sql_quote(dest, columns[icol].name, ']');
			index_keys += strlen(columns[icol].name);
		} else {
			/* not found, fallback looking for terminator */
			char save;
			char *end = strstr(index_keys, ", ");
			if (!end)
				end = strchr(index_keys, 0);
			save = *end;
			*end = 0;
			dest = sql_quote(dest, index_keys, ']');
			*end = save;
			index_keys = end;
		}
		*dest++ = ']';
		if (strncmp(index_keys, ", ", 2) == 0)
			index_keys += 2;
		first = false;
	}
	*dest = 0;
	return new_index_keys;
}

static char *
parse_index_row(DBPROCESS *dbproc, PROCEDURE *procedure, char *create_index, const DDL *columns, int num_columns)
{
	static const char *const colmap_names[3] = {
		"index_name\0",
		"index_description\0",
		"index_keys\0",
	};
	int colmap[TDS_VECTOR_SIZE(colmap_names)];
	char *index_name, *index_description, *index_keys, *p, fprimary=0;
	char *tmp_str;
	DBINT datlen;
	int ret, i;

	assert(dbnumcols(dbproc) >=3 );	/* column had better be in range */

	search_columns(dbproc, colmap, colmap_names, TDS_VECTOR_SIZE(colmap));

	/* name */
	datlen = dbdatlen(dbproc, 1);
	index_name = (char *) tds_strndup(dbdata(dbproc, 1), datlen);
	assert(index_name);

	/* kind */
	i = colmap[1];
	datlen = dbdatlen(dbproc, i);
	index_description = (char *) tds_strndup(dbdata(dbproc, i), datlen);
	assert(index_description);

	/* columns */
	i = colmap[2];
	datlen = dbdatlen(dbproc, i);
	index_keys = (char *) tds_strndup(dbdata(dbproc, i), datlen);
	assert(index_keys);

	tmp_str = quote_index_keys(index_keys, columns, num_columns);
	free(index_keys);
	index_keys = tmp_str;

	/* fix up the index attributes; we're going to use the string verbatim (almost). */
	p = strstr(index_description, "located");
	if (p) {
		*p = '\0'; /* we don't care where it's located */
	}
	/* Microsoft version: [non]clustered[, unique][, primary key] located on PRIMARY */
	p = strstr(index_description, "primary key");
	if (p) {
		fprimary = 1;
		*p = '\0'; /* we don't care where it's located */
		if ((p = strchr(index_description, ',')) != NULL)
			*p = '\0'; /* we use only the first term (clustered/nonclustered) */
	} else {
		/* reorder "unique" and "clustered" */
		char nonclustered[] = "nonclustered", unique[] = "unique";
		char *pclustering = nonclustered;
		if (NULL == strstr(index_description, pclustering)) {
			pclustering += 3;
			if (NULL == strstr(index_description, pclustering))
				*pclustering = '\0';
		}
		if (NULL == strstr(index_description, unique))
			unique[0] = '\0';
		sprintf(index_description, "%s %s", unique, pclustering);
	}
	/* Put it to a temporary variable; we'll print it after the CREATE TABLE statement. */
	tmp_str = create_index;
	create_index = create_index ? create_index : "";
	if (fprimary) {
		ret = asprintf(&create_index,
			"%sALTER TABLE %s.%s ADD CONSTRAINT %s PRIMARY KEY %s (%s)\nGO\n\n",
			create_index,
			quote_id(procedure->owner), quote_id(procedure->name),
			quote_id(index_name), index_description, index_keys);
	} else {
		ret = asprintf(&create_index,
			"%sCREATE %s INDEX %s on %s.%s(%s)\nGO\n\n",
			create_index,
			index_description, quote_id(index_name),
			quote_id(procedure->owner), quote_id(procedure->name), index_keys);
	}
	assert(ret >= 0);
	free(tmp_str);
	tmp_free();

	free(index_name);
	free(index_description);
	free(index_keys);

	return create_index;
}

/*
 * Get the table information from sp_help, because it's easier to get the index information (eventually).
 * The column descriptions are in resultset #2, which is where we start.
 * As shown below, sp_help returns different columns for resultset #2, so we build a map.
 * 	    Sybase 	   	   Microsoft
 *	    -----------------      ----------------
 *	 1. Column_name            Column_name
 *	 2. Type		   Type
 *       3.                        Computed
 *	 4. Length		   Length
 *	 5. Prec		   Prec
 *	 6. Scale		   Scale
 *	 7. Nulls		   Nullable
 *	 8. Default_name	   TrimTrail
 *	 9. Rule_name              FixedLenNullIn
 *	10. Access_Rule_name       Collation
 *	11. Identity
 */
static int
print_ddl(DBPROCESS *dbproc, PROCEDURE *procedure)
{
	static const char *const colmap_names[6] = {
		"column_name\0",
		"type\0",
		"length\0",
		"prec\0",
		"scale\0",
		"nulls\0nullable\0",
	};

	DDL *ddl = NULL;
	char *create_index = NULL;
	RETCODE erc;
	int iresultset, i;
	int maxnamelen = 0, nrows = 0;
	char **p_str;
	bool is_ms = false;

	assert(dbproc);
	assert(procedure);

	/* sp_help returns several result sets.  We want just the second one, for now */
	for (iresultset=1; (erc = dbresults(dbproc)) != NO_MORE_RESULTS; iresultset++) {
		int row_code;

		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbresults(), result set %d failed\n", options.appname, __LINE__, iresultset);
			goto cleanup;
		}

		/* Get the data */
		while ((row_code = dbnextrow(dbproc)) != NO_MORE_ROWS) {
			DDL *p;
			char **coldesc[sizeof(DDL)/sizeof(char*)];	/* an array of pointers to the DDL elements */
			int colmap[TDS_VECTOR_SIZE(colmap_names)];

			assert(row_code == REG_ROW);

			/* Look for index data */
			if (0 == strcmp("index_name", dbcolname(dbproc, 1))) {
				create_index = parse_index_row(dbproc, procedure, create_index, ddl, nrows);
				continue;
			}

			/* skip other resultsets that don't describe the table's columns */
			if (0 != strcasecmp("Column_name", dbcolname(dbproc, 1)))
				continue;

			/* Find the columns we need */
			search_columns(dbproc, colmap, colmap_names, TDS_VECTOR_SIZE(colmap));

			/* check if server is Microsoft */
			for (i = 1; i <= dbnumcols(dbproc); ++i)
				if (strcasecmp(dbcolname(dbproc, i), "Collation") == 0) {
					is_ms = true;
					break;
				}

			/* Make room for the next row */
			p = (DDL *) realloc(ddl, ++nrows * sizeof(DDL));
			if (p == NULL) {
				perror("error: insufficient memory for row DDL");
				assert(p !=  NULL);
				exit(1);
			}
			ddl = p;

			/* take the address of each member, so we can loop through them */
			coldesc[0] = &ddl[nrows-1].name;
			coldesc[1] = &ddl[nrows-1].type;
			coldesc[2] = &ddl[nrows-1].length;
			coldesc[3] = &ddl[nrows-1].precision;
			coldesc[4] = &ddl[nrows-1].scale;
			coldesc[5] = &ddl[nrows-1].nullable;

			for( i=0; i < sizeof(DDL)/sizeof(char*); i++) {
				const int col_index = colmap[i];
				const DBINT datlen = dbdatlen(dbproc, col_index);
				const int type = dbcoltype(dbproc, col_index);

				assert(datlen >= 0);	/* column had better be in range */

				if (datlen == 0) {
					*coldesc[i] = NULL;
					continue;
				}

				if (type == SYBCHAR || type == SYBVARCHAR) {
					*coldesc[i] = (char *) tds_strndup(dbdata(dbproc, col_index), datlen);
					if (!*coldesc[i]) {
						perror("error: insufficient memory for row detail");
						exit(1);
					}
				} else {
					char buf[256];
					DBINT len = dbconvert(dbproc, type, dbdata(dbproc, col_index), datlen,
						SYBVARCHAR, (BYTE *) buf, -1);
					if (len < 0) {
						fprintf(stderr, "Error converting column to char");
						exit(1);
					}
					*coldesc[i] = strdup(buf);
					if (!*coldesc[i]) {
						perror("error: insufficient memory for row detail");
						exit(1);
					}
				}

				/*
				 * maxnamelen will determine how much room we allow for column names
				 * in the CREATE TABLE statement
				 */
				if (i == 0)
					maxnamelen = (maxnamelen > datlen)? maxnamelen : datlen;
			}
		} /* wend */
	}

	/*
	 * We've collected the description for the columns in the 'ddl' array.
	 * Now we'll print the CREATE TABLE statement in jkl's preferred format.
	 */
	if (nrows == 0)
		goto cleanup;

	printf("%sCREATE TABLE %s.%s\n", use_statement, quote_id(procedure->owner), quote_id(procedure->name));
	tmp_free();
	for (i=0; i < nrows; i++) {
		static const char varytypenames[] =	"char\0"
							"nchar\0"
							"varchar\0"
							"nvarchar\0"
							"unichar\0"
							"univarchar\0"
							"binary\0"
							"varbinary\0"
							;
		char *type = NULL;
		bool is_null;
		int ret;

		/* get size of decimal, numeric, char, and image types */
		ret = 0;
		if (is_in(ddl[i].type, "decimal\0numeric\0")) {
			if (ddl[i].precision && 0 != strcasecmp("NULL", ddl[i].precision)) {
				rtrim(ddl[i].precision);
				rtrim(ddl[i].scale);
				ret = asprintf(&type, "%s(%s,%s)", ddl[i].type, ddl[i].precision, ddl[i].scale);
			}
		} else if (is_in(ddl[i].type, varytypenames)) {
			ltrim(rtrim(ddl[i].length));
			if (strcmp(ddl[i].length, "-1") == 0)
				ret = asprintf(&type, "%s(max)", ddl[i].type);
			else if (is_ms && is_in(ddl[i].type, "nchar\0nvarchar\0"))
				ret = asprintf(&type, "%s(%d)", ddl[i].type, atoi(ddl[i].length)/2);
			else
				ret = asprintf(&type, "%s(%s)", ddl[i].type, ddl[i].length);
		}
		assert(ret >= 0);

		ltrim(rtrim(ddl[i].nullable));
		is_null = is_in(ddl[i].nullable, "1\0yes\0");

		/*      {(|,} name type [NOT] NULL */
		printf("\t%c %-*s %-15s %3s NULL\n", (i==0? '(' : ','), maxnamelen+2, quote_id(ddl[i].name),
						(type? type : ddl[i].type), (is_null? "" : "NOT"));
		tmp_free();

		free(type);
	}
	printf("\t)\nGO\n\n");

	/* print the CREATE INDEX statements */
	if (create_index != NULL)
		fputs(create_index, stdout);

cleanup:
	p_str = (char **) ddl;
	for (i=0; i < nrows * (sizeof(DDL)/sizeof(char*)); ++i)
		free(p_str[i]);
	free(ddl);
	free(create_index);
	return nrows;
}

static int /* return count of SQL text rows */
print_results(DBPROCESS *dbproc)
{
	RETCODE erc;
	int row_code;
	int iresultset;
	int nrows=0;
	int prior_procedure_number=1;

	/* bound variables */
	enum column_id { ctext=1, number=2 };
	char sql_text[16002];
	int	 sql_text_status;
	int	 procedure_number; /* for create proc abc;2 */
	int	 procedure_number_status;

	/*
	 * Set up each result set with dbresults()
	 */
	for (iresultset=1; (erc = dbresults(dbproc)) != NO_MORE_RESULTS; iresultset++) {
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbresults(), result set %d failed\n", options.appname, __LINE__, iresultset);
			return -1;
		}

		if (SUCCEED != DBROWS(dbproc)) {
			return 0;
		}

		/*
		 * Bind the columns to our variables.
		 */
		if (sizeof(sql_text) <= dbcollen(dbproc, ctext) ) {
			assert(sizeof(sql_text) > dbcollen(dbproc, ctext));
			return 0;
		}
		erc = dbbind(dbproc, ctext, STRINGBIND, 0, (BYTE *) sql_text);
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbbind(), column %d failed\n", options.appname, __LINE__, ctext);
			return -1;
		}
		erc = dbnullbind(dbproc, ctext, &sql_text_status);
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbnullbind(), column %d failed\n", options.appname, __LINE__, ctext);
			return -1;
		}

		erc = dbbind(dbproc, number, INTBIND, -1, (BYTE *) &procedure_number);
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbbind(), column %d failed\n", options.appname, __LINE__, number);
			return -1;
		}
		erc = dbnullbind(dbproc, number, &procedure_number_status);
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbnullbind(), column %d failed\n", options.appname, __LINE__, number);
			return -1;
		}

		/*
		 * Print the data to stdout.
		 */
		printf("%s", use_statement);
		for (;(row_code = dbnextrow(dbproc)) != NO_MORE_ROWS; nrows++, prior_procedure_number = procedure_number) {
			switch (row_code) {
			case REG_ROW:
				if ( -1 == sql_text_status) {
					fprintf(stderr, "defncopy: error: unexpected NULL row in SQL text\n");
				} else {
					if (prior_procedure_number != procedure_number)
						printf("\nGO\n");
					printf("%s", sql_text);
				}
				break;
			case BUF_FULL:
			default:
				fprintf(stderr, "defncopy: error: expected REG_ROW (%d), got %d instead\n", REG_ROW, row_code);
				assert(row_code == REG_ROW);
				break;
			} /* row_code */

		} /* wend dbnextrow */
		printf("\nGO\n");

	} /* wend dbresults */
	return nrows;
}

static void
usage(const char invoked_as[])
{
	fprintf(stderr, "usage:  %s \n"
			"        [-U username] [-P password]\n"
			"        [-S servername] [-D database]\n"
			"        [-i input filename] [-o output filename]\n"
			"        [owner.]object_name [[owner.]object_name...]\n"
			, invoked_as);
/**
defncopy Syntax Error
Usage: defncopy
    [-v]
    [-X]
--  [-a <display_charset>]
--  [-I <interfaces_file>]
--  [-J [<client_charset>]]
--  [-K <keytab_file>]
    [-P <password>]
--  [-R <remote_server_principal>]
    [-S [<server_name>]]
    [-U <user_name>]
--  [-V <security_options>]
--  [-Z <security_mechanism>]
--  [-z <language>]
    { in <file_name> <database_name> |
      out <file_name> <database_name> [<owner>.]<object_name>
          [[<owner>.]<object_name>...] }
**/
}

static LOGINREC *
get_login(int argc, char *argv[], OPTIONS *options)
{
	LOGINREC *login;
	char *password;
	int ch;
	int fdomain = TRUE;

	extern char *optarg;
	extern int optind;

	assert(options && argv);

	options->appname = basename(argv[0]);

	login = dblogin();

	if (!login) {
		fprintf(stderr, "%s: unable to allocate login structure\n", options->appname);
		exit(1);
	}

	DBSETLAPP(login, options->appname);

	if (-1 != gethostname(options->hostname, sizeof(options->hostname))) {
		DBSETLHOST(login, options->hostname);
	}

	while ((ch = getopt(argc, argv, "U:P:S:d:D:i:o:v")) != -1) {
		switch (ch) {
		case 'U':
			DBSETLUSER(login, optarg);
			fdomain = FALSE;
			break;
		case 'P':
			password = tds_getpassarg(optarg);
			if (!password) {
				fprintf(stderr, "Error getting password\n");
				exit(1);
			}
			DBSETLPWD(login, password);
			memset(password, 0, strlen(password));
			free(password);
			password = NULL;
			fdomain = FALSE;
			break;
		case 'S':
			options->servername = strdup(optarg);
			break;
		case 'd':
		case 'D':
			options->database = strdup(optarg);
			break;
		case 'i':
			options->input_filename = strdup(optarg);
			break;
		case 'o':
			options->output_filename = strdup(optarg);
			break;
		case 'v':
			printf("%s\n\n%s", argv[0],
				"Copyright (C) 2004-2011  James K. Lowden\n\n"
				"This program  is free software; you can redistribute it and/or\n"
				"modify it under the terms of the GNU General Public\n"
				"License as published by the Free Software Foundation; either\n"
				"version 2 of the License, or (at your option) any later version.\n\n"
				"This library is distributed in the hope that it will be useful,\n"
				"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
				"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
				"Library General Public License for more details.\n\n"
				"You should have received a copy of the GNU General Public\n"
				"License along with this library; if not, write to the\n"
				"Free Software Foundation, Inc., 59 Temple Place - Suite 330,\n"
				"Boston, MA 02111-1307, USA.\n");
				exit(1);
			break;
		case '?':
		default:
			usage(options->appname);
			exit(1);
		}
	}

#if defined(MicrosoftsDbLib) && defined(_WIN32)
	if (fdomain)
		DBSETLSECURE(login);
#else
	if (fdomain)
		DBSETLNETWORKAUTH(login, TRUE);
#endif /* MicrosoftsDbLib */
	if (!options->servername) {
		usage(options->appname);
		exit(1);
	}

	options->optind = optind;

	return login;
}

static int
#ifndef MicrosoftsDbLib
err_handler(DBPROCESS * dbproc TDS_UNUSED, int severity, int dberr, int oserr TDS_UNUSED,
	    char *dberrstr, char *oserrstr TDS_UNUSED)
#else
err_handler(DBPROCESS * dbproc TDS_UNUSED, int severity, int dberr, int oserr TDS_UNUSED,
	    const char dberrstr[], const char oserrstr[] TDS_UNUSED)
#endif /* MicrosoftsDbLib */
{

	if (dberr) {
		fprintf(stderr, "%s: Msg %d, Level %d\n", options.appname, dberr, severity);
		fprintf(stderr, "%s\n\n", dberrstr);
	} else {
		fprintf(stderr, "%s: DB-LIBRARY error:\n\t", options.appname);
		fprintf(stderr, "%s\n", dberrstr);
	}

	return INT_CANCEL;
}

static int
#ifndef MicrosoftsDbLib
msg_handler(DBPROCESS * dbproc TDS_UNUSED, DBINT msgno, int msgstate TDS_UNUSED, int severity TDS_UNUSED, char *msgtext,
	    char *srvname TDS_UNUSED, char *procname TDS_UNUSED, int line TDS_UNUSED)
#else
msg_handler(DBPROCESS * dbproc TDS_UNUSED, DBINT msgno, int msgstate TDS_UNUSED, int severity TDS_UNUSED, const char msgtext[],
	    const char srvname[] TDS_UNUSED, const char procname[] TDS_UNUSED, unsigned short int line TDS_UNUSED)
#endif /* MicrosoftsDbLib */
{
	char *dbname, *endquote;

	switch (msgno) {
	case 5701: /* Print "USE dbname" for "Changed database context to 'dbname'" */
		dbname = strchr(msgtext, '\'');
		if (!dbname)
			break;
		endquote = strchr(++dbname, '\'');
		if (!endquote)
			break;
		*endquote = '\0';
		sprintf(use_statement, "USE %s\nGO\n\n", quote_id(dbname));
		tmp_free();
		return 0;

	case 0:	/* Ignore print messages */
	case 5703:	/* Ignore "Changed language setting to <language>". */
		return 0;

	default:
		break;
	}

#if 0
	printf("Msg %ld, Severity %d, State %d\n", (long) msgno, severity, msgstate);

	if (strlen(srvname) > 0)
		printf("Server '%s', ", srvname);
	if (strlen(procname) > 0)
		printf("Procedure '%s', ", procname);
	if (line > 0)
		printf("Line %d", line);
#endif
	printf("\t/* %s */\n", msgtext);

	return 0;
}

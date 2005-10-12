/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2004  James K. Lowden
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

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

#include <sqlfront.h>
#include <sybdb.h>
#include "replacements.h"

static char software_version[] = "$Id: defncopy.c,v 1.12 2005-10-12 07:03:21 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
static int msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, 
		char *srvname, char *procname, int line);

struct METADATA { char *name, *source, *format_string; int type, size; };

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

static int print_ddl(DBPROCESS *dbproc, PROCEDURE *procedure);
static int print_results(DBPROCESS *dbproc);
static LOGINREC* get_login(int argc, char *argv[], OPTIONS *poptions);
static void parse_argument(const char argument[], PROCEDURE* procedure);
static int set_format_string(struct METADATA * meta, const char separator[]);
static int get_printable_size(int type, int size);
static void usage(const char invoked_as[]);

/* global variables */
OPTIONS options;
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

	/* Initialize db-lib */
	erc = dbinit();	
	if (erc == FAIL) {
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
		if (freopen(options.input_filename, "r", stdin) == NULL) {
			fprintf(stderr, "%s: unable to open %s: %s\n", options.appname, options.input_filename, strerror(errno));
			exit(1);
		}
	}

	if (options.output_filename) {
		if (freopen(options.output_filename, "w", stdout) == NULL) {
			fprintf(stderr, "%s: unable to open %s: %s\n", options.appname, options.output_filename, strerror(errno));
			exit(1);
		}
	}
	
	/* 
	 * Connect to the server 
	 */
	dbproc = dbopen(login, options.servername);
	assert(dbproc != NULL);
	
	/* Switch to the specified database, if any */
	if (options.database)
		dbuse(dbproc, options.database);

	/* 
	 * Read the procedure names and move their texts.  
	 */
	for (i=options.optind; i < argc; i++) {
		static const char query[] = " select	c.text"
					 " from	syscomments  as c"
					 " join 	sysobjects as o"
					 " on 		o.id = c.id"
					 " where	o.name = '%s'"
					 " and 	o.uid = user_id('%s')"
					 " and		o.type not in ('U', 'S')" /* no user or system tables */
					 " order by 	c.colid"
					;
		static const char query_table[] = " execute sp_help '%s.%s' ";

		parse_argument(argv[i], &procedure);

		erc = dbfcmd(dbproc, query, procedure.name, procedure.owner); 
	
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
			erc = dbfcmd(dbproc, query_table, procedure.owner, procedure.name);
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

		tds_strlcpy(procedure->name, s+1, sizeof(procedure->name));
	} else {
		strcpy(procedure->owner, "dbo");
		tds_strlcpy(procedure->name, argument, sizeof(procedure->name));
	}
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
int
print_ddl(DBPROCESS *dbproc, PROCEDURE *procedure) 
{
 	struct DDL { char *name, *type, *length, *precision, *scale, *nullable; } *ddl = NULL;
	static int microsoft_colmap[6] = {1,2,  4,5,6,7}, 
		      sybase_colmap[6] = {1,2,3,4,5,6  };
	int *colmap = NULL;

	FILE *create_index;
	RETCODE erc;
	int row_code, iresultset, i, ret;
	int maxnamelen = 0, nrows = 0;
	
	create_index = tmpfile();
	
	assert(dbproc);
	assert(procedure);
	assert(create_index);

	/* sp_help returns several result sets.  We want just the second one, for now */
	for (iresultset=1; (erc = dbresults(dbproc)) != NO_MORE_RESULTS; iresultset++) {
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbresults(), result set %d failed\n", options.appname, __LINE__, iresultset);
			return 0;
		}

		/* Get the data */
		while ((row_code = dbnextrow(dbproc)) != NO_MORE_ROWS) {
			struct DDL *p;
			char **coldesc[sizeof(struct DDL)/sizeof(char*)];	/* an array of pointers to the DDL elements */
			
			assert(row_code == REG_ROW);
			
			/* Look for index data */
			if (0 == strcmp("index_name", dbcolname(dbproc, 1))) {
				char *index_name, *index_description, *index_keys, *p, fprimary=0;
				DBINT datlen;
				
				assert(dbnumcols(dbproc) >=3 );	/* column had better be in range */

				/* name */
				datlen = dbdatlen(dbproc, 1);
				index_name = calloc(1, 1 + datlen);
				assert(index_name);
				memcpy(index_name, dbdata(dbproc, 1), datlen);
				
				/* kind */
				datlen = dbdatlen(dbproc, 2);
				index_description = calloc(1, 1 + datlen);
				assert(index_description);
				memcpy(index_description, dbdata(dbproc, 2), datlen);
				
				/* columns */
				datlen = dbdatlen(dbproc, 3);
				index_keys = calloc(1, 1 + datlen);
				assert(index_keys);
				memcpy(index_keys, dbdata(dbproc, 3), datlen);
				
				/* fix up the index attributes; we're going to use the string verbatim (almost). */
				p = strstr(index_description, "located");
				if (p) {
					*p = '\0'; /* we don't care where it's located */
				}
				/* Microsoft version: clustered, unique, primary key located on PRIMARY */
				p = strstr(index_description, "primary key");
				if (p) {
					fprimary = 1;
					*p = '\0'; /* we don't care where it's located */
					if ((p = strchr(index_description, ',')) != NULL) 
						*p = '\0'; /* we use only the first term (clustered/nonclustered) */
				}
				while ((p = strchr(index_description, ',')) != NULL) {
					*p = ' ';	/* and we don't need the comma */
				}

				/* Put it to a temporary file; we'll print it after the CREATE TABLE statement. */
				if (fprimary) {
					fprintf(create_index, "ALTER TABLE %s.%s ADD CONSTRAINT %s PRIMARY KEY %s (%s)\nGO\n\n", 
						procedure->owner, procedure->name, index_name, index_description, index_keys); 
				} else {
					fprintf(create_index, "CREATE %s INDEX %s on %s.%s(%s)\nGO\n\n", 
						index_description, index_name, procedure->owner, procedure->name, index_keys);
				}
					
				free(index_name);
				free(index_description);
				free(index_keys);
				
				continue;
			}
			
			/* skip other resultsets that don't describe the table's columns */
			if (0 != strcmp("Column_name", dbcolname(dbproc, 1)))
				continue;
				
			/* Infer which columns we need from their names */
			colmap = (0 == strcmp("Computed", dbcolname(dbproc, 3)))? microsoft_colmap : sybase_colmap;
				
			/* Make room for the next row */
			p = realloc(ddl, ++nrows * sizeof(struct DDL));
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

			for( i=0; i < sizeof(struct DDL)/sizeof(char*); i++) {
				DBINT datlen = dbdatlen(dbproc, colmap[i]);

				assert(datlen >= 0);	/* column had better be in range */

				if (datlen == 0) {
					*coldesc[i] = NULL;
					continue;
				}

				*coldesc[i] = calloc(1, 1 + datlen); /* calloc will null terminate */
				if( *coldesc[i] == NULL ) {
					perror("error: insufficient memory for row detail");
					assert(*coldesc[i] != NULL);
					exit(1);
				}
				memcpy(*coldesc[i], dbdata(dbproc, colmap[i]), datlen);
				
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
	printf("CREATE TABLE %s.%s\n", procedure->owner, procedure->name);
	for (i=0; i < nrows; i++) {
		static const char *varytypenames[] =    { "char"  		
							, "nchar"  		
							, "varchar"  		
							, "nvarchar"  		
							, "text"  		
							, "ntext"  		
							, "unichar"  		
							, "univarchar"  		
							, "binary"  		
							, "varbinary"  		
							, "image"  		
							, NULL
							};
		const char **t;
		char *type = NULL;
		int is_null;

		/* get size of decimal, numeric, char, and image types */
		if (0 == strcasecmp("decimal", ddl[i].type) || 0 == strcasecmp("numeric", ddl[i].type)) {
			if (ddl[i].precision && 0 != strcasecmp("NULL", ddl[i].precision))
				ret = asprintf(&type, "%s(%d,%d)", ddl[i].type, *(int*)ddl[i].precision, *(int*)ddl[i].scale);
		} else {
			for (t = varytypenames; *t; t++) {
				if (0 == strcasecmp(*t, ddl[i].type)) {
					ret = asprintf(&type, "%s(%d)", ddl[i].type, *(int*)ddl[i].length);
					break;
				}
			}
		}

		if (colmap == sybase_colmap) 
			is_null = *(int*)ddl[i].nullable == 1;
		else 
			is_null = (0 == strcasecmp("1", ddl[i].nullable) || 0 == strcasecmp("yes", ddl[i].nullable));
			
		/*      {(|,} name type [NOT] NULL */
		printf("\t%c %-*s %-15s %3s NULL\n", (i==0? '(' : ','), maxnamelen, ddl[i].name, 
						(type? type : ddl[i].type), (is_null? "" : "NOT"));

		free(type);
	}
	printf("\t)\nGO\n\n");
	
	/* print the CREATE INDEX statements */
	rewind(create_index);
	while ((i = fgetc(create_index)) != EOF) {
		fputc(i, stdout);
	}
	
	fclose(create_index);
	return nrows;
}

int /* return count of SQL text rows */
print_results(DBPROCESS *dbproc) 
{
	static const char empty_string[] = "";
	struct METADATA *metadata = NULL;
	struct DATA { char *buffer; int status; } *data = NULL;
	
	RETCODE erc;
	int row_code;
	int c, ret;
	int iresultset;
	int nrows=0, ncols=0, ncomputeids;
	
	/* 
	 * Set up each result set with dbresults()
	 */
	for (iresultset=1; (erc = dbresults(dbproc)) != NO_MORE_RESULTS; iresultset++) {
		if (erc == FAIL) {
			fprintf(stderr, "%s:%d: dbresults(), result set %d failed\n", options.appname, __LINE__, iresultset);
			return -1;
		}

		if (SUCCEED != dbrows(dbproc)) {
			return 0;
		}

		/* Free prior allocations, if any. */
		for (c=0; c < ncols; c++) {
			free(metadata[c].format_string);
			free(data[c].buffer);
		}
		free(metadata);
		metadata = NULL;
		free(data);
		data = NULL;
		ncols = 0;
		
		/* 
		 * Allocate memory for metadata and bound columns 
		 */
		ncols = dbnumcols(dbproc);	

		metadata = (struct METADATA*) calloc(ncols, sizeof(struct METADATA));
		assert(metadata);

		data = (struct DATA*) calloc(ncols, sizeof(struct DATA));
		assert(data);
		
		/* The hard-coded queries don't generate compute rows. */
		ncomputeids = dbnumcompute(dbproc);
		assert(0 == ncomputeids);

		/* 
		 * For each column, get its name, type, and size. 
		 * Allocate a buffer to hold the data, and bind the buffer to the column.
		 * "bind" here means to give db-lib the address of the buffer we want filled as each row is fetched.
		 */

		for (c=0; c < ncols; c++) {
			int width;
			/* Get and print the metadata.  Optional: get only what you need. */
			char *name = dbcolname(dbproc, c+1);
			metadata[c].name = (name)? name : (char*) empty_string;

			name = dbcolsource(dbproc, c+1);
			metadata[c].source = (name)? name : (char*) empty_string;

			metadata[c].type = dbcoltype(dbproc, c+1);
			metadata[c].size = dbcollen(dbproc, c+1);
			assert(metadata[c].size != -1); /* -1 means indicates an out-of-range request*/

#if 0
			fprintf(stderr, "%6d  %30s  %30s  %15s  %6d  %6d  \n", 
				c+1, metadata[c].name, metadata[c].source, dbprtype(metadata[c].type), 
				metadata[c].size,  dbvarylen(dbproc, c+1));
#endif 
			/* 
			 * Build the column header format string, based on the column width. 
			 * This is just one solution to the question, "How wide should my columns be when I print them out?"
			 */
			width = get_printable_size(metadata[c].type, metadata[c].size);
			if (width < strlen(metadata[c].name))
				width = strlen(metadata[c].name);
				
			ret = set_format_string(&metadata[c], (c+1 < ncols)? "  " : "\n");
			if (ret <= 0) {
				fprintf(stderr, "%s:%d: asprintf(), column %d failed\n", options.appname, __LINE__, c+1);
				return -1;
			}

			/* 
			 * Bind the column to our variable.
			 * We bind everything to strings, because we want db-lib to convert everything to strings for us.
			 * If you're performing calculations on the data in your application, you'd bind the numeric data
			 * to C integers and floats, etc. instead. 
			 * 
			 * It is not necessary to bind to every column returned by the query.  
			 * Data in unbound columns are simply never copied to the user's buffers and are thus 
			 * inaccesible to the application.  
			 */

			data[c].buffer = calloc(1, metadata[c].size);
			assert(data[c].buffer);

			erc = dbbind(dbproc, c+1, STRINGBIND, -1, (BYTE *) data[c].buffer);
			if (erc == FAIL) {
				fprintf(stderr, "%s:%d: dbbind(), column %d failed\n", options.appname, __LINE__, c+1);
				return -1;
			}

			erc = dbnullbind(dbproc, c+1, &data[c].status);
			if (erc == FAIL) {
				fprintf(stderr, "%s:%d: dbnullbind(), column %d failed\n", options.appname, __LINE__, c+1);
				return -1;
			}
		}

		/* 
		 * Print the data to stdout.  
		 */
		for (;(row_code = dbnextrow(dbproc)) != NO_MORE_ROWS; nrows++) {
			switch (row_code) {
			case REG_ROW:
				for (c=0; c < ncols; c++) {
					switch (data[c].status) { /* handle nulls */
					case -1: 	/* is null */
						fprintf(stderr, "defncopy: error: unexpected NULL row in SQL text\n");
						break;
					case 0:	/* OK */
					default:	/* >1 is datlen when buffer is too small */
						fprintf(stdout, "%s", data[c].buffer);
						break;
					}
				}
				break;
				
			case BUF_FULL:
			default:
				fprintf(stderr, "defncopy: error: expected REG_ROW (%d), got %d instead\n", REG_ROW, row_code);
				assert(row_code == REG_ROW);
				break;
			} /* row_code */

		} /* wend dbnextrow */
		fprintf(stdout, "\n");

	} /* wend dbresults */
	return nrows;
}

static int
get_printable_size(int type, int size)	/* adapted from src/dblib/dblib.c */
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
		return 26;	/* FIX ME */
	case SYBDATETIME4:
		return 26;	/* FIX ME */
#if 0	/* seems not to be exported to sybdb.h */
	case SYBBITN:
#endif
	case SYBBIT:
		return 1;
		/* FIX ME -- not all types present */
	default:
		return 0;
	}

}

/** 
 * Build the column header format string, based on the column width. 
 * This is just one solution to the question, "How wide should my columns be when I print them out?"
 */
#define is_character_data(x)   (x==SYBTEXT || x==SYBCHAR || x==SYBVARCHAR)
static int
set_format_string(struct METADATA * meta, const char separator[])
{
	int width, ret;
	const char *size_and_width;
	assert(meta);

	/* right justify numbers, left justify strings */
	size_and_width = is_character_data(meta->type)? "%%-%d.%ds%s" : "%%%d.%ds%s";
	
	width = get_printable_size(meta->type, meta->size);
	if (width < strlen(meta->name))
		width = strlen(meta->name);

	ret = asprintf(&meta->format_string, size_and_width, width, width, separator);
		       
	return ret;
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
	int ch;

	extern char *optarg;
	extern int optind;

	assert(options && argv);
	
	options->appname = tds_basename(argv[0]);
	
	login = dblogin();
	
	if (!login) {
		fprintf(stderr, "%s: unable to allocate login structure\n", options->appname);
		exit(1);
	}
	
	DBSETLAPP(login, options->appname);
	
	if (-1 == gethostname(options->hostname, sizeof(options->hostname))) {
		perror("unable to get hostname");
	} else {
		DBSETLHOST(login, options->hostname);
	}

	while ((ch = getopt(argc, argv, "U:P:S:d:D:i:o:v")) != -1) {
		switch (ch) {
		case 'U':
			DBSETLUSER(login, optarg);
			break;
		case 'P':
			DBSETLPWD(login, optarg);
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
			printf("%s\n\n%s", software_version, 
				"Copyright (C) 2004  James K. Lowden\n"
				"This program  is free software; you can redistribute it and/or\n"
				"modify it under the terms of the GNU General Public\n"
				"License as published by the Free Software Foundation\n");
				exit(1);
			break;
		case '?':
		default:
			usage(options->appname);
			exit(1);
		}
	}

	if (!options->servername) {
		usage(options->appname);
		exit(1);
	}

	options->optind = optind;
	
	return login;
}

static int
err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{

	if (dberr) {
		fprintf(stderr, "%s: Msg %d, Level %d\n", options.appname, dberr, severity);
		fprintf(stderr, "%s\n\n", dberrstr);
	}

	else {
		fprintf(stderr, "%s: DB-LIBRARY error:\n\t", options.appname);
		fprintf(stderr, "%s\n", dberrstr);
	}

	return INT_CANCEL;
}

static int
msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
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
		fprintf(stdout, "USE %s\nGO\n\n", dbname);
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

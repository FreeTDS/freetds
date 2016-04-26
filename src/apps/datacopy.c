/* datacopy - Program to move database table between servers
 * Copyright (C) 2004-2005  Bill Thompson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <freetds/time.h>
#include <freetds/sysdep_private.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#include <sybfront.h>
#include <sybdb.h>

#include "replacements.h"

typedef struct
{
	char *user;
	char *pass;
	char *server;
	char *db;
	char *dbobject;
} OBJECTINFO;

typedef struct pd
{
	int batchsize;
	int packetsize;
	OBJECTINFO src, dest;
	char *owner;
	int textsize;
	int tflag;
	int aflag;
	int cflag;
	int Sflag;
	int Dflag;
	int bflag;
	int pflag;
	int Eflag;
	int vflag;
} BCPPARAMDATA;

static void pusage(void);
static int process_parameters(int, char **, struct pd *);
static int login_to_databases(const BCPPARAMDATA * pdata, DBPROCESS ** dbsrc, DBPROCESS ** dbdest);
static int create_target_table(char *sobjname, char *owner, char *dobjname, DBPROCESS * dbsrc, DBPROCESS * dbdest);
static int check_table_structures(char *sobjname, char *dobjname, DBPROCESS * dbsrc, DBPROCESS * dbdest);
static int transfer_data(const BCPPARAMDATA * params, DBPROCESS * dbsrc, DBPROCESS * dbdest);
static RETCODE set_textsize(DBPROCESS *dbproc, int textsize);

static int err_handler(DBPROCESS *, int, int, int, char *, char *);
static int msg_handler(DBPROCESS *, DBINT, int, int, char *, char *, char *, int);

int tdsdump_open(const char *filename);

int
main(int argc, char **argv)
{
	BCPPARAMDATA params;

	DBPROCESS *dbsrc;
	DBPROCESS *dbtarget;

	setlocale(LC_ALL, "");

	memset(&params, '\0', sizeof(params));

	if (process_parameters(argc, argv, &params) == FALSE) {
		pusage();
		return 1;
	}

	if (login_to_databases(&params, &dbsrc, &dbtarget) == FALSE)
		return 1;

	if (set_textsize(dbtarget, params.textsize) != SUCCEED
	    || set_textsize(dbsrc, params.textsize) != SUCCEED)
		return 1;

	if (params.cflag) {
		if (create_target_table(params.src.dbobject, params.owner, params.dest.dbobject, dbsrc, dbtarget) == FALSE) {
			printf("datacopy: could not create target table %s.%s . terminating\n", params.owner, params.dest.dbobject);
			dbclose(dbsrc);
			dbclose(dbtarget);
			return 1;
		}
	}

	if (check_table_structures(params.src.dbobject, params.dest.dbobject, dbsrc, dbtarget) == FALSE) {
		printf("datacopy: table structures do not match. terminating\n");
		dbclose(dbsrc);
		dbclose(dbtarget);
		return 1;
	}

	if (transfer_data(&params, dbsrc, dbtarget) == FALSE) {
		printf("datacopy: table copy failed.\n");
		printf("           the data may have been partially copied into the target database \n");
		dbclose(dbsrc);
		dbclose(dbtarget);
		return 1;
	}


	dbclose(dbsrc);
	dbclose(dbtarget);

	return 0;
}

static char *
gets_alloc(void)
{
	char reply[256];
	char *p = NULL;

	if (fgets(reply, sizeof(reply), stdin) != NULL) {
		p = strchr(reply, '\n');
		if (p)
			*p = 0;
		p = strdup(reply);
	}
	memset(reply, 0, sizeof(reply));
	return p;
}

static int
process_objectinfo(OBJECTINFO *oi, char *arg, const char *prompt)
{
	char *tok;

	tok = strsep(&arg, "/");
	if (!tok)
		return FALSE;
	oi->server = strdup(tok);

	tok = strsep(&arg, "/");
	if (!tok)
		return FALSE;
	oi->user = strdup(tok);

	tok = strsep(&arg, "/");
	if (!tok)
		return FALSE;
	if (strcmp(tok,"-") == 0) {
		printf("%s", prompt);
		oi->pass = gets_alloc();
	} else {
		oi->pass = strdup(tok);
		memset(tok, '*', strlen(tok));
	}

	tok = strsep(&arg, "/");
	if (!tok)
		return FALSE;
	oi->db = strdup(tok);

	tok = strsep(&arg, "/");
	if (!tok)
		return FALSE;
	oi->dbobject = strdup(tok);

	return TRUE;
}

static int
process_parameters(int argc, char **argv, BCPPARAMDATA * pdata)
{
	int opt;

	/* set some defaults */

	pdata->textsize = -1;
	pdata->batchsize = 1000;

	/* get the rest of the arguments */

	while ((opt = getopt(argc, argv, "b:p:tac:dS:D:T:Ev")) != -1) {
		switch (opt) {
		case 'b':
			pdata->bflag++;
			pdata->batchsize = atoi(optarg);
			break;
		case 'p':
			pdata->pflag++;
			pdata->packetsize = atoi(optarg);
			break;
		case 't':
			pdata->tflag++;
			break;
		case 'a':
			pdata->aflag++;
			break;
		case 'c':
			pdata->cflag++;
			if (optarg[0] == '-') {
				fprintf(stderr, "Invalid owner specified.\n");
				return FALSE;
			}
			pdata->owner = strdup(optarg);
			break;
		case 'd':
			tdsdump_open(NULL);
			break;
		case 'S':
			pdata->Sflag++;
			if (process_objectinfo(&pdata->src, optarg, "Enter Source Password: ") == FALSE)
				return FALSE;
			break;
		case 'D':
			pdata->Dflag++;
			if (process_objectinfo(&pdata->dest, optarg, "Enter Destination Password: ") == FALSE)
				return FALSE;
			break;
		case 'T':
			pdata->textsize = opt = atoi(optarg);
			if (opt < 0 || opt > 0x7fffffff) {
				fprintf(stderr, "Invalid textsize specified.\n");
				return FALSE;
			}
			break;
		case 'E':
			pdata->Eflag++;
			break;
		case 'v':
			pdata->vflag++;
			break;
		default:
			return FALSE;
		}
	}
	/* one of these must be specified */

	if ((pdata->tflag + pdata->aflag + pdata->cflag) != 1) {
		fprintf(stderr, "one (and only one) of -t, -a or -c must be specified\n");
		return FALSE;
	}

	if (!pdata->Sflag) {

		printf("\nNo [-S]ource information supplied.\n\n");
		printf("Enter Server   : ");
		pdata->src.server = gets_alloc();

		printf("Enter Login    : ");
		pdata->src.user = gets_alloc();

		printf("Enter Password : ");
		pdata->src.pass = gets_alloc();

		printf("Enter Database : ");
		pdata->src.db = gets_alloc();

		printf("Enter Table    : ");
		pdata->src.dbobject = gets_alloc();
	}

	if (!pdata->Dflag) {

		printf("\nNo [-D]estination information supplied.\n\n");
		printf("Enter Server   : ");
		pdata->dest.server = gets_alloc();

		printf("Enter Login    : ");
		pdata->dest.user = gets_alloc();

		printf("Enter Password : ");
		pdata->dest.pass = gets_alloc();

		printf("Enter Database : ");
		pdata->dest.db = gets_alloc();

		printf("Enter Table    : ");
		pdata->dest.dbobject = gets_alloc();
	}

	return TRUE;

}

static int
login_to_databases(const BCPPARAMDATA * pdata, DBPROCESS ** dbsrc, DBPROCESS ** dbdest)
{
	int result = FALSE;
	LOGINREC *slogin = NULL;
	LOGINREC *dlogin = NULL;

	/* Initialize DB-Library. */

	if (dbinit() == FAIL)
		return FALSE;

	/*
	 * Install the user-supplied error-handling and message-handling
	 * routines. They are defined at the bottom of this source file.
	 */

	dberrhandle(err_handler);
	dbmsghandle(msg_handler);

	/*
	 * Allocate and initialize the LOGINREC structure to be used
	 * to open a connection to SQL Server.
	 */

	slogin = dblogin();

	if (pdata->src.user)
		DBSETLUSER(slogin, pdata->src.user);
	if (pdata->src.pass)
		DBSETLPWD(slogin, pdata->src.pass);
	DBSETLAPP(slogin, "Migrate Data");

	/* if packet size specified, set in login record */

	if (pdata->pflag && pdata->packetsize > 0) {
		DBSETLPACKET(slogin, pdata->packetsize);
	}

	/*
	 * Get a connection to the database.
	 */

	if ((*dbsrc = dbopen(slogin, pdata->src.server)) == (DBPROCESS *) NULL) {
		fprintf(stderr, "Can't connect to source server.\n");
		goto cleanup;
	}

	if (dbuse(*dbsrc, pdata->src.db) == FAIL) {
		fprintf(stderr, "Can't change database to %s .\n", pdata->src.db);
		goto cleanup;
	}

	dlogin = dblogin();

	if (pdata->dest.user)
		DBSETLUSER(dlogin, pdata->dest.user);
	if (pdata->dest.pass)
		DBSETLPWD(dlogin, pdata->dest.pass);
	DBSETLAPP(dlogin, "Migrate Data");

	/* Enable bulk copy for this connection. */

	BCP_SETL(dlogin, TRUE);

	/* if packet size specified, set in login record */

	if (pdata->pflag && pdata->packetsize > 0) {
		DBSETLPACKET(dlogin, pdata->packetsize);
	}

	/*
	 * Get a connection to the database.
	 */

	if ((*dbdest = dbopen(dlogin, pdata->dest.server)) == (DBPROCESS *) NULL) {
		fprintf(stderr, "Can't connect to destination server.\n");
		goto cleanup;
	}

	if (dbuse(*dbdest, pdata->dest.db) == FAIL) {
		fprintf(stderr, "Can't change database to %s .\n", pdata->dest.db);
		goto cleanup;
	}

	result = TRUE;

cleanup:
	dbloginfree(slogin);
	dbloginfree(dlogin);
	return result;
}

static int
create_target_table(char *sobjname, char *owner, char *dobjname, DBPROCESS * dbsrc, DBPROCESS * dbdest)
{
	char ls_command[2048];
	int i;
	const char *sep;

	DBINT num_cols;
	DBCOL2 colinfo;

	sprintf(ls_command, "SET FMTONLY ON select * from %s SET FMTONLY OFF", sobjname);

	if (dbcmd(dbsrc, ls_command) == FAIL) {
		printf("dbcmd failed\n");
		return FALSE;
	}

	if (dbsqlexec(dbsrc) == FAIL) {
		printf("table %s not found on SOURCE\n", sobjname);
		return FALSE;
	}

	while (dbresults(dbsrc) != NO_MORE_RESULTS)
		continue;

	sprintf(ls_command, "CREATE TABLE %s%s%s ", owner, owner[0] ? "." : "", dobjname);

	num_cols = dbnumcols(dbsrc);

	sep = "( ";
	for (i = 1; i <= num_cols; i++) {

		colinfo.SizeOfStruct = sizeof(colinfo);
		if (dbtablecolinfo(dbsrc, i, (DBCOL *) &colinfo) != SUCCEED)
			return FALSE;

		strlcat(ls_command, sep, sizeof(ls_command));
		sep = ", ";

		strlcat(ls_command, colinfo.Name, sizeof(ls_command));
		strlcat(ls_command, " ", sizeof(ls_command));

		strlcat(ls_command, colinfo.ServerTypeDeclaration, sizeof(ls_command));

		if (colinfo.Null == TRUE) {
			strlcat(ls_command, " NULL", sizeof(ls_command));
		} else {
			strlcat(ls_command, " NOT NULL", sizeof(ls_command));
		}
	}
	if (strlcat(ls_command, " )", sizeof(ls_command)) >= sizeof(ls_command)) {
		fprintf(stderr, "Buffer overflow building command to create table\n");
		return FALSE;
	}

	if (dbcmd(dbdest, ls_command) == FAIL) {
		printf("dbcmd failed\n");
		return FALSE;
	}

	if (dbsqlexec(dbdest) == FAIL) {
		printf("create table on DESTINATION failed\n");
		return FALSE;
	}

	while (NO_MORE_RESULTS != dbresults(dbdest))
		continue;

	return TRUE;
}

static RETCODE
set_textsize(DBPROCESS *dbproc, int textsize)
{
	char buf[32];

	if (textsize < 0)
		return SUCCEED;

	sprintf(buf, "%d", textsize);
	if (dbsetopt(dbproc, DBTEXTSIZE, buf, -1) == FAIL) {
		fprintf(stderr, "dbsetopt failed\n");
		return FAIL;
	}

	return SUCCEED;
}

static int
check_table_structures(char *sobjname, char *dobjname, DBPROCESS * dbsrc, DBPROCESS * dbdest)
{
	char ls_command[256];
	int i, ret;

	DBINT src_numcols = 0;
	DBINT dest_numcols = 0;

	DBINT src_coltype, dest_coltype;
	DBINT src_collen, dest_collen;


	sprintf(ls_command, "SET FMTONLY ON select * from %s SET FMTONLY OFF", sobjname);

	if (dbcmd(dbsrc, ls_command) == FAIL) {
		printf("dbcmd failed\n");
		return FALSE;
	}

	if (dbsqlexec(dbsrc) == FAIL) {
		printf("table %s not found on SOURCE\n", sobjname);
		return FALSE;
	}

	while ((ret=dbresults(dbsrc)) == SUCCEED)
		src_numcols = dbnumcols(dbsrc);
	if (ret != NO_MORE_RESULTS) {
		printf("Error in dbresults\n");
		return FALSE;
	}
	if (0 == src_numcols) {
		printf("Error in dbnumcols 1\n");
		return FALSE;
	}

	sprintf(ls_command, "SET FMTONLY ON select * from %s SET FMTONLY OFF", dobjname);

	if (dbcmd(dbdest, ls_command) == FAIL) {
		printf("dbcmd failed\n");
		return FALSE;
	}

	if (dbsqlexec(dbdest) == FAIL) {
		printf("table %s not found on DEST\n", sobjname);
		return FALSE;
	}

	while ((ret=dbresults(dbdest)) == SUCCEED)
		dest_numcols = dbnumcols(dbdest);
	if (ret != NO_MORE_RESULTS) {
		printf("Error in dbresults\n");
		return FALSE;
	}
	if (0 == dest_numcols) {
		printf("Error in dbnumcols 2\n");
		return FALSE;
	}

	if (src_numcols != dest_numcols) {
		printf("number of columns do not match. source : %d , dest: %d\n", src_numcols, dest_numcols);
		return FALSE;
	}

	for (i = 1; i <= src_numcols; i++) {

		src_coltype = dbcoltype(dbsrc, i);
		src_collen = dbcollen(dbsrc, i);
		dest_coltype = dbcoltype(dbdest, i);
		dest_collen = dbcollen(dbdest, i);

		if ((src_coltype == SYBNUMERIC && dest_coltype == SYBNUMERIC) ||
		    (src_coltype == SYBDECIMAL && dest_coltype == SYBDECIMAL)
			) {
			continue;
		}

		if (src_coltype != dest_coltype || src_collen != dest_collen) {
			printf("COLUMN TYPE MISMATCH: column %d\n", i);
			printf("source: type %d, length %d\n", src_coltype, src_collen);
			printf("dest  : type %d, length %d\n", dest_coltype, dest_collen);
			return FALSE;
		}
	}
	return TRUE;
}

static int
transfer_data(const BCPPARAMDATA * params, DBPROCESS * dbsrc, DBPROCESS * dbdest)
{
	char ls_command[256];
	int col;

	DBINT src_numcols = 0;

	typedef struct migcoldata
	{
		DBINT coltype;
	} MIGCOLDATA;

	MIGCOLDATA *srcdata;

	DBINT rows_read = 0;
	DBINT rows_sent = 0;
	DBINT rows_done = 0;
	DBINT ret;

	struct timeval start_time;
	struct timeval end_time;
	double elapsed_time;

	DBCOL2 colinfo;
	BOOL identity_column_exists = FALSE;

	if (params->vflag) {
		printf("\nStarting copy...\n");
	}

	if (params->tflag) {

		sprintf(ls_command, "truncate table %s", params->dest.dbobject);

		if (dbcmd(dbdest, ls_command) == FAIL) {
			printf("dbcmd failed\n");
			return FALSE;
		}

		if (dbsqlexec(dbdest) == FAIL) {
			printf("dbsqlexec failed\n");
			return FALSE;
		}

		if (dbresults(dbdest) == FAIL) {
			printf("Error in dbresults\n");
			return FALSE;
		}
	}


	sprintf(ls_command, "select * from %s", params->src.dbobject);

	if (dbcmd(dbsrc, ls_command) == FAIL) {
		printf("dbcmd failed\n");
		return FALSE;
	}

	if (dbsqlexec(dbsrc) == FAIL) {
		printf("dbsqlexec failed\n");
		return FALSE;
	}

	if (NO_MORE_RESULTS != dbresults(dbsrc)) {
		if (0 == (src_numcols = dbnumcols(dbsrc))) {
			printf("Error in dbnumcols\n");
			return FALSE;
		}
	}



	if (bcp_init(dbdest, params->dest.dbobject, (char *) NULL, (char *) NULL, DB_IN) == FAIL) {
		printf("Error in bcp_init\n");
		return FALSE;
	}

	srcdata = (MIGCOLDATA *) calloc(sizeof(MIGCOLDATA), src_numcols);

	for (col = 0; col < src_numcols; col++) {

		/* Find out if there is an identity column. */
		colinfo.SizeOfStruct = sizeof(colinfo);

		if (dbtablecolinfo(dbsrc, col+1, (DBCOL *) &colinfo) != SUCCEED)
			return FALSE;
		if (colinfo.Identity)
			identity_column_exists = TRUE;

		srcdata[col].coltype = dbcoltype(dbsrc, col + 1);

		switch (srcdata[col].coltype) {
		case SYBBIT:
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBINT8:
		case SYBFLT8:
		case SYBREAL:
		case SYBMONEY:
		case SYBMONEY4:
		case SYBDATETIME:
		case SYBDATETIME4:
		case SYBTIME:
		case SYBDATE:
		case SYBBIGTIME:
		case SYBBIGDATETIME:
		case SYBCHAR:
		case SYBTEXT:
		case SYBBINARY:
		case SYBIMAGE:
		case SYBNUMERIC:
		case SYBDECIMAL:
			break;
		default:
			fprintf(stderr, "Type %d not handled by datacopy\n", srcdata[col].coltype);
			exit(1);
		}
	}

	/* Take appropriate action if there's an identity column and we've been asked to preserve identity values. */
	if (params->Eflag && identity_column_exists)
		bcp_control(dbdest, BCPKEEPIDENTITY, 1);

	gettimeofday(&start_time, 0);

	while (dbnextrow(dbsrc) != NO_MORE_ROWS) {
		rows_read++;
		for (col = 0; col < src_numcols; col++) {
			BYTE *data = dbdata(dbsrc, col + 1);

			switch (srcdata[col].coltype) {
			case SYBBIT:
			case SYBINT1:
			case SYBINT2:
			case SYBINT4:
			case SYBINT8:
			case SYBFLT8:
			case SYBREAL:
			case SYBDATETIME:
			case SYBDATETIME4:
			case SYBTIME:
			case SYBDATE:
			case SYBBIGTIME:
			case SYBBIGDATETIME:
			case SYBMONEY:
			case SYBMONEY4:
			case SYBCHAR:
			case SYBTEXT:
			case SYBBINARY:
			case SYBIMAGE:
			case SYBNUMERIC:
			case SYBDECIMAL:
				bcp_colptr(dbdest, data, col + 1);
				if (data == NULL) {	/* NULL data retrieved from source */
					bcp_collen(dbdest, 0, col + 1);
				} else {
					bcp_collen(dbdest, dbdatlen(dbsrc, col + 1), col + 1);
				}
				break;
			default:
				fprintf(stderr, "Type %d not handled by datacopy\n", srcdata[col].coltype);
				exit(1);
			}
		}
		if (bcp_sendrow(dbdest) == FAIL) {
			fprintf(stderr, "bcp_sendrow failed.  \n");
			return FALSE;
		} else {
			rows_sent++;
			if (rows_sent == params->batchsize) {
				ret = bcp_batch(dbdest);
				if (ret == -1) {
					printf("bcp_batch error\n");
					return FALSE;
				} else {
					rows_done += ret;
					printf("%d rows successfully copied (total %d)\n", ret, rows_done);
					rows_sent = 0;
				}
			}
		}
	}

	if (rows_read) {
		ret = bcp_done(dbdest);
		if (ret == -1) {
			fprintf(stderr, "bcp_done failed.  \n");
			return FALSE;
		} else {
			rows_done += ret;
		}
	}

	gettimeofday(&end_time, 0);


	elapsed_time = (double) (end_time.tv_sec - start_time.tv_sec) +
		((double) (end_time.tv_usec - start_time.tv_usec) / 1000000.00);

	if (params->vflag) {
		printf("\n");
		printf("rows read            : %d\n", rows_read);
		printf("rows written         : %d\n", rows_done);
		printf("elapsed time (secs)  : %f\n", elapsed_time);
		printf("rows per second      : %f\n", rows_done / elapsed_time);
	}

	return TRUE;


}

static void
pusage(void)
{
	fprintf(stderr, "usage: datacopy [-t | -a | -c owner] [-b batchsize] [-p packetsize] [-T textsize] [-v] [-d] [-E]\n");
	fprintf(stderr, "       [-S server/username/password/database/table]\n");
	fprintf(stderr, "       [-D server/username/password/database/table]\n");
	fprintf(stderr, "       -t : truncate target table before loading data\n");
	fprintf(stderr, "       -a : append data to target table\n");
	fprintf(stderr, "       -c : create table owner.table before loading data\n");
	fprintf(stderr, "       -b : alter the number of records in each bcp batch\n");
	fprintf(stderr, "       (larger batch size = faster)\n");
	fprintf(stderr, "       -p : alter the default TDS packet size from the default\n");
	fprintf(stderr, "       (larger packet size = faster)\n");
	fprintf(stderr, "       -T : Text and image size\n");
	fprintf(stderr, "       -E : keep identity values\n");
	fprintf(stderr, "       -v : produce verbose output (timings etc.)\n");
	fprintf(stderr, "       -d : produce TDS DUMP log (serious debug only!)\n");
}

static int
err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	if (dberr) {
		fprintf(stderr, "Msg %d, Level %d\n", dberr, severity);
		fprintf(stderr, "%s\n\n", dberrstr);
	} else {
		fprintf(stderr, "DB-LIBRARY error:\n\t");
		fprintf(stderr, "%s\n", dberrstr);
	}

	return INT_CANCEL;
}

static int
msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
{
	/*
	 * If it's a database change message, we'll ignore it.
	 * Also ignore language change message.
	 */
	if (msgno == 5701 || msgno == 5703)
		return 0;

	printf("Msg %ld, Level %d, State %d\n", (long int) msgno, severity, msgstate);

	if (strlen(srvname) > 0)
		printf("Server '%s', ", srvname);
	if (strlen(procname) > 0)
		printf("Procedure '%s', ", procname);
	if (line > 0)
		printf("Line %d", line);

	printf("\n\t%s\n", msgtext);

	return 0;
}

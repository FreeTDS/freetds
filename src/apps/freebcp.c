/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

#include <stdio.h>
#include <ctype.h>

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

#include "tds.h"
#include <sybfront.h>
#include <sybdb.h>
#include "dblib.h"
#include "freebcp.h"

static char software_version[] = "$Id: freebcp.c,v 1.37 2005-02-03 09:41:29 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

void pusage(void);
int process_parameters(int, char **, struct pd *);
static void unescape(char arg[]);
int login_to_database(struct pd *, DBPROCESS **);

int file_character(PARAMDATA * pdata, DBPROCESS * dbproc, DBINT dir);
int file_native(PARAMDATA * pdata, DBPROCESS * dbproc, DBINT dir);
int file_formatted(PARAMDATA * pdata, DBPROCESS * dbproc, DBINT dir);

int err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
int msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname,
		int line);

int
main(int argc, char **argv)
{
	PARAMDATA params;
	DBPROCESS *dbproc;
	int ok = FALSE;

	memset(&params, '\0', sizeof(PARAMDATA));

	params.textsize = 4096;	/* our default text size is 4K */

	if (process_parameters(argc, argv, &params) == FALSE) {
		exit(1);
	}
	if (getenv("FREEBCP")) {
		fprintf(stderr, "User name: \"%s\"\n", params.user);
	}


	if (login_to_database(&params, &dbproc) == FALSE) {
		exit(1);
	}

	if (dbfcmd(dbproc, "set textsize %d", params.textsize) == FAIL) {
		printf("dbfcmd failed\n");
		return FALSE;
	}

	if (dbsqlexec(dbproc) == FAIL) {
		printf("dbsqlexec failed\n");
		return FALSE;
	}

	while (NO_MORE_RESULTS != dbresults(dbproc));

	if (params.cflag) {	/* character format file */
		ok = file_character(&params, dbproc, params.direction);
	} else if (params.nflag) {	/* native format file    */
		ok = file_native(&params, dbproc, params.direction);
	} else if (params.fflag) {	/* formatted file        */
		ok = file_formatted(&params, dbproc, params.direction);
	} else {
		ok = FALSE;
	}

	exit((ok == TRUE) ? 0 : 1);

	return 0;
}


static void unescape(char arg[])
{
	char *p = arg;
	char escaped = '1'; /* any digit will do for an initial value */
	while ((p = strchr(p, '\\')) != NULL) {
	
		switch (p[1]) {
		case '0':
			/* FIXME we use strlen() of field/row terminators, which obviously won't work here */
			fprintf(stderr, "freebcp, line %d: NULL terminators ('\\0') not yet supported.\n", __LINE__);
			escaped = '\0';
			break;
		case 't':
			escaped = '\t';
			break;
		case 'r':
			escaped = '\r';
			break;
		case 'n':
			escaped = '\n';
			break;
		case '\\':
			escaped = '\\';
			break;
		default:
			break;
		}
			
		/* Overwrite the backslash with the intended character, and shift everything down one */
		if (!isdigit((unsigned char) escaped)) {
			*p++ = escaped;
			memmove(p, p+1, 1 + strlen(p+1));
		}
	}
}

int
process_parameters(int argc, char **argv, PARAMDATA *pdata)
{
	extern char *optarg;
	extern int optind;
	extern int optopt;
	
	int ch;

	if (argc < 6) {
		if (argc == 1) {
			pusage();
			return (FALSE);
		}
		fprintf(stderr, "A minimum of 6 parameters must be supplied.\n");
		return (FALSE);
	}

	/* 
	 * Set some defaults and read the table, file, and direction arguments.  
	 */
	pdata->firstrow = 0;
	pdata->lastrow = 0;
	pdata->batchsize = 1000;
	pdata->maxerrors = 10;

	/* argument 1 - the database object */
	pdata->dbobject = (char *) malloc(strlen(argv[1]) + 1);
	if (pdata->dbobject != (char *) NULL)
		strcpy(pdata->dbobject, argv[1]);

	/* argument 2 - the direction */
	strcpy(pdata->dbdirection, argv[2]);

	if (strcmp(pdata->dbdirection, "in") == 0) {
		pdata->direction = DB_IN;
	} else if (strcmp(pdata->dbdirection, "out") == 0) {
		pdata->direction = DB_OUT;
	} else if (strcmp(pdata->dbdirection, "queryout") == 0) {
		pdata->direction = DB_QUERYOUT;
	} else {
		fprintf(stderr, "Copy direction must be either 'in', 'out' or 'queryout'.\n");
		return (FALSE);
	}

	/* argument 3 - the datafile name */
	strcpy(pdata->hostfilename, argv[3]);

	/* 
	 * Get the rest of the arguments 
	 */
	optind = 4; /* start processing options after table, direction, & filename */
	while ((ch = getopt(argc, argv, "m:f:e:F:L:b:t:r:U:P:I:S:h:T:A:ncEdvV")) != -1) {
		switch (ch) {
		case 'v':
		case 'V':
			printf("freebcp version %s\n", software_version);
			return FALSE;
			break;
		case 'm':
			pdata->mflag++;
			pdata->maxerrors = atoi(optarg);
			break;
		case 'f':
			pdata->fflag++;
			strcpy(pdata->formatfile, optarg);
			break;
		case 'e':
			pdata->eflag++;
			pdata->errorfile = strdup(optarg);
			break;
		case 'F':
			pdata->Fflag++;
			pdata->firstrow = atoi(optarg);
			break;
		case 'L':
			pdata->Lflag++;
			pdata->lastrow = atoi(optarg);
			break;
		case 'b':
			pdata->bflag++;
			pdata->batchsize = atoi(optarg);
			break;
		case 'n':
			pdata->nflag++;
			break;
		case 'c':
			pdata->cflag++;
			break;
		case 'E':
			pdata->Eflag++;
			break;
		case 'd':
			tdsdump_open(NULL);
			break;
		case 't':
			pdata->tflag++;
			pdata->fieldterm = strdup(optarg);
			unescape(pdata->fieldterm);
			break;
		case 'r':
			pdata->rflag++;
			pdata->rowterm = strdup(optarg);
			unescape(pdata->rowterm);
			break;
		case 'U':
			pdata->Uflag++;
			pdata->user = strdup(optarg);
			break;
		case 'P':
			pdata->Pflag++;
			pdata->pass = strdup(optarg);
			break;
		case 'I':
			pdata->Iflag++;
			strcpy(pdata->interfacesfile, optarg);
			break;
		case 'S':
			pdata->Sflag++;
			pdata->server = strdup(optarg);
			break;
		case 'h':
			pdata->hint = strdup(optarg);
			break;
		case 'T':
			pdata->Tflag++;
			pdata->textsize = atoi(optarg);
			break;
		case 'A':
			pdata->Aflag++;
			pdata->packetsize = atoi(optarg);
			break;
		case '?':
		default:
			pusage();
			return (FALSE);
		}
	}

	/* 
	 * Check for required/disallowed option combinations 
	 */
	 
	/* these must be specified */
	if (!pdata->Uflag || !pdata->Pflag || !pdata->Sflag) {
		fprintf(stderr, "All 3 options -U, -P, -S must be supplied.\n");
		return (FALSE);
	}

	/* only one of these can be specified */
	if (pdata->cflag + pdata->nflag + pdata->fflag != 1) {
		fprintf(stderr, "Exactly one of options -c, -n, -f must be supplied.\n");
		return (FALSE);
	}

	/* character mode file: Fill in some default values*/
	if (pdata->cflag) {

		if (!pdata->tflag || !pdata->fieldterm) {	/* field terminator not specified */
			pdata->fieldterm = "\t";
		}
		if (!pdata->rflag || !pdata->rowterm) {		/* row terminator not specified */
			pdata->rowterm =  "\n";
		}
	}

	return (TRUE);

}

int
login_to_database(PARAMDATA * pdata, DBPROCESS ** pdbproc)
{
	LOGINREC *login;

	/* Initialize DB-Library. */

	if (dbinit() == FAIL)
		return (FALSE);

	/* Install the user-supplied error-handling and message-handling
	 * routines. They are defined at the bottom of this source file.
	 */

	dberrhandle(err_handler);
	dbmsghandle(msg_handler);

	/* If the interfaces file was specified explicitly, set it. */
	if (pdata->interfacesfile != NULL)
		dbsetifile(pdata->interfacesfile);

	/* 
	 * Allocate and initialize the LOGINREC structure to be used 
	 * to open a connection to SQL Server.
	 */

	login = dblogin();

	DBSETLUSER(login, pdata->user);
	DBSETLPWD(login, pdata->pass);
	DBSETLAPP(login, "FreeBCP");

	/* if packet size specified, set in login record */

	if (pdata->Aflag && pdata->packetsize > 0) {
		DBSETLPACKET(login, pdata->packetsize);
	}

	/* Enable bulk copy for this connection. */

	BCP_SETL(login, TRUE);

	/*
	 * ** Get a connection to the database.
	 */

	if ((*pdbproc = dbopen(login, pdata->server)) == (DBPROCESS *) NULL) {
		fprintf(stderr, "Can't connect to server \"%s\".\n", pdata->server);
		return (FALSE);
	}

	/* set hint if any */
	if (pdata->hint) {
		int erc = bcp_options(*pdbproc, BCPHINTS, (BYTE *) pdata->hint, strlen(pdata->hint));

		if (erc != SUCCEED)
			fprintf(stderr, "db-lib: Unable to set hint \"%s\"\n", pdata->hint);
		return FALSE;
	}

	return (TRUE);

}

int
file_character(PARAMDATA * pdata, DBPROCESS * dbproc, DBINT dir)
{
	DBINT li_rowsread = 0;
	int i;
	int li_numcols = 0;
	RETCODE ret_code = 0;

	if (dir == DB_QUERYOUT) {
		if (dbfcmd(dbproc, "SET FMTONLY ON %s SET FMTONLY OFF", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	} else {
		if (dbfcmd(dbproc, "SET FMTONLY ON select * from %s SET FMTONLY OFF", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	}

	if (dbsqlexec(dbproc) == FAIL) {
		printf("dbsqlexec failed\n");
		return FALSE;
	}

	while (NO_MORE_RESULTS != (ret_code = dbresults(dbproc))) {
		if (ret_code == SUCCEED && li_numcols == 0) {
			li_numcols = dbnumcols(dbproc);
		}
	}

	if (0 == li_numcols) {
		printf("Error in dbnumcols\n");
		return FALSE;
	}

	if (FAIL == bcp_init(dbproc, pdata->dbobject, pdata->hostfilename, pdata->errorfile, dir))
		return FALSE;

	if (pdata->Eflag) {

		dbproc->bcpinfo->identity_insert_on = 1;
	
		if (dbfcmd(dbproc, "set identity_insert %s on", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	
		if (dbsqlexec(dbproc) == FAIL) {
			printf("dbsqlexec failed\n");
			return FALSE;
		}

		while (NO_MORE_RESULTS != dbresults(dbproc));
	}

	dbproc->hostfileinfo->firstrow = pdata->firstrow;
	dbproc->hostfileinfo->lastrow = pdata->lastrow;
	dbproc->hostfileinfo->maxerrs = pdata->maxerrors;

	if (bcp_columns(dbproc, li_numcols) == FAIL) {
		printf("Error in bcp_columns.\n");
		return FALSE;
	}

	for (i = 1; i <= li_numcols - 1; i++) {
		if (bcp_colfmt(dbproc, i, SYBCHAR, 0, -1, (const BYTE *) pdata->fieldterm,
			       strlen(pdata->fieldterm), i) == FAIL) {
			printf("Error in bcp_colfmt col %d\n", i);
			return FALSE;
		}
	}

	if (bcp_colfmt(dbproc, li_numcols, SYBCHAR, 0, -1, (const BYTE *) pdata->rowterm,
		       strlen(pdata->rowterm), li_numcols) == FAIL) {
		printf("Error in bcp_colfmt col %d\n", li_numcols);
		return FALSE;
	}

	bcp_control(dbproc, BCPBATCH, pdata->batchsize);

	printf("\nStarting copy...\n\n");

	if (FAIL == bcp_exec(dbproc, &li_rowsread)) {
		fprintf(stderr, "bcp copy %s failed\n", (dir == DB_IN) ? "in" : "out");
		return FALSE;
	}

	printf("%d rows copied.\n", li_rowsread);

	return TRUE;
}

int
file_native(PARAMDATA * pdata, DBPROCESS * dbproc, DBINT dir)
{
	DBINT li_rowsread = 0;
	int i;
	int li_numcols = 0;
	int li_coltype;
	int li_collen;
	RETCODE ret_code = 0;

	if (dir == DB_QUERYOUT) {
		if (dbfcmd(dbproc, "SET FMTONLY ON %s SET FMTONLY OFF", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	} else {
		if (dbfcmd(dbproc, "SET FMTONLY ON select * from %s SET FMTONLY OFF", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	}

	if (dbsqlexec(dbproc) == FAIL) {
		printf("dbsqlexec failed\n");
		return FALSE;
	}

	while (NO_MORE_RESULTS != (ret_code = dbresults(dbproc))) {
		if (ret_code == SUCCEED && li_numcols == 0) {
			li_numcols = dbnumcols(dbproc);
		}
	}

	if (0 == li_numcols) {
		printf("Error in dbnumcols\n");
		return FALSE;
	}

	if (FAIL == bcp_init(dbproc, pdata->dbobject, pdata->hostfilename, pdata->errorfile, dir))
		return FALSE;

	if (pdata->Eflag) {

		dbproc->bcpinfo->identity_insert_on = 1;
	
		if (dbfcmd(dbproc, "set identity_insert %s on", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	
		if (dbsqlexec(dbproc) == FAIL) {
			printf("dbsqlexec failed\n");
			return FALSE;
		}

		while (NO_MORE_RESULTS != dbresults(dbproc));
	}

	dbproc->hostfileinfo->firstrow = pdata->firstrow;
	dbproc->hostfileinfo->lastrow = pdata->lastrow;
	dbproc->hostfileinfo->maxerrs = pdata->maxerrors;

	if (bcp_columns(dbproc, li_numcols) == FAIL) {
		printf("Error in bcp_columns.\n");
		return FALSE;
	}

	for (i = 1; i <= li_numcols; i++) {
		li_coltype = dbcoltype(dbproc, i);
		li_collen = dbcollen(dbproc, i);

		if (bcp_colfmt(dbproc, i, li_coltype, -1, -1, (BYTE *) NULL, -1, i) == FAIL) {
			printf("Error in bcp_colfmt col %d\n", i);
			return FALSE;
		}
	}

	printf("\nStarting copy...\n\n");


	if (FAIL == bcp_exec(dbproc, &li_rowsread)) {
		fprintf(stderr, "bcp copy %s failed\n", (dir == DB_IN) ? "in" : "out");
		return FALSE;
	}

	printf("%d rows copied.\n", li_rowsread);

	return TRUE;
}

int
file_formatted(PARAMDATA * pdata, DBPROCESS * dbproc, DBINT dir)
{

	int li_rowsread;

	if (FAIL == bcp_init(dbproc, pdata->dbobject, pdata->hostfilename, pdata->errorfile, dir))
		return FALSE;

	if (pdata->Eflag) {

		dbproc->bcpinfo->identity_insert_on = 1;
	
		if (dbfcmd(dbproc, "set identity_insert %s on", pdata->dbobject) == FAIL) {
			printf("dbfcmd failed\n");
			return FALSE;
		}
	
		if (dbsqlexec(dbproc) == FAIL) {
			printf("dbsqlexec failed\n");
			return FALSE;
		}

		while (NO_MORE_RESULTS != dbresults(dbproc));
	}

	dbproc->hostfileinfo->firstrow = pdata->firstrow;
	dbproc->hostfileinfo->lastrow = pdata->lastrow;
	dbproc->hostfileinfo->maxerrs = pdata->maxerrors;

	if (FAIL == bcp_readfmt(dbproc, pdata->formatfile))
		return FALSE;

	printf("\nStarting copy...\n\n");


	if (FAIL == bcp_exec(dbproc, &li_rowsread)) {
		fprintf(stderr, "bcp copy %s failed\n", (dir == DB_IN) ? "in" : "out");
		return FALSE;
	}

	printf("%d rows copied.\n", li_rowsread);

	return TRUE;
}

void
pusage(void)
{
	fprintf(stderr, "usage:  freebcp [[database_name.]owner.]table_name {in | out} datafile\n");
	fprintf(stderr, "        [-m maxerrors] [-f formatfile] [-e errfile]\n");
	fprintf(stderr, "        [-F firstrow] [-L lastrow] [-b batchsize]\n");
	fprintf(stderr, "        [-n] [-c] [-t field_terminator] [-r row_terminator]\n");
	fprintf(stderr, "        [-U username] [-P password] [-I interfaces_file] [-S server]\n");
	fprintf(stderr, "        [-v] [-d] [-h \"hint [,...]\" \n");
	fprintf(stderr, "        [-A packet size] [-T text or image size] [-E]\n");
	fprintf(stderr, "        \n");
	fprintf(stderr, "example: freebcp testdb.dbo.inserttest in inserttest.txt -S mssql -U guest -P password -c\n");
}

int
err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{

	if (dberr) {
		fprintf(stderr, "Msg %d, Level %d\n", dberr, severity);
		fprintf(stderr, "%s\n\n", dberrstr);
	}

	else {
		fprintf(stderr, "DB-LIBRARY error:\n\t");
		fprintf(stderr, "%s\n", dberrstr);
	}

	return (INT_CANCEL);
}

int
msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
{
	/*
	 * ** If it's a database change message, we'll ignore it.
	 * ** Also ignore language change message.
	 */
	if (msgno == 5701 || msgno == 5703)
		return (0);

	printf("Msg %ld, Level %d, State %d\n", (long) msgno, severity, msgstate);

	if (strlen(srvname) > 0)
		printf("Server '%s', ", srvname);
	if (strlen(procname) > 0)
		printf("Procedure '%s', ", procname);
	if (line > 0)
		printf("Line %d", line);

	printf("\n\t%s\n", msgtext);

	return (0);
}

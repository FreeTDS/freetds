/* 
 * Purpose: Test retrieving output parameters and return status 
 * Functions: DBTDS dbnumrets dbresults dbretdata dbretlen dbretname dbrettype dbsqlexec
 */

#include "common.h"
#include <assert.h>

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	int erc, failed = 0;
	char *retname = NULL;
	int rettype = 0, retlen = 0;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0022");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	printf("Dropping proc\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while ((erc = dbresults(dbproc)) == SUCCEED) {
		printf("dbresult succeeded dropping procedure\n");
		while ((erc = dbnextrow(dbproc)) == SUCCEED) {
			printf("dbnextrow returned spurious rows dropping procedure\n");
			assert(0); /* dropping a procedure returns no rows */
		}
		assert(erc == NO_MORE_ROWS);
	}
	assert(erc == NO_MORE_RESULTS);

	printf("creating proc\n");
	sql_cmd(dbproc);
	if (dbsqlexec(dbproc) == FAIL) {
		printf("Failed to create proc t0022.\n");
		exit(1);
	}
	while ((erc = dbresults(dbproc)) != NO_MORE_RESULTS) {
		assert(erc != FAIL);
		while ((erc = dbnextrow(dbproc)) == SUCCEED) {
			assert(0); /* creating a procedure returns no rows */
		}
		assert(erc == NO_MORE_ROWS);
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	while ((erc = dbresults(dbproc)) != NO_MORE_RESULTS) {
		if (erc == FAIL) {
			printf("Was expecting a result set.\n");
			exit(1);
		}
		while ((erc = dbnextrow(dbproc)) == SUCCEED) {
			assert(0); /* procedure returns no rows */
		}
		assert(erc == NO_MORE_ROWS);
	}

#if defined(DBTDS_7_0) && defined(DBTDS_7_1) && defined(DBTDS_7_2) && defined(DBTDS_7_3) \
	&& defined(DBTDS_7_4) && defined(DBTDS_8_0_)
	if ((dbnumrets(dbproc) == 0)
	    && ((DBTDS(dbproc) == DBTDS_7_0)
		|| (DBTDS(dbproc) == DBTDS_7_1)
		|| (DBTDS(dbproc) == DBTDS_7_2)
		|| (DBTDS(dbproc) == DBTDS_7_3)
		|| (DBTDS(dbproc) == DBTDS_7_4)
		|| (DBTDS(dbproc) == DBTDS_8_0_))) {
		printf("WARNING:  Received no return parameters from server!\n");
		printf("WARNING:  This is likely due to a bug in Microsoft\n");
		printf("WARNING:  SQL Server 7.0 SP3 and later.\n");
		printf("WARNING:  Please try again using TDS protocol 4.2.\n");
		dbcmd(dbproc, "drop proc t0022");
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
		dbexit();
		exit(0);
	}
#endif
	for (i = 1; i <= dbnumrets(dbproc); i++) {
		retname = dbretname(dbproc, i);
		printf("ret name %d is %s\n", i, retname);
		rettype = dbrettype(dbproc, i);
		printf("ret type %d is %d\n", i, rettype);
		retlen = dbretlen(dbproc, i);
		printf("ret len %d is %d\n", i, retlen);
		dbconvert(dbproc, rettype, dbretdata(dbproc, i), retlen, SYBVARCHAR, (BYTE*) teststr, -1);
		printf("ret data %d is %s\n", i, teststr);
	}
	if ((retname == NULL) || strcmp(retname, "@b")) {
		printf("Was expecting a retname to be @b.\n");
		exit(1);
	}
	if (strcmp(teststr, "42")) {
		printf("Was expecting a retdata to be 42.\n");
		exit(1);
	}
	if (rettype != SYBINT4) {
		printf("Was expecting a rettype to be SYBINT4 was %d.\n", rettype);
		exit(1);
	}
	if (retlen != 4) {
		printf("Was expecting a retlen to be 4.\n");
		exit(1);
	}

	printf("Dropping proc\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	
	/*
	 * Chapter 2: test for resultsets containing only a return status
	 */
	
	printf("Dropping proc t0022a\n");
	sql_cmd(dbproc);

	dbsqlexec(dbproc);

	while ((erc = dbresults(dbproc)) == SUCCEED) {
		printf("dbresult succeeded dropping procedure\n");
		while ((erc = dbnextrow(dbproc)) == SUCCEED) {
			printf("dbnextrow returned spurious rows dropping procedure\n");
			assert(0); /* dropping a procedure returns no rows */
		}
		assert(erc == NO_MORE_ROWS);
	}
	assert(erc == NO_MORE_RESULTS);

	printf("creating proc t0022a\n");
	sql_cmd(dbproc);
	if (dbsqlexec(dbproc) == FAIL) {
		printf("Failed to create proc t0022a.\n");
		exit(1);
	}
	while ((erc = dbresults(dbproc)) != NO_MORE_RESULTS) {
		assert(erc != FAIL);
		while ((erc = dbnextrow(dbproc)) == SUCCEED) {
			assert(0); /* creating a procedure returns no rows */
		}
		assert(erc == NO_MORE_ROWS);
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	for (i=1; (erc = dbresults(dbproc)) != NO_MORE_RESULTS; i++) {
		enum {expected_iterations = 2};
		DBBOOL fret;
		DBINT  status;
		if (erc == FAIL) {
			printf("t0022a failed for some reason.\n");
			exit(1);
		}
		printf("procedure returned %srows\n", DBROWS(dbproc)==SUCCEED? "" : "no ");
		while ((erc = dbnextrow(dbproc)) == SUCCEED) {
			assert(0); /* procedure returns no rows */
		}
		assert(erc == NO_MORE_ROWS);
		
		fret = dbhasretstat(dbproc);
		printf("procedure has %sreturn status\n", fret==TRUE? "" : "no ");
		assert(fret == TRUE);
		
		status = dbretstatus(dbproc);
		printf("return status %d is %d\n", i, (int) status);
		switch (i) {
		case 1: assert(status == 17); break;
		case 2: assert(status == 1024); break;
		default: assert(i <= expected_iterations);
		}
		
	}

	assert(erc == NO_MORE_RESULTS);
	
	printf("Dropping proc t0022a\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	
	/* end chapter 2 */


	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}

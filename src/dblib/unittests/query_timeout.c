/*
 * Purpose: Test setting of query timeouts on a per-connection basis.
 * Functions:  dberrhandle, dbsettimeout, dbgettimeout
 */

#include "common.h"
#include <time.h>

int ntimeouts = 0, ncancels = 0;
const int max_timeouts = 3, timeout_seconds = 1;
time_t start_time;

int timeout_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
int chkintr(DBPROCESS * dbproc);
int hndlintr(DBPROCESS * dbproc);

#if !defined(SYBETIME)
#define SYBETIME SQLETIME
#define INT_TIMEOUT INT_CANCEL
dbsetinterrupt(DBPROCESS *dbproc, void* hand, void* p) {}
#endif

int
timeout_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	/*
	 * For server messages, cancel the query and rely on the
	 * message handler to spew the appropriate error messages out.
	 */
	if (dberr == SYBESMSG)
		return INT_CANCEL;

	if (dberr == SYBETIME) {
		fprintf(stderr, "%d timeouts received in %ld seconds, ", ++ntimeouts, (long int) (time(NULL) - start_time));
		if (ntimeouts > max_timeouts) {
			if (++ncancels > 1) {
				fprintf(stderr, "could not timeout cleanly, breaking connection\n");
				ncancels = 0;
				return INT_CANCEL;
			}
			fprintf(stderr, "lost patience, cancelling (allowing 10 seconds)\n");
			if (dbsettime(10) == FAIL)
				fprintf(stderr, "... but dbsettime() failed in error handler\n");
			return INT_TIMEOUT;
		}
		fprintf(stderr, "continuing to wait\n");
		return INT_CONTINUE;
	}

	ntimeouts = 0; /* reset */

	fprintf(stderr,
		"DB-LIBRARY error (severity %d, dberr %d, oserr %d, dberrstr %s, oserrstr %s):\n",
		severity, dberr, oserr, dberrstr ? dberrstr : "(null)", oserrstr ? oserrstr : "(null)");
	fflush(stderr);

	/*
	 * If the dbprocess is dead or the dbproc is a NULL pointer and
	 * we are not in the middle of logging in, then we need to exit.
	 * We can't do anything from here on out anyway.
	 * It's OK to end up here in response to a dbconvert() that
	 * resulted in overflow, so don't exit in that case.
	 */
	if ((dbproc == NULL) || DBDEAD(dbproc)) {
		if (dberr != SYBECOFL) {
			fprintf(stderr, "error: dbproc (%p) is %s, goodbye\n",
					dbproc, dbproc? (DBDEAD(dbproc)? "DEAD" : "OK") : "NULL");
			exit(255);
		}
	}

	return INT_CANCEL;
}

int
chkintr(DBPROCESS * dbproc)
{
	printf("in chkintr, %ld seconds elapsed\n", (long int) (time(NULL) - start_time));
	return FALSE;
}

int
hndlintr(DBPROCESS * dbproc)
{
	printf("in hndlintr, %ld seconds elapsed\n", (long int) (time(NULL) - start_time));
	return INT_CONTINUE;
}

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i,r, failed = 0;
	RETCODE erc, row_code;
	int num_resultset = 0;
	char teststr[1024];

	/*
	 * Connect to server
	 */
	set_malloc_options();

	read_login_info(argc, argv);

	fprintf(stdout, "Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(timeout_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "#timeout");

	fprintf(stdout, "About to open %s.%s\n", SERVER, DATABASE);

	/*
	 * One way to test the login timeout is to connect to a discard server (grep discard /etc/services).
	 * It's normally supported by inetd.
	 */
	printf ("using %d 1-second login timeouts\n", max_timeouts);
	dbsetlogintime(1);

	start_time = time(NULL);	/* keep track of when we started for reporting purposes */

	if (NULL == (dbproc = dbopen(login, SERVER))){
		fprintf(stderr, "Failed: dbopen\n");
		exit(1);
	}

	printf ("connected.\n");

	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

	dbloginfree(login);

    /* Set a very long global timeout. */
    dbsettime(5 * 60);

	/* send something that will take awhile to execute */
	printf ("using %d %d-second query timeouts\n", max_timeouts, timeout_seconds);
	if (FAIL == dbsettimeout(dbproc, timeout_seconds)) {
		fprintf(stderr, "Failed: dbsettimeout\n");
		exit(1);
	}
	printf ("issuing a query that will take 30 seconds\n");

	if (FAIL == sql_cmd(dbproc)) {
		fprintf(stderr, "Failed: dbcmd\n");
		exit(1);
	}

	start_time = time(NULL);
	ntimeouts = 0;
	dbsetinterrupt(dbproc, (void*)chkintr, (void*)hndlintr);

	if (FAIL == dbsqlsend(dbproc)) {
		fprintf(stderr, "Failed: dbsend\n");
		exit(1);
	}

	/* wait for it to execute */
	printf("executing dbsqlok\n");
	erc = dbsqlok(dbproc);
	if (erc != FAIL) {
		fprintf(stderr, "dbsqlok should fail for timeout\n");
		exit(1);
	}

	/* retrieve outputs per usual */
	r = 0;
	for (i=0; (erc = dbresults(dbproc)) != NO_MORE_RESULTS; i++) {
		int nrows, ncols;
		switch (erc) {
		case SUCCEED:
			if (DBROWS(dbproc) == FAIL){
				r++;
				continue;
			}
			assert(DBROWS(dbproc) == SUCCEED);
			printf("dbrows() returned SUCCEED, processing rows\n");

			ncols = dbnumcols(dbproc);
			++num_resultset;
			printf("bound 1 of %d columns ('%s') in result %d.\n", ncols, dbcolname(dbproc, 1), ++r);
			dbbind(dbproc, 1, STRINGBIND, 0, (BYTE *) teststr);

			printf("\t%s\n\t-----------\n", dbcolname(dbproc, 1));
			while ((row_code = dbnextrow(dbproc)) != NO_MORE_ROWS) {
				if (row_code == REG_ROW) {
					printf("\t%s\n", teststr);
				} else {
					/* not supporting computed rows in this unit test */
					failed = 1;
					fprintf(stderr, "Failed.  Expected a row\n");
					exit(1);
				}
			}
			nrows = (int) dbcount(dbproc);
			printf("row count %d\n", nrows);
			if (nrows < 0){
				failed++;
				printf("error: dbrows() returned SUCCEED, but dbcount() returned -1\n");
			}

            if (timeout_seconds != dbgettimeout(dbproc)) {
                failed++;
				printf("error: dbgettimeout(%p) returned unexpected value %d\n", dbproc, dbgettimeout(dbproc));
            }
			break;
		case FAIL:
			printf("dbresults returned FAIL\n");
			exit(1);
		default:
			printf("unexpected return code %d from dbresults\n", erc);
			exit(1);
		}
		if (i > 1) {
			failed++;
			break;
		}
	} /* while dbresults */

	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}

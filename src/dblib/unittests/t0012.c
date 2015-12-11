/* 
 * Purpose: Test datetime conversion as well as dbdata() & dbdatlen()
 * Functions: dbcmd dbdata dbdatecrack dbdatlen dbnextrow dbresults dbsqlexec
 */

#include "common.h"

static int failed = 0;
static void set_failed(int line)
{
	failed = 1;
	fprintf(stderr, "Failed check at line %d\n", line);
}
#define set_failed() set_failed(__LINE__)

#ifdef MSDBLIB
#define dateyear year
#define datemonth month
#define datedmonth day
#define datedyear dayofyear
#define datedweek weekday
#define datehour hour
#define dateminute minute
#define datesecond second
#define datemsecond millisecond
#define datensecond nanosecond
#endif

static int
ignore_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line)
{
	return 0;
}

int
main(int argc, char *argv[])
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	char datestring[256];
	DBDATEREC  dateinfo;
#ifdef SYBMSDATETIME2
	DBDATEREC2 dateinfo2;
#endif
	DBDATETIME mydatetime;
	int output_count = 0;

	set_malloc_options();

	read_login_info(argc, argv);
	fprintf(stdout, "Starting %s\n", argv[0]);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0012");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
	}
	dbloginfree(login);
	fprintf(stdout, "After logon\n");

	fprintf(stdout, "creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	/* insert */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	/* insert */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	/* select */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	dbresults(dbproc);

	while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		++output_count;
		/* Print the date info  */
		dbconvert(dbproc, dbcoltype(dbproc, 1), dbdata(dbproc, 1), dbdatlen(dbproc, 1), SYBCHAR, (BYTE*) datestring, -1);

		printf("%s\n", datestring);

		/* Break up the creation date into its constituent parts */
		memcpy(&mydatetime, (DBDATETIME *) (dbdata(dbproc, 1)), sizeof(DBDATETIME));
		dbdatecrack(dbproc, &dateinfo, &mydatetime);

		/* Print the parts of the creation date */
		printf("\tYear = %d.\n", dateinfo.dateyear);
		printf("\tMonth = %d.\n", dateinfo.datemonth);
		printf("\tDay of month = %d.\n", dateinfo.datedmonth);
		printf("\tDay of year = %d.\n", dateinfo.datedyear);
		printf("\tDay of week = %d.\n", dateinfo.datedweek);
		printf("\tHour = %d.\n", dateinfo.datehour);
		printf("\tMinute = %d.\n", dateinfo.dateminute);
		printf("\tSecond = %d.\n", dateinfo.datesecond);
		printf("\tMillisecond = %d.\n", dateinfo.datemsecond);
		if (dateinfo.dateyear != 1898 && dateinfo.dateyear != 2001)
			set_failed();
		if (dateinfo.dateminute != 24 && dateinfo.dateminute != 30)
			set_failed();
		if (dateinfo.datehour != 19 && dateinfo.datehour != 10)
			set_failed();
	}
	dbresults(dbproc);

	if (output_count != 2)
		set_failed();

#ifdef SYBMSDATETIME2
	dbmsghandle(ignore_msg_handler);

	/* select */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	dbresults(dbproc);

	while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		int type = dbcoltype(dbproc, 1);

		++output_count;
		/* Print the date info  */
		dbconvert(dbproc, type, dbdata(dbproc, 1), dbdatlen(dbproc, 1), SYBCHAR, (BYTE*) datestring, -1);

		printf("%s\n", datestring);

		/* network not high enough for this type ! */
		if (type == SYBCHAR || type == SYBVARCHAR)
			break;

		/* Break up the creation date into its constituent parts */
		if (dbanydatecrack(dbproc, &dateinfo2, dbcoltype(dbproc, 1), dbdata(dbproc, 1)) != SUCCEED)
			set_failed();

		/* Print the parts of the creation date */
		printf("\tYear = %d.\n", dateinfo2.dateyear);
		printf("\tMonth = %d.\n", dateinfo2.datemonth);
		printf("\tDay of month = %d.\n", dateinfo2.datedmonth);
		printf("\tDay of year = %d.\n", dateinfo2.datedyear);
		printf("\tDay of week = %d.\n", dateinfo2.datedweek);
		printf("\tHour = %d.\n", dateinfo2.datehour);
		printf("\tMinute = %d.\n", dateinfo2.dateminute);
		printf("\tSecond = %d.\n", dateinfo2.datesecond);
		printf("\tNanosecond = %d.\n", dateinfo2.datensecond);
		if (dateinfo2.dateyear != 1898 || dateinfo2.datensecond != 567000000)
			set_failed();
	}
#endif

	dbclose(dbproc);
	dbexit();

	if (output_count < 2 || output_count > 3)
		set_failed();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}

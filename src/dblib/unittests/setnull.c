/* 
 * Purpose: dbnull behavior
 */

#include "common.h"

#if HAVE_LIBGEN_H
#include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include "replacements.h"

static char software_version[] = "$Id: setnull.c,v 1.3 2007-12-06 09:19:22 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT db_i;
	RETCODE ret;
	
	int failed = 0;

	read_login_info(argc, argv);

	printf("Start\n");
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "setnull");


	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	/* try to set an int */
	db_i = 0xdeadbeef;
	ret = dbsetnull(dbproc, INTBIND, 0, (BYTE *) &db_i);
	if (ret != SUCCEED) {
		fprintf(stderr, "dbsetnull returned error %d\n", (int) ret);
		failed = 1;
	}

	ret = dbsetnull(dbproc, INTBIND, 1, (BYTE *) &db_i);
	if (ret != SUCCEED) {
		fprintf(stderr, "dbsetnull returned error %d\n", (int) ret);
		failed = 1;
	}

	/* check result */
	db_i = 0;
	dbcmd(dbproc, "select cast(NULL as int)");

	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "Was expecting a row.\n");
		failed = 1;
		dbcancel(dbproc);
	}

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) &db_i);
	printf("db_i = %ld\n", (long int) db_i);

	if (dbnextrow(dbproc) != REG_ROW) {
		fprintf(stderr, "Was expecting a row.\n");
		failed = 1;
		dbcancel(dbproc);
	}
	printf("db_i = %ld\n", (long int) db_i);

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		fprintf(stderr, "Only one row expected\n");
		dbcancel(dbproc);
		failed = 1;
	}

	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	if (db_i != 0xdeadbeef) {
		fprintf(stderr, "Invalid NULL %ld returned (%s:%d)\n", (long int) db_i, tds_basename(__FILE__), __LINE__);
		failed = 1;
	}

	/* try if dbset null consider length */
	for (db_i = 1; db_i > 0; db_i <<= 1) {
		printf("db_i = %ld\n", (long int) db_i);
		ret = dbsetnull(dbproc, INTBIND, db_i, (BYTE *) &db_i);
		if (ret != SUCCEED) {
			fprintf(stderr, "dbsetnull returned error %d for bindlen %ld\n", (int) ret, (long int) db_i);
			failed = 1;
			break;
		}
	}

	printf("dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	dbexit();

	return failed ? 1 : 0;
}

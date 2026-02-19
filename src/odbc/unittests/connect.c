#include "common.h"


static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	CHKAllocConnect(&odbc_conn, "S");
}

TEST_MAIN()
{
	char tmp[1024*4];
	SQLSMALLINT len;
	int succeeded = 0;
	bool is_freetds;
	bool is_ms;
	SQLRETURN rc;

	/* NOTE: In Windows, this function will copy the data from PWD into
	 * a registry entry HKCU\Software\ODBC\ODBC.INI\{servername}
	 */
	if (odbc_read_login_info())
		exit(1);

#ifndef _WIN32
	/*
	 * prepare our odbcinst.ini 
	 * is better to do it before connect cause uniODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 *
	 * In Windows, odbcinst.ini text file is not used - you will need to
	 * configure FreeTDS via the Odbcinst.ini registry key.
	 */
	if (common_pwd.driver[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\n", common_pwd.driver);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}
#endif
	/* Connect using SQLConnect() with Server, User and Password from PWD file
	 * NOTE: In Windows this looks up DSN in HKCU\Software\ODBC\ODBC.INI, with fallback to HKLM
	 */
	printf("SQLConnect(server=%s, ...) connect..\n", common_pwd.server);
	odbc_connect();
	is_freetds = odbc_driver_is_freetds();
	is_ms = odbc_db_is_microsoft();
	odbc_disconnect();
	++succeeded;

	if (!is_freetds) {
		printf("Driver is not FreeTDS, exiting\n");
		odbc_test_skipped();
		return 0;
	}

	/* try connect string with using DSN= (should behave the same as SQLConnect)
	 */
	printf("SQLDriverConnect(DSN=%s;...)\n", common_pwd.server);
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", common_pwd.server,
		common_pwd.user, common_pwd.password, common_pwd.database);
	CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SI");
	odbc_disconnect();
	++succeeded;

	/* try connect string using DRIVER=  (this means to look up DRIVER name in ODBCINST.INI) */
	printf("SQLDriverConnect(DRIVER=FreeTDS;SERVERNAME=%s;...)\n", common_pwd.server);
#ifndef _WIN32
	printf("odbcinst.ini must exist and contain an entry [FreeTDS].\n");
#endif
	/* this is expected to work with unixODBC, and Windows.
	 * NOTE: This will connect to whatever driver you have configured in ODBCINST.INI,
	 * not the actual driver we just built and are testing. However for this test,
	 * that's OK as here we are testing connecting to a driver, not testing that driver.
	 */
	init_connect();
	sprintf(tmp,
		"DRIVER=FreeTDS;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;",
		common_pwd.server, common_pwd.user, common_pwd.password, common_pwd.database);
	rc = CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp) / sizeof(SQLTCHAR), &len,
			      SQL_DRIVER_NOPROMPT, "SIE");
	if (rc == SQL_ERROR) {
		printf("Unable to open data source (ret=%d)\n", rc);
#ifdef _WIN32
		printf("Try from admin command prompt:\n"
		       "\todbcconf /A {INSTALLDRIVER \"FreeTDS|Driver=C:\\Program Files(x86)\\FreeTDS\\bin\\tdsodbc.dll\"}\n"
		       "(replace path with your installation path for tdsodbc.dll if necessary)\n");
#endif
	} else {
		++succeeded;
	}
	odbc_disconnect();

	/* this is expected to work with iODBC (passing full path to driver)
	 */
	if (common_pwd.driver[0]) {
		printf("SQLDriverConnect(DRIVER=%s;...)\n", common_pwd.driver);
		init_connect();
		sprintf(tmp,
			"DRIVER=%s;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;",
			common_pwd.driver, common_pwd.server, common_pwd.user, common_pwd.password, common_pwd.database);
		rc = CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp) / sizeof(SQLTCHAR), &len,
				      SQL_DRIVER_NOPROMPT, "SIE");
		if (rc == SQL_ERROR) {
			printf("Unable to open data source via iODBC syntax (ret=%d)\n", rc);
		} else {
			++succeeded;
		}
		odbc_disconnect();
	}

	if (is_ms) {
		char app_name[130];

		memset(app_name, 'a', sizeof(app_name));
		app_name[sizeof(app_name) - 1] = 0;

		/* Try passing very long APP string.
		 * The server is supposed to fail the connection if
		 * this string is too long, make sure we truncate it.
		 */
		printf("connect string DSN connect with a long APP..\n");
		init_connect();
		sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;APP=%s",
			common_pwd.server, common_pwd.user, common_pwd.password, common_pwd.database, app_name);
		CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp) / sizeof(SQLTCHAR), &len,
				 SQL_DRIVER_NOPROMPT, "SI");
		odbc_disconnect();
	}

	/* test invalid string, it should not leak memory */
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;test={unfinished",
		common_pwd.server, common_pwd.user, common_pwd.password, common_pwd.database);
	CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp) / sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "E");
	odbc_disconnect();

	/* at least one should success.. */
	if (succeeded < 3) {
		ODBC_REPORT_ERROR("Too few successes");
		exit(1);
	}

	printf("Done.\n");
	return 0;
}

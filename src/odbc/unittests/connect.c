#include "common.h"


static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	CHKAllocConnect(&odbc_conn, "S");
}

#ifdef _WIN32
#include <odbcinst.h>

static char *entry = NULL;

static char *
get_entry(const char *key)
{
	static TCHAR buf[256];

	entry = NULL;
	if (SQLGetPrivateProfileString(T(odbc_server), T(key), TEXT(""), buf, TDS_VECTOR_SIZE(buf), TEXT("odbc.ini")) > 0)
		entry = C(buf);

	return entry;
}
#endif

int
main(int argc, char *argv[])
{
	char tmp[1024*4];
	SQLSMALLINT len;
	int succeeded = 0;
	bool is_freetds;
	bool is_ms;
	SQLRETURN rc;

	if (odbc_read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini 
	 * is better to do it before connect cause uniODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 */
	if (odbc_driver[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\n", odbc_driver);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	printf("SQLConnect connect..\n");
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

	/* try connect string with using DSN */
	printf("connect string DSN connect..\n");
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", odbc_server, odbc_user, odbc_password, odbc_database);
	CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SI");
	odbc_disconnect();
	++succeeded;

	/* try connect string using old SERVERNAME specification */
	printf("connect string SERVERNAME connect..\n");
	printf("odbcinst.ini must be configured with FreeTDS driver..\n");

	/* this is expected to work with unixODBC */
	init_connect();
	sprintf(tmp, "DRIVER=FreeTDS;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;", odbc_server, odbc_user, odbc_password, odbc_database);
	rc = CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SIE");
	if (rc == SQL_ERROR) {
		printf("Unable to open data source (ret=%d)\n", rc);
	} else {
		++succeeded;
	}
	odbc_disconnect();

	/* this is expected to work with iODBC
	 * (passing shared object name as driver)
	 */
	if (odbc_driver[0]) {
		init_connect();
		sprintf(tmp, "DRIVER=%s;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;", odbc_driver, odbc_server, odbc_user, odbc_password, odbc_database);
		rc = CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SIE");
		if (rc == SQL_ERROR) {
			printf("Unable to open data source (ret=%d)\n", rc);
		} else {
			++succeeded;
		}
		odbc_disconnect();
	}

#ifdef _WIN32
	if (get_entry("SERVER")) {
		init_connect();
		sprintf(tmp, "DRIVER=FreeTDS;SERVER=%s;UID=%s;PWD=%s;DATABASE=%s;", entry, odbc_user, odbc_password, odbc_database);
		if (get_entry("TDS_Version"))
			sprintf(strchr(tmp, 0), "TDS_Version=%s;", entry);
		if (get_entry("Port"))
			sprintf(strchr(tmp, 0), "Port=%s;", entry);
		rc = CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SIE");
		if (rc == SQL_ERROR) {
			printf("Unable to open data source (ret=%d)\n", rc);
		} else {
			++succeeded;
		}
		odbc_disconnect();
	}
#endif

	if (is_ms) {
		char app_name[130];
		memset(app_name, 'a', sizeof(app_name));
		app_name[sizeof(app_name) - 1] = 0;

		/* Try passing very long APP string.
		 * The server is supposed to fail the connection if
		 * this string is too long, make sure we trucate it.
		 */
		printf("connect string DSN connect with a long APP..\n");
		init_connect();
		sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;APP=%s", odbc_server, odbc_user, odbc_password, odbc_database, app_name);
		CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SI");
		odbc_disconnect();
	}

	/* at least one should success.. */
	if (succeeded < 3) {
		ODBC_REPORT_ERROR("Too few successes");
		exit(1);
	}

	printf("Done.\n");
	return 0;
}

/*
 * Code to test ODBC implementation.
 *  - David Fraser, Abelon Systems 2003.
 */

/* 
 * TODO
 * remove Northwind dependency
 */

#include "common.h"

static char software_version[] = "$Id: testodbc.c,v 1.7 2004-03-12 12:42:41 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#ifdef DEBUG
# define AB_FUNCT(x)  do { printf x; printf("\n"); } while(0)
# define AB_PRINT(x)  do { printf x; printf("\n"); } while(0)
#else
# define AB_FUNCT(x)
# define AB_PRINT(x)
#endif
#define AB_ERROR(x)   do { printf("ERROR: "); printf x; printf("\n"); } while(0)

#undef TRUE
#undef FALSE
enum
{ FALSE, TRUE };
typedef int DbTestFn(void);

static int RunTests(void);

typedef struct
{
	DbTestFn *testFn;
	const char *description;
} DbTestEntry;

/*
 * Output ODBC errors.
 */
static void
DispODBCErrs(SQLHENV envHandle, SQLHDBC connHandle, SQLHSTMT statementHandle)
{
	SQLCHAR buffer[256];
	SQLCHAR sqlState[16];

	/* Statement errors */
	if (statementHandle) {
		while (SQLError(envHandle, connHandle, statementHandle, sqlState, 0, buffer, sizeof(buffer), 0) == SQL_SUCCESS) {
			AB_ERROR(("%s, SQLSTATE=%s", buffer, sqlState));
		}
	}

	/* Connection errors */
	while (SQLError(envHandle, connHandle, SQL_NULL_HSTMT, sqlState, 0, buffer, sizeof(buffer), 0) == SQL_SUCCESS) {
		AB_ERROR(("%s, SQLSTATE=%s", buffer, sqlState));
	}

	/* Environmental errors */
	while (SQLError(envHandle, SQL_NULL_HDBC, SQL_NULL_HSTMT, sqlState, 0, buffer, sizeof(buffer), 0) == SQL_SUCCESS) {
		AB_ERROR(("%s, SQLSTATE=%s", buffer, sqlState));
	}
}

/*
 * Output ODBC diagnostics. Only used for 'raw' ODBC tests.
 */
static void
DispODBCDiags(SQLHSTMT statementHandle)
{
	SQLSMALLINT recNumber;
	SQLCHAR sqlState[10];
	SQLINTEGER nativeError = -99;
	SQLCHAR messageText[500];
	SQLSMALLINT bufferLength = 500;
	SQLSMALLINT textLength = -99;
	SQLRETURN status;

	recNumber = 1;

	AB_FUNCT(("DispODBCDiags (in)"));

	do {
		status = SQLGetDiagRec(SQL_HANDLE_STMT, statementHandle, recNumber,
				       sqlState, &nativeError, messageText, bufferLength, &textLength);
		if (status != SQL_SUCCESS) {
			/* No data mean normal end of iteration. Anything else is error. */
			if (status != SQL_NO_DATA) {
				AB_ERROR(("SQLGetDiagRec status is %d", status));
			}
			break;
		}
		printf("DIAG #%d, sqlState=%s, nativeError=%d, message=%s\n", recNumber, sqlState, (int) nativeError, messageText);
		recNumber++;
	} while (status == SQL_SUCCESS);

	AB_FUNCT(("DispODBCDiags (out)"));
}

/*
 * Test that makes a parameterized ODBC query using SQLPrepare and SQLExecute
 */
static int
TestRawODBCPreparedQuery(void)
{
	SQLRETURN status;
	SQLCHAR queryString[200];
	SQLINTEGER lenOrInd = 0;
	SQLSMALLINT supplierId = 4;
	int count;

	AB_FUNCT(("TestRawODBCPreparedQuery (in)"));

	/* INIT */

	Connect();

	/* MAKE QUERY */

	Command(Statement, "CREATE TABLE #Products ("
		"ProductID int NOT NULL ,"
		"ProductName varchar (40) ,"
		"SupplierID int NULL ,"
		"CategoryID int NULL ,"
		"QuantityPerUnit varchar (20)  ,"
		"UnitPrice money NULL ,"
		"UnitsInStock smallint NULL ,"
		"UnitsOnOrder smallint NULL ,"
		"ReorderLevel smallint NULL ,"
		"Discontinued bit NOT NULL "
		") "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(9,'Mishi Kobe Niku',4,6,'18 - 500 g pkgs.',97.00,29,0,0,1) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(10,'Ikura',4,8,'12 - 200 ml jars',31.00,31,0,0,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(74,'Longlife Tofu',4,7,'5 kg pkg.',10.00,4,20,5,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(11,'Queso Cabrales',5,4,'1 kg pkg.',21.00,22,30,30,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(12,'Queso Manchego La Pastora',5,4,'10 - 500 g pkgs.',38.00,86,0,0,0)");
	while (SQLMoreResults(Statement) == SQL_SUCCESS);

	strcpy((char *) (queryString), "SELECT * FROM #Products WHERE SupplierID = ?");

	status = SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_INTEGER, 0, 0, &supplierId, 0, &lenOrInd);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		DispODBCErrs(Environment, Connection, Statement);
		DispODBCDiags(Statement);
		AB_FUNCT(("TestRawODBCPreparedQuery (out): error"));
		return FALSE;
	}

	status = SQLPrepare(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Prepare failed"));
		AB_FUNCT(("TestRawODBCPreparedQuery (out): error"));
		return FALSE;
	}

	status = SQLExecute(Statement);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Execute failed"));
		DispODBCErrs(Environment, Connection, Statement);
		DispODBCDiags(Statement);
		AB_FUNCT(("TestRawODBCPreparedQuery (out): error"));
		return FALSE;
	}

	count = 0;

	while (SQLFetch(Statement) == SQL_SUCCESS) {
		count++;
	}
	AB_PRINT(("Got %d rows", count));

	if (count != 3) {
		/*
		 * OK - so 3 is a magic number - it's the number of rows matching
		 * this query from the MS sample Northwind database and is a constant.
		 */
		AB_ERROR(("Expected %d rows - but got %d rows", 3, count));
		AB_FUNCT(("TestRawODBCPreparedQuery (out): error"));
		return FALSE;
	}

	/* CLOSEDOWN */

	Disconnect();

	AB_FUNCT(("TestRawODBCPreparedQuery (out): ok"));
	return TRUE;
}

/*
 * Test that makes a parameterized ODBC query using SQLExecDirect.
 */
static int
TestRawODBCDirectQuery(void)
{
	SQLRETURN status;
	SQLCHAR queryString[200];
	SQLINTEGER lenOrInd = 0;
	SQLSMALLINT supplierId = 1;
	int count;

	AB_FUNCT(("TestRawODBCDirectQuery (in)"));

	/* INIT */

	Connect();

	/* MAKE QUERY */

	Command(Statement, "CREATE TABLE #Products ("
		"ProductID int NOT NULL ,"
		"ProductName varchar (40) ,"
		"SupplierID int NULL ,"
		"CategoryID int NULL ,"
		"QuantityPerUnit varchar (20)  ,"
		"UnitPrice money NULL ,"
		"UnitsInStock smallint NULL ,"
		"UnitsOnOrder smallint NULL ,"
		"ReorderLevel smallint NULL ,"
		"Discontinued bit NOT NULL "
		") "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(1,'Chai',1,1,'10 boxes x 20 bags',18.00,39,0,10,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(2,'Chang',1,1,'24 - 12 oz bottles',19.00,17,40,25,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(3,'Aniseed Syrup',1,2,'12 - 550 ml bottles',10.00,13,70,25,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(4,'Chef Anton''s Cajun Seasoning',2,2,'48 - 6 oz jars',22.00,53,0,0,0) "
		"INSERT INTO #Products(ProductID,ProductName,SupplierID,CategoryID,QuantityPerUnit,UnitPrice,UnitsInStock,UnitsOnOrder,ReorderLevel,Discontinued) VALUES(5,'Chef Anton''s Gumbo Mix',2,2,'36 boxes',21.35,0,0,0,1) ");
	while (SQLMoreResults(Statement) == SQL_SUCCESS);

	strcpy((char *) (queryString), "SELECT * FROM #Products WHERE SupplierID = ?");

	status = SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_INTEGER, 0, 0, &supplierId, 0, &lenOrInd);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		DispODBCErrs(Environment, Connection, Statement);
		DispODBCDiags(Statement);
		AB_FUNCT(("TestRawODBCDirectQuery (out): error"));
		return FALSE;
	}

	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Execute failed"));
		DispODBCErrs(Environment, Connection, Statement);
		DispODBCDiags(Statement);
		AB_FUNCT(("TestRawODBCDirectQuery (out): error"));
		return FALSE;
	}

	count = 0;

	while (SQLFetch(Statement) == SQL_SUCCESS) {
		count++;
	}
	AB_PRINT(("Got %d rows", count));

	if (count != 3) {
		/*
		 * OK - so 3 is a magic number - it's the number of rows matching
		 * this query from the MS sample Northwind database and is a constant.
		 */
		AB_ERROR(("Expected %d rows - but got %d rows", 3, count));
		AB_FUNCT(("TestRawODBCDirectQuery (out): error"));
		return FALSE;
	}

	/* CLOSEDOWN */

	Disconnect();

	AB_FUNCT(("TestRawODBCDirectQuery (out): ok"));
	return TRUE;
}

/*
 * Test that show what works and what doesn't for the poorly
 * documented GUID.
 */
static int
TestRawODBCGuid(void)
{
	SQLRETURN status;

	SQLCHAR queryString[300];
	SQLINTEGER lenOrInd;
	SQLSMALLINT age;
	SQLCHAR guid[40];
	SQLCHAR name[20];

	SQLGUID sqlguid;
	int count = 0;

	AB_FUNCT(("TestRawODBCGuid (in)"));

	Connect();
	
	if (!db_is_microsoft()) {
		Disconnect();
		return TRUE;
	}

	AB_PRINT(("Creating #pet table"));

	strcpy((char *) (queryString), "CREATE TABLE #pet (name VARCHAR(20), owner VARCHAR(20), "
	       "species VARCHAR(20), sex CHAR(1), age INTEGER, " "guid UNIQUEIDENTIFIER DEFAULT NEWID() ); ");
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS && status != SQL_NO_DATA) {
		AB_ERROR(("Create table failed"));
		goto odbcfail;
	}

	CommandWithResult(Statement, "DROP PROC GetGUIDRows");

	AB_PRINT(("Creating stored proc GetGUIDRows"));

	strcpy((char *) (queryString), "CREATE PROCEDURE GetGUIDRows (@guidpar uniqueidentifier) AS \
                SELECT name, guid FROM #pet WHERE guid = @guidpar");
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS && status != SQL_NO_DATA) {
		AB_ERROR(("Create procedure failed"));
		goto odbcfail;
	}

	AB_PRINT(("Insert row 1"));

	strcpy((char *) (queryString), "INSERT INTO #pet( name, owner, species, sex, age ) \
                         VALUES ( 'Fang', 'Mike', 'dog', 'm', 12 );");
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Insert row 1 failed"));
		goto odbcfail;
	}

	AB_PRINT(("Insert row 2"));

	/*
	 * Ok - new row with explicit GUID, but parameterised age.
	 */
	strcpy((char *) (queryString), "INSERT INTO #pet( name, owner, species, sex, age, guid ) \
                         VALUES ( 'Splash', 'Dan', 'fish', 'm', ?, \
                         '12345678-1234-1234-1234-123456789012' );");

	lenOrInd = 0;
	age = 3;
	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_INTEGER, 0, 0, &age, 0, &lenOrInd)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		goto odbcfail;
	}

	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Insert row 2 failed"));
		goto odbcfail;
	}
	if (SQLFreeStmt(Statement, SQL_CLOSE) != SQL_SUCCESS) {
		AB_ERROR(("Free statement failed (5)"));
		goto odbcfail;
	}

	AB_PRINT(("Insert row 3"));
	/*
	 * Ok - new row with parameterised GUID.
	 */
	strcpy((char *) (queryString), "INSERT INTO #pet( name, owner, species, sex, age, guid ) \
                         VALUES ( 'Woof', 'Tom', 'cat', 'f', 2, ? );");

	lenOrInd = SQL_NTS;
	strcpy((char *) (guid), "87654321-4321-4321-4321-123456789abc");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_GUID, 0, 0, guid, 0, &lenOrInd)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		goto odbcfail;
	}
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Insert row 3 failed"));
		goto odbcfail;
	}

	AB_PRINT(("Insert row 4"));
	/*
	 * Ok - new row with parameterised GUID.
	 */
	strcpy((char *) (queryString), "INSERT INTO #pet( name, owner, species, sex, age, guid ) \
                         VALUES ( 'Spike', 'Diane', 'pig', 'f', 4, ? );");

	lenOrInd = SQL_NTS;
	strcpy((char *) (guid), "1234abcd-abcd-abcd-abcd-123456789abc");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 36, 0, guid, 0, &lenOrInd)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		goto odbcfail;
	}
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Insert row 4 failed"));
		goto odbcfail;
	}

	AB_PRINT(("Insert row 5"));
	/*
	 * Ok - new row with parameterised GUID.
	 */
	strcpy((char *) (queryString), "INSERT INTO #pet( name, owner, species, sex, age, guid ) \
                         VALUES ( 'Fluffy', 'Sam', 'dragon', 'm', 16, ? );");

	sqlguid.Data1 = 0xaabbccdd;
	sqlguid.Data2 = 0xeeff;
	sqlguid.Data3 = 0x1122;
	sqlguid.Data4[0] = 0x11;
	sqlguid.Data4[1] = 0x22;
	sqlguid.Data4[2] = 0x33;
	sqlguid.Data4[3] = 0x44;
	sqlguid.Data4[4] = 0x55;
	sqlguid.Data4[5] = 0x66;
	sqlguid.Data4[6] = 0x77;
	sqlguid.Data4[7] = 0x88;

	lenOrInd = 16;
	strcpy((char *) (guid), "1234abcd-abcd-abcd-abcd-123456789abc");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_GUID, SQL_GUID, 16, 0, &sqlguid, 16, &lenOrInd)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		goto odbcfail;
	}
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("Insert row 5 failed"));
		AB_ERROR(("Sadly this was expected in *nix ODBC. Carry on."));
	}

	/*
	 * Now retrieve rows - especially GUID column values.
	 */
	AB_PRINT(("retrieving name and guid"));
	strcpy((char *) (queryString), "SELECT name, guid FROM #pet");
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("SELECT failed"));
		goto odbcfail;
	}
	while (SQLFetch(Statement) == SQL_SUCCESS) {
		count++;
		if (SQLGetData(Statement, 1, SQL_CHAR, name, 20, 0)
		    != SQL_SUCCESS) {
			AB_ERROR(("Get row %d, name column failed", count));
			goto odbcfail;
		}
		if (SQLGetData(Statement, 2, SQL_CHAR, guid, 37, 0)
		    != SQL_SUCCESS) {
			AB_ERROR(("Get row %d, guid column failed", count));
			goto odbcfail;
		}

		AB_PRINT(("name: %-10s guid: %s", name, guid));
	}

	/*
	 * Realloc cursor handle - (Windows ODBC considers it an invalid cursor
	 * state if we try SELECT again).
	 */
	if (SQLFreeStmt(Statement, SQL_CLOSE) != SQL_SUCCESS) {
		AB_ERROR(("Free statement failed (5)"));
		goto odbcfail;
	}
	if (SQLAllocHandle(SQL_HANDLE_STMT, Connection, &Statement)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLAllocStmt failed(1)"));
		goto odbcfail;
	}


	/*
	 * Now retrieve rows - especially GUID column values.
	 */

	AB_PRINT(("retrieving name and guid again"));
	strcpy((char *) (queryString), "SELECT name, guid FROM #pet");
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("SELECT failed"));
		goto odbcfail;
	}
	while (SQLFetch(Statement) == SQL_SUCCESS) {
		count++;
		if (SQLGetData(Statement, 1, SQL_CHAR, name, 20, 0)
		    != SQL_SUCCESS) {
			AB_ERROR(("Get row %d, name column failed", count));
			goto odbcfail;
		}
		if (SQLGetData(Statement, 2, SQL_GUID, &sqlguid, 16, 0)
		    != SQL_SUCCESS) {
			AB_ERROR(("Get row %d, guid column failed", count));
			goto odbcfail;
		}

		AB_PRINT(("%-10s %08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			  name,
			  (int) (sqlguid.Data1), sqlguid.Data2,
			  sqlguid.Data3, sqlguid.Data4[0], sqlguid.Data4[1],
			  sqlguid.Data4[2], sqlguid.Data4[3], sqlguid.Data4[4],
			  sqlguid.Data4[5], sqlguid.Data4[6], sqlguid.Data4[7]));
	}

	/*
	 * Realloc cursor handle - (Windows ODBC considers it an invalid cursor
	 * state if we try SELECT again).
	 */
	if (SQLFreeStmt(Statement, SQL_CLOSE) != SQL_SUCCESS) {
		AB_ERROR(("Free statement failed (5)"));
		goto odbcfail;
	}
	if (SQLAllocHandle(SQL_HANDLE_STMT, Connection, &Statement)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLAllocStmt failed(1)"));
		goto odbcfail;
	}

	/*
	 * Now retrieve rows via stored procedure passing GUID as param.
	 */
	AB_PRINT(("retrieving name and guid"));

	strcpy((char *) (queryString), "{call GetGUIDRows(?)}");
	lenOrInd = SQL_NTS;
	strcpy((char *) (guid), "87654321-4321-4321-4321-123456789abc");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_GUID, 0, 0, guid, 0, &lenOrInd)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLBindParameter failed"));
		goto odbcfail;
	}
	status = SQLExecDirect(Statement, queryString, SQL_NTS);
	if (status != SQL_SUCCESS) {
		AB_ERROR(("SELECT failed"));
		goto odbcfail;
	}
	while (SQLFetch(Statement) == SQL_SUCCESS) {
		count++;
		if (SQLGetData(Statement, 1, SQL_CHAR, name, 20, 0)
		    != SQL_SUCCESS) {
			AB_ERROR(("Get row %d, name column failed", count));
			goto odbcfail;
		}
		if (SQLGetData(Statement, 2, SQL_CHAR, guid, 37, 0)
		    != SQL_SUCCESS) {
			AB_ERROR(("Get row %d, guid column failed", count));
			goto odbcfail;
		}

		AB_PRINT(("%-10s %s", name, guid));
	}

	/*
	 * Realloc cursor handle - (Windows ODBC considers it an invalid cursor
	 * state after a previous SELECT has occurred).
	 */
	if (SQLFreeStmt(Statement, SQL_CLOSE) != SQL_SUCCESS) {
		AB_ERROR(("Free statement failed (5)"));
		goto odbcfail;
	}
	if (SQLAllocHandle(SQL_HANDLE_STMT, Connection, &Statement)
	    != SQL_SUCCESS) {
		AB_ERROR(("SQLAllocStmt failed(1)"));
		goto odbcfail;
	}

	/* cleanup */
	CommandWithResult(Statement, "DROP PROC GetGUIDRows");

	/* CLOSEDOWN */

	Disconnect();

	AB_FUNCT(("TestRawODBCGuid (out): ok"));
	return TRUE;

      odbcfail:
	DispODBCErrs(Environment, Connection, Statement);
	DispODBCDiags(Statement);
	AB_FUNCT(("TestRawODBCGuid (out): error"));
	return FALSE;
}

/*!
 * Array of tests.
 */
static DbTestEntry _dbTests[] = {
	/* 1 */ {TestRawODBCDirectQuery, "Raw ODBC direct query"},
	/* 2 */ {TestRawODBCPreparedQuery, "Raw ODBC prepared query"},
	/* 3 */ {TestRawODBCGuid, "Raw ODBC GUID"},
	/* end */ {0, 0}
};

static DbTestEntry *tests = _dbTests;

/*!
 * Code to iterate through all tests to run.
 *
 * \return
 *      TRUE if all tests pass, FALSE if any tests fail.
 */
static int
RunTests(void)
{
	unsigned int i;
	unsigned int passes = 0;
	unsigned int fails = 0;

	i = 0;
	while (tests[i].testFn) {
		printf("Running test %2d: %s... ", i + 1, tests[i].description);
		fflush(stdout);
		if (tests[i].testFn()) {
			printf("pass\n");
			passes++;
		} else {
			printf("fail\n");
			fails++;
		}
		i++;
	}

	if (fails == 0) {
		printf("\nAll %d tests passed.\n\n", passes);
	} else {
		printf("\nTest passes: %d, test fails: %d\n\n", passes, fails);
	}

	/* Return TRUE if there are no failures */
	return (!fails);
}

int
main(int argc, char *argv[])
{
	use_odbc_version3 = 1;

	if (RunTests()) {
		return 0;	/* Success */
	} else {
		return 1;	/* Error code */
	}
}

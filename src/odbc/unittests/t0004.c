#include "common.h"

/* Test for SQLMoreResults */

int main( int argc, char * argv[] ) 
{ 
char       buf[128];
SQLINTEGER ind;
	
	setenv("TDSDUMP","",1);

	Connect();

	strcpy(buf,"I don't exist");
	strcpy(buf,"sysobjects");
	ind = strlen(buf);

	if ( SQLBindParameter(Statement,1,SQL_PARAM_INPUT,SQL_C_CHAR,
			SQL_VARCHAR,20,0,buf,128,&ind) != SQL_SUCCESS ) {
		printf( "Unable to bind parameter\n" );
		exit (1);
	}

	if ( SQLPrepare(Statement,"SELECT * FROM sysobjects WHERE name = ?",
				SQL_NTS) != SQL_SUCCESS ) {
		printf( "Unable to prepare statement\n" );
		exit (1);
	}

	if ( SQLExecute(Statement) != SQL_SUCCESS ) {
		printf( "Unable to execute statement\n" );
		exit (1);
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

	Disconnect();

	printf( "Done.\n" );
	return 0;
} 


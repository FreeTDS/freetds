#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "common.h"

/* Test for SQLMoreResults */

static char  software_version[]   = "$Id: t0004.c,v 1.4 2002-10-19 03:02:34 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

int main( int argc, char * argv[] ) 
{ 
char       buf[128];
SQLINTEGER ind;
	
	setenv("TDSDUMP","",1);

	Connect();

	strcpy(buf,"I don't exist");
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

	strcpy(buf,"sysobjects");
	ind = strlen(buf);

	if ( SQLExecute(Statement) != SQL_SUCCESS ) {
		printf( "Unable to execute statement\n" );
		exit (1);
	}

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Data expected\n");
		exit(1);
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


#include "common.h"

HENV                    Environment; 
HDBC                    Connection; 
HSTMT                   Statement; 

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];

int
read_login_info(void)
{
FILE *in;
char line[512];
char *s1, *s2;

	in = fopen("../../../PWD","r");
	if (!in) {
		fprintf(stderr,"Can not open PWD file\n\n");
		return 1;
	}
	while (fgets(line, 512, in)) {
		s1=strtok(line,"=");
		s2=strtok(NULL,"\n");
		if (!s1 || !s2) continue;
		if (!strcmp(s1,"UID")) {
			strcpy(USER,s2);
		} else if (!strcmp(s1,"SRV")) {
			strcpy(SERVER,s2);
		} else if (!strcmp(s1,"PWD")) {
			strcpy(PASSWORD,s2);
		} else if (!strcmp(s1,"DB")) {
			strcpy(DATABASE,s2);
		}
	}
	return 0;
}

void
CheckReturn(void)
{ 
    SQLSMALLINT         handletype; 
    SQLHANDLE           handle; 
    unsigned char       sqlstate[ 6 ]; 
    unsigned char       msg[ 256 ]; 
  

        if( Statement != NULL ) { 
            handletype = SQL_HANDLE_STMT; 
            handle = Statement; 
        } else if( Connection != NULL ) { 
            handletype = SQL_HANDLE_DBC; 
            handle = Connection; 
        } else { 
            handletype = SQL_HANDLE_ENV; 
            handle = Environment; 
        } 
        SQLGetDiagRec( handletype, handle, 1, sqlstate, NULL, msg, sizeof( msg ), NULL ); 
        printf( "SQL error %s -- %s\n", sqlstate, msg ); 
        exit( 1 );  

} 


int
Connect(void)
{

int res; 


SQLCHAR command[512]; 

	if (read_login_info())
		exit (1);

    if( SQLAllocEnv( &Environment ) != SQL_SUCCESS ) { 
        printf( "Unable to allocate env\n" ); 
        exit( 1 ); 
    } 
    if( SQLAllocConnect( Environment, &Connection ) != SQL_SUCCESS ) { 
        printf( "Unable to allocate connection\n" ); 
        SQLFreeEnv( Environment ); 
        exit( 1 ); 
    } 
    printf( "odbctest\n--------\n\n");
    printf( "connection parameters:\nserver:   '%s'\nuser:     '%s'\npassword: '%s'\ndatabase: '%s'\n", 
             SERVER, USER, "????" /* PASSWORD */, DATABASE); 

    res =  SQLConnect( Connection, 
                    SERVER, SQL_NTS, 
                    USER, SQL_NTS, 
                    PASSWORD, SQL_NTS ); 
    if( res != SQL_SUCCESS ) { 
        printf( "Unable to open data source (ret=%d)\n", res ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    if( SQLAllocStmt( Connection, &Statement ) != SQL_SUCCESS ) { 
        printf( "Unable to allocate statement\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    sprintf(command,"use %s", DATABASE);
    printf("%s\n",command);

    if( SQLExecDirect( Statement, command, SQL_NTS ) 
        != SQL_SUCCESS ) { 
        printf( "Unable to execute statement\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 
    return 0;
}

int
Disconnect(void)
{
    SQLDisconnect( Connection ); 
    SQLFreeConnect( Connection ); 
    SQLFreeEnv( Environment ); 
    return 0;
} 

void
Command(HSTMT stmt, const char *command)
{
	printf("%s\n",command);
	if( SQLExecDirect( stmt, (SQLCHAR *) command, SQL_NTS )
		!= SQL_SUCCESS ) {
		printf( "Unable to execute statement\n" );
		CheckReturn();
		exit( 1 );
	}
}


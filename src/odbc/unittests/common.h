#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sqltypes.h>
#include <sql.h>
#include <sqlext.h>

extern HENV                    Environment; 
extern HDBC                    Connection; 
extern HSTMT                   Statement; 

extern char USER[512];
extern char SERVER[512];
extern char PASSWORD[512];
extern char DATABASE[512];

int read_login_info();

void CheckReturn(); 

int Connect(); 

int Disconnect();


#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sybfront.h>
#include <sybdb.h>

#include "freebcp.h"

void pusage();
int process_parameters(int , char **, struct pd *);
int login_to_database(struct pd *, DBPROCESS **);

int err_handler();
int msg_handler();


main (int argc, char **argv)
{

PARAMDATA        params;
DBPROCESS       *dbproc;

    tdsdump_open(NULL);

    memset(&params, '\0', sizeof(PARAMDATA));

    if (process_parameters(argc, argv, &params ) == FALSE )
    {
       pusage();
       exit(1);
    }

    if (login_to_database(&params, &dbproc ) == FALSE )
    {
       exit(1);
    }


    dbproc->firstrow = params.firstrow;
    dbproc->lastrow  = params.lastrow;

    if (strcmp(params.dbdirection,"in") == 0)
    {
       if (params.cflag)              /* character format file */
       {
          file_character(&params, dbproc, DB_IN );
       }
       else if (params.nflag)         /* native format file    */
            {
                file_native(&params, dbproc, DB_IN );
            }
            else if (params.fflag)    /* formatted file        */
                 {
                      file_formatted(&params, dbproc, DB_IN );
                 }
                 else
                 {
                      exit(1);
                 }
     
    }
    else
    {
       if (params.cflag)              /* character format file */
       {
          file_character(&params, dbproc, DB_OUT );
       }
       else if (params.nflag)         /* native format file    */
            {
                file_native(&params, dbproc, DB_OUT );
            }
            else if (params.fflag)    /* formatted file        */
                 {
                      file_formatted(&params, dbproc, DB_OUT );
                 }
                 else
                 {
                      exit(1);
                 }
     
    }

}


int process_parameters(int argc, char **argv, PARAMDATA *pdata )
{
int  state;
int i ;

char arg[FILENAME_MAX + 1];


    if (argc < 6)
       return(FALSE);

    /* set some defaults */

    pdata->firstrow    = 0;
    pdata->lastrow     = 0;
    pdata->batchsize   = 1000;
    pdata->maxerrors   = 10;

    /* argument 1 - the database object */

    pdata->dbobject = malloc(strlen(argv[1] + 1));
    if (pdata->dbobject != (char *) NULL)
       strcpy(pdata->dbobject, argv[1]);

    strcpy(pdata->dbdirection, argv[2]);

    if (strcmp(pdata->dbdirection, "in") != 0 && strcmp(pdata->dbdirection, "out") != 0 )
    {
       fprintf(stderr,"Copy direction must be either 'in' or 'out'.\n");
       return(FALSE);
    }

    strcpy(pdata->hostfilename, argv[3]);

    /* get the rest of the arguments */

    state = GET_NEXTARG;

    for (i = 4; i < argc ; i++)
    {
        strcpy(arg, argv[i]);

        switch(state) {

           case GET_NEXTARG : 

                if (arg[0] != '-')
                   return(FALSE);

                switch (arg[1]) {

                   case 'm': 
                        pdata->mflag++;
                        if (strlen(arg) > 2)
                           pdata->maxerrors = atoi(&arg[2]);
                        else
                           state = GET_MAXERRORS;
                        break;
                   case 'f': 
                        pdata->fflag++;
                        if (strlen(arg) > 2)
                           strcpy(pdata->formatfile,&arg[2]);
                        else
                           state = GET_FORMATFILE;
                        break;
                   case 'e': 
                        pdata->eflag++;
                        if (strlen(arg) > 2)
                           strcpy(pdata->errorfile,&arg[2]);
                        else
                           state = GET_ERRORFILE;
                        break;
                   case 'F': 
                        pdata->Fflag++;
                        if (strlen(arg) > 2)
                           pdata->firstrow = atoi(&arg[2]);
                        else
                           state = GET_FIRSTROW;
                        break;
                   case 'L': 
                        pdata->Lflag++;
                        if (strlen(arg) > 2)
                           pdata->lastrow = atoi(&arg[2]);
                        else
                           state = GET_LASTROW;
                        break;
                   case 'b': 
                        pdata->bflag++;
                        if (strlen(arg) > 2)
                           pdata->batchsize = atoi(&arg[2]);
                        else
                           state = GET_BATCHSIZE;
                        break;
                   case 'n':
                        pdata->nflag++;
                        break;
                   case 'c':
                        pdata->cflag++;
                        break;
                   case 't': 
                        pdata->tflag++;
                        if (strlen(arg) > 2)
                        {
                           pdata->fieldterm = malloc(strlen(arg));
                           strcpy(pdata->fieldterm,&arg[2]);
                        }
                        else
                           state = GET_FIELDTERM;
                        break;
                   case 'r': 
                        pdata->rflag++;
                        if (strlen(arg) > 2)
                        {
                           pdata->rowterm = malloc(strlen(arg));
                           strcpy(pdata->rowterm,&arg[2]);
                        }
                        else
                           state = GET_ROWTERM;
                        break;
                   case 'U': 
                        pdata->Uflag++;
                        if (strlen(arg) > 2)
                        {
                           pdata->user = malloc(strlen(arg));
                           strcpy(pdata->user,&arg[2]);
                        }
                        else
                           state = GET_USER;
                        break;
                   case 'P': 
                        pdata->Pflag++;
                        if (strlen(arg) > 2)
                        {
                           pdata->pass = malloc(strlen(arg));
                           strcpy(pdata->pass,&arg[2]);
                        }
                        else
                           state = GET_PASS;
                        break;
                   case 'I': 
                        pdata->Iflag++;
                        if (strlen(arg) > 2)
                           strcpy(pdata->interfacesfile,&arg[2]);
                        else
                           state = GET_INTERFACESFILE;
                        break;
                   case 'S': 
                        pdata->Sflag++;
                        if (strlen(arg) > 2)
                        {
                           pdata->server = malloc(strlen(arg));
                           strcpy(pdata->server,&arg[2]);
                        }
                        else
                           state = GET_SERVER;
                        break;
                   default:
                        return(FALSE);

                }
                break;
           case GET_MAXERRORS:
                pdata->maxerrors = atoi(arg);
                state = GET_NEXTARG;
                break;
           case GET_FORMATFILE:
                strcpy(pdata->formatfile,arg);
                state = GET_NEXTARG;
                break;
           case GET_ERRORFILE:
                strcpy(pdata->errorfile,arg);
                state = GET_NEXTARG;
                break;
           case GET_FIRSTROW:
                pdata->firstrow = atoi(arg);
                state = GET_NEXTARG;
                break;
           case GET_LASTROW:
                pdata->lastrow = atoi(arg);
                state = GET_NEXTARG;
                break;
           case GET_BATCHSIZE:
                pdata->batchsize = atoi(arg);
                state = GET_NEXTARG;
                break;
           case GET_FIELDTERM:
                pdata->fieldterm = malloc(strlen(arg) + 1);
                strcpy(pdata->fieldterm,arg);
                state = GET_NEXTARG;
                break;
           case GET_ROWTERM:
                pdata->rowterm = malloc(strlen(arg) + 1);
                strcpy(pdata->rowterm,arg);
                state = GET_NEXTARG;
                break;
           case GET_USER:
                pdata->user = malloc(strlen(arg) + 1);
                strcpy(pdata->user,arg);
                state = GET_NEXTARG;
                break;
           case GET_PASS:
                pdata->pass = malloc(strlen(arg) + 1);
                strcpy(pdata->pass,arg);
                state = GET_NEXTARG;
                break;
           case GET_INTERFACESFILE:
                strcpy(pdata->interfacesfile,arg);
                state = GET_NEXTARG;
                break;
           case GET_SERVER:
                pdata->server = malloc(strlen(arg) + 1);
                strcpy(pdata->server,arg);
                state = GET_NEXTARG;
                break;

           default:
                break;

       }
   }
   /* these must be specified */
   if ( !pdata->Uflag || !pdata->Pflag || !pdata->Sflag )
      return(FALSE);

   /* only one of these can be specified */
   if ( pdata->cflag + pdata->nflag + pdata->fflag  != 1)
      return(FALSE);

   /* character mode file */
   if (pdata->cflag)
   {
      /* Fill in some default values */

      if (!pdata->tflag) /* field terminator not specified */
      {
         pdata->fieldterm = malloc(2);
         strcpy(pdata->fieldterm,"\t");
      }
      if (!pdata->rflag) /* row terminator not specified */
      {
         pdata->rowterm = malloc(2);
         strcpy(pdata->rowterm,"\n");
      }
   }

   return(TRUE);
   
}

int login_to_database(PARAMDATA *pdata, DBPROCESS **dbproc)
{
    LOGINREC        *login;

    /* Initialize DB-Library. */

    if (dbinit() == FAIL)
        return(FALSE);

    /* Install the user-supplied error-handling and message-handling
    ** routines. They are defined at the bottom of this source file.
    */

    dberrhandle(err_handler);
    dbmsghandle(msg_handler);
    
    /* Allocate and initialize the LOGINREC structure to be used
    ** to open a connection to SQL Server.
    */

    login = dblogin();

    DBSETLUSER(login, pdata->user);
    DBSETLPWD(login,  pdata->pass);
    DBSETLAPP(login, "FreeBCP");

    /* Enable bulk copy for this connection. */

    BCP_SETL(login, TRUE);
    
    /*
    ** Get a connection to the database.
    */

    if ((*dbproc = dbopen(login, pdata->server)) == (DBPROCESS *) NULL)
    {
        fprintf(stderr, "Can't connect to server.\n");
        return(FALSE);
    }

    return(TRUE);

}

int file_character(PARAMDATA *pdata, DBPROCESS *dbproc, DBINT dir )
{
DBINT   li_rowsread = 0;
char ls_command[256];
int  i;
int  li_direction = 0;
int li_numcols = 0;


     sprintf ( ls_command, "select * from %s where 1=2", pdata->dbobject );

     if (dbcmd (dbproc, ls_command) == FAIL )
     {
          printf("dbcmd failed\n");
          return FALSE;
     }

     if (dbsqlexec (dbproc) == FAIL)
     {
          printf("dbsqlexec failed\n");
          return FALSE;
     }

     while (NO_MORE_RESULTS != dbresults (dbproc));
     {
        
        if (0 == (li_numcols = dbnumcols (dbproc)))
        {
             printf("Error in dbnumcols\n");
             return FALSE;
        }

        if (FAIL == bcp_init (dbproc, pdata->dbobject, pdata->hostfilename, (char *)NULL , dir))
           return FALSE;

        if (bcp_columns(dbproc, li_numcols) == FAIL)
        {
            printf("Error in bcp_columns.\n");
            return FALSE;
        }

        for (i = 1; i <= li_numcols - 1; i++ )
        {
             if (bcp_colfmt(dbproc,i, SYBCHAR, 0, -1,(BYTE *)"\t",sizeof(char) ,i) == FAIL)
             {
                   printf("Error in bcp_colfmt col %d\n",i);
                   return FALSE;
             }
        }

        if (bcp_colfmt(dbproc,li_numcols,SYBCHAR,0, -1,(BYTE *)"\n",sizeof(char) ,li_numcols) == FAIL)
        {
              printf("Error in bcp_colfmt col %d\n",li_numcols);
              return FALSE;
        }
     }

     bcp_control(dbproc, BCPBATCH, pdata->batchsize);

     printf("\nStarting copy...\n\n");

     if (FAIL == bcp_exec (dbproc, &li_rowsread))
     {
        fprintf(stderr,"bcp copy %s failed\n", (dir == DB_IN) ? "in" : "out" );
        return FALSE;
     }

     printf("%d rows copied.\n", li_rowsread);

     return TRUE;
}

int file_native(PARAMDATA *pdata, DBPROCESS *dbproc, DBINT dir )
{
DBINT   li_rowsread = 0;
char ls_command[256];
int  i;
int  li_direction = 0;
int  li_numcols = 0;
int  li_coltype;
int  li_collen;


     sprintf ( ls_command, "select * from %s where 1=2", pdata->dbobject );

     if (dbcmd (dbproc, ls_command) == FAIL )
     {
          printf("dbcmd failed\n");
          return FALSE;
     }

     if (dbsqlexec (dbproc) == FAIL)
     {
          printf("dbsqlexec failed\n");
          return FALSE;
     }

     while (NO_MORE_RESULTS != dbresults (dbproc));
     {
        
        if (0 == (li_numcols = dbnumcols (dbproc)))
        {
             printf("Error in dbnumcols\n");
             return FALSE;
        }

        if (FAIL == bcp_init (dbproc, pdata->dbobject, pdata->hostfilename, (char *)NULL , dir))
           return FALSE;

        if (bcp_columns(dbproc, li_numcols) == FAIL)
        {
            printf("Error in bcp_columns.\n");
            return FALSE;
        }

        for (i = 1; i <= li_numcols ; i++ )
        {
             li_coltype = dbcoltype(dbproc, i); 
             li_collen  = dbcollen (dbproc, i); 

             if (bcp_colfmt(dbproc,i, li_coltype, -1, -1, (BYTE *)NULL , -1 ,i) == FAIL)
             {
                   printf("Error in bcp_colfmt col %d\n",i);
                   return FALSE;
             }
        }

     }

     printf("\nStarting copy...\n\n");


     if (FAIL == bcp_exec (dbproc, &li_rowsread))
     {
        fprintf(stderr,"bcp copy %s failed\n", (dir == DB_IN) ? "in" : "out" );
        return FALSE;
     }

     printf("%d rows copied.\n", li_rowsread);

     return TRUE;
}

int file_formatted(PARAMDATA *pdata, DBPROCESS *dbproc, DBINT dir )
{

int li_rowsread;

     if (FAIL == bcp_init (dbproc, pdata->dbobject, pdata->hostfilename, (char *)NULL , dir))
        return FALSE;

     if (FAIL == bcp_readfmt (dbproc, pdata->formatfile))
        return FALSE;

     printf("\nStarting copy...\n\n");


     if (FAIL == bcp_exec (dbproc, &li_rowsread))
     {
        fprintf(stderr,"bcp copy %s failed\n", (dir == DB_IN) ? "in" : "out" );
        return FALSE;
     }

     printf("%d rows copied.\n", li_rowsread);

     return TRUE;
}

void pusage()
{
   fprintf(stderr,"usage: bcp [[database_name.]owner.]table_name {in | out} datafile\n");
   fprintf(stderr,"        [-m maxerrors] [-f formatfile] [-e errfile]\n");
   fprintf(stderr,"        [-F firstrow] [-L lastrow] [-b batchsize]\n");
   fprintf(stderr,"        [-n] [-c] [-t field_terminator] [-r row_terminator]\n");
   fprintf(stderr,"        [-U username] [-P password] [-I interfaces_file] [-S server]\n");
   fprintf(stderr,"        [-a display_charset] [-q datafile_charset] [-z language] [-v]\n");
   fprintf(stderr,"        [-A packet size] [-J client character set]\n");
   fprintf(stderr,"        [-T text or image size] [-E] [-N] [-X]  [-y sybase_dir]\n");
   fprintf(stderr,"        [-Mlabelname labelvalue] [-labeled]\n");
}

int err_handler(dbproc, severity, dberr, oserr, dberrstr, oserrstr)
DBPROCESS       *dbproc;
int             severity;
int             dberr;
int             oserr;
char            *dberrstr;
char            *oserrstr;
{

    if (dberr)
    {
       fprintf(stderr,"Msg %ld, Level %d\n", dberr, severity);
       fprintf(stderr,"%s\n\n", dberrstr);
    }

    else
    {
       fprintf(stderr,"DB-LIBRARY error:\n\t");
       fprintf(stderr,"%s\n", dberrstr);
    }

    return(INT_CANCEL);
}

int msg_handler(dbproc, msgno, msgstate, severity, msgtext, 
                srvname, procname, line)

DBPROCESS       *dbproc;
DBINT           msgno;
int             msgstate;
int             severity;
char            *msgtext;
char            *srvname;
char            *procname;
DBUSMALLINT     line;

{
    /*
    ** If it's a database change message, we'll ignore it.
    ** Also ignore language change message.
    */
    if (msgno == 5701 || msgno == 5703)
        return(0);

    printf ("Msg %ld, Level %d, State %d\n", 
            msgno, severity, msgstate);

    if (strlen(srvname) > 0)
        printf ("Server '%s', ", srvname);
    if (strlen(procname) > 0)
        printf ("Procedure '%s', ", procname);
    if (line > 0)
        printf ("Line %d", line);

    printf("\n\t%s\n", msgtext);

    return(0);
}


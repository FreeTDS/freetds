#include <stdio.h>
#include <string.h>
#include <tds.h>

static char  software_version[]   = "$Id: common.c,v 1.1 2001-10-12 23:29:03 brianb Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];

int read_login_info()
{
   FILE *in;
   char line[512];
   char *s1, *s2;

   in = fopen("../../../PWD","r");
   if (!in) {
      fprintf(stderr,"Can not open PWD file\n\n");
      return TDS_FAIL;
   }

   while (fgets(line, 512, in)) {
      s1=strtok(line,"=");
      s2=strtok(NULL,"\n");
      if (!s1 || !s2)			{ continue; }
      if (!strcmp(s1,"UID"))		{ strcpy(USER,s2); }
      else if (!strcmp(s1,"SRV"))	{ strcpy(SERVER,s2); }
      else if (!strcmp(s1,"PWD"))	{ strcpy(PASSWORD,s2); }
      else if (!strcmp(s1,"DB"))	{ strcpy(DATABASE,s2); }
   }
   return TDS_SUCCEED;
}


int try_tds_login(
   TDSLOGIN  **login,
   TDSSOCKET **tds,
   char *appname,
   int verbose)
{
   if (verbose)	{ fprintf(stdout, "Entered tds_try_login()\n"); }
   if (! login) {
      fprintf(stderr, "Invalid TDSLOGIN**\n");
      return TDS_FAIL;
   }
   if (! tds) {
      fprintf(stderr, "Invalid TDSSOCKET**\n");
      return TDS_FAIL;
   }

   if (verbose)	{ fprintf(stdout, "Trying read_login_info()\n"); }
   read_login_info();

   if (verbose)	{ fprintf(stdout, "Setting login parameters\n"); }
   *login = tds_alloc_login();
   if (! *login) {
      fprintf(stderr, "tds_alloc_login() failed.\n");
      return TDS_FAIL;
   }
   tds_set_passwd(*login, PASSWORD);
   tds_set_user(*login, USER);
   tds_set_app(*login, appname);
   tds_set_host(*login, "myhost");
   tds_set_library(*login, "TDS-Library");
   tds_set_server(*login, SERVER);
   tds_set_charset(*login, "iso_1");
   tds_set_language(*login, "us_english");
   tds_set_packet(*login, 512);
  
   if (verbose)	{ fprintf(stdout, "Connecting to database\n"); }
   *tds = tds_connect(*login);
   if (! *tds) {
      fprintf(stderr, "tds_connect() failed\n");
      return TDS_FAIL;
   }

   return TDS_SUCCEED;
}


/* Note that this always suceeds */
int try_tds_logout(
   TDSLOGIN  *login,
   TDSSOCKET *tds,
   int verbose)
{
   if (verbose)	{ fprintf(stdout, "Entered tds_try_logout()\n"); }
   tds_free_socket(tds);
   tds_free_login(login);
   return TDS_SUCCEED;
}

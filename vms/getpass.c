/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003  Craig A. Berry	craigberry@mac.com	23-JAN-2003
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <descrip.h>
#include <iodef.h>
#include <lib$routines.h>
#include <starlet.h>
#include <stsdef.h>
#include <syidef.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char software_version[] = "$Id: getpass.c,v 1.1 2003-03-01 12:48:36 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };
static char passbuff[128];

#if 0
int main() {
   char *pass = getpass("pWord: ");
   printf("The password I got was >>>%s<<<\n", pass);
}
#endif

/**
 * A simple getpass() implementation for VMS based on sys$qiow.
 * This attempts to follow the documented characteristics of the
 * function on other platforms, including the 128-byte static
 * buffer for the password.
 */

char *getpass( const char* prompt ) {

  unsigned long tmo_item = SYI$_LGI_PWD_TMO;
  unsigned long timeout_secs = 0;
  unsigned long ttchan = 0;
  unsigned short iosb[4];
  unsigned long status = 0;
  $DESCRIPTOR(ttdsc, "SYS$COMMAND:");
  char *retval = NULL;

  /* The prompt is expected to provide its own leading newline */
  char *myprompt = malloc(strlen(prompt)+1);
  if (prompt[0] != '\n') sprintf(myprompt, "\n%s", prompt);

  /* How long should we wait before giving up? */
  status = lib$getsyi(&tmo_item, &timeout_secs);
  if (!$VMS_STATUS_SUCCESS(status)) timeout_secs = 30;

  /* Make sure the password buffer and iosb are zeroed */
  memset(passbuff, 0, sizeof(passbuff));
  memset(iosb, 0, sizeof(iosb));

  /* assign a channel and read with prompt */
  status = sys$assign(&ttdsc, &ttchan, 0, 0, 0);
  if ($VMS_STATUS_SUCCESS(status)) {
      status = sys$qiow(0,
                        ttchan,
                        IO$_READPROMPT | IO$M_NOECHO | IO$M_TIMED,
                        &iosb,
                        0, 0,
                        passbuff,
                        sizeof(passbuff)-1,
                        timeout_secs, 
			0,
                        myprompt,
                        strlen(myprompt));

      if ($VMS_STATUS_SUCCESS(status)) status = iosb[0];
      if ($VMS_STATUS_SUCCESS(status)) {
          passbuff[iosb[1]] = '\0';
          retval = passbuff;
          fputc('\n', stderr);  /* kludge to get back to left margin */
      }
      else {
          fprintf(stderr, "\nError reading command input\n");
      }
      sys$dassgn(ttchan);
  }

  free(myprompt);
  return(retval);
}

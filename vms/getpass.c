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

#include <dcdef.h>
#include <descrip.h>
#include <dvidef.h>
#include <iodef.h>
#include <lib$routines.h>
#include <libclidef.h>
#include <smg$routines.h>
#include <smgdef.h>
#include <ssdef.h>
#include <starlet.h>
#include <stsdef.h>
#include <syidef.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char software_version[] = "$Id: getpass.c,v 1.3 2003-05-20 11:36:34 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };
static char passbuff[128];

/* 
 * A collection of assorted UNIXy input functions for VMS.  The core
 * functionality is provided by readpassphrase(), and the general 
 * requirements come mostly from Todd Miller's OpenBSD code but are 
 * completely reimplemented using native services.  
 *
 * There are simple getpass() and readline() implementations wrapped
 * around readpassphrase(), the main difference being that getpass()
 * suppresses echoing whereas readline() enables echoing and suppresses
 * timeouts.  There is also an add_history() stub; echoed lines are
 * already stored in the command recall buffer by SMG$READ_COMPOSED_LINE.
 */

#define MY_PASSWORD_LEN 1024
#define RECALL_SIZE     50	/* Lines in recall buffer. */
#define DEFAULT_TIMEOUT 30	/* Seconds to wait for user input. */

/* The following flag values are taken directly from the OpenBSD version 
 * of getpassphrase.
 */
#define RPP_ECHO_OFF    0x00	/* Turn off echo (default). */
#define RPP_ECHO_ON     0x01	/* Leave echo on. */
#define RPP_REQUIRE_TTY 0x02	/* Fail if there is no tty. */
#define RPP_FORCELOWER  0x04	/* Force input to lower case. */
#define RPP_FORCEUPPER  0x08	/* Force input to upper case. */
#define RPP_SEVENBIT    0x10	/* Strip the high bit from input. */

/* Flags defined below are VMS-specific.  Use the high byte to minimize
 * potential conflicts.
 */
#define RPP_TIMEOUT_OFF 0x01000000	/* Wait indefinitely for input. */

char *
readpassphrase(const char *prompt, char *pbuf, size_t buflen, int flags)
{

	static unsigned long keyboard_id, keytable_id = 0;
	unsigned long ctrl_mask, saved_ctrl_mask = 0;
	int timeout_secs = 0;
	int *timeout_ptr = NULL;
	unsigned long dvi_item, ttdevclass, status = 0;
	unsigned short iosb[4];
	unsigned short ttchan, result_len = 0;

	$DESCRIPTOR(ttdsc, "SYS$COMMAND:");
	$DESCRIPTOR(pbuf_dsc, "");
	$DESCRIPTOR(prompt_dsc, "");
	char *retval = NULL;
	char *myprompt = NULL;

	if (pbuf == NULL || buflen == 0) {
		errno = EINVAL;
		return NULL;
	}
	bzero(pbuf, buflen);
	pbuf_dsc.dsc$a_pointer = pbuf;
	pbuf_dsc.dsc$w_length = buflen - 1;

	/*
	 * Find out if SYS$COMMAND is a terminal.
	 */
	dvi_item = DVI$_DEVCLASS;
	status = LIB$GETDVI(&dvi_item, 0, &ttdsc, &ttdevclass);
	if (!$VMS_STATUS_SUCCESS(status)) {
		errno = EVMSERR;
		vaxc$errno = status;
		return NULL;
	}

	/*
	 * If it's not a terminal and only reading from a terminal is allowed, we
	 * stop here.  
	 */
	if ((ttdevclass != DC$_TERM) && (flags & RPP_REQUIRE_TTY)) {
		errno = ENOTTY;
		return NULL;
	}

	/* 
	 * The prompt is expected to provide its own leading newline.
	 */
	myprompt = malloc(strlen(prompt) + 1);
	if (myprompt == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	sprintf(myprompt, "\n%s", prompt);
	prompt_dsc.dsc$a_pointer = myprompt;
	prompt_dsc.dsc$w_length = strlen(myprompt);

	/* Disable Ctrl-T and Ctrl-Y */
	ctrl_mask = LIB$M_CLI_CTRLT | LIB$M_CLI_CTRLY;
	status = LIB$DISABLE_CTRL(&ctrl_mask, &saved_ctrl_mask);
	if (!$VMS_STATUS_SUCCESS(status)) {
		errno = EVMSERR;
		vaxc$errno = status;
		free(myprompt);
		return NULL;
	}

	/* 
	 * Unless timeouts are disabled, find out how long should we wait for input
	 * before giving up.
	 */
	if (!(flags & RPP_TIMEOUT_OFF)) {
		unsigned long tmo_item = SYI$_LGI_PWD_TMO;

		status = LIB$GETSYI(&tmo_item, &timeout_secs);
		if (!$VMS_STATUS_SUCCESS(status))
			timeout_secs = DEFAULT_TIMEOUT;
		timeout_ptr = &timeout_secs;
	}

	if (!(flags & RPP_ECHO_ON) && (ttdevclass == DC$_TERM)) {
		/* 
		 * If we are suppressing echoing, get a line of input with $QIOW.  
		 * Non-echoed lines are not stored for recall.  (The same thing
		 * could be done with SMG but would require maintenance of a virtual 
		 * display and pasteboard.)
		 */
		status = SYS$ASSIGN(&ttdsc, &ttchan, 0, 0, 0);
		if ($VMS_STATUS_SUCCESS(status)) {

			unsigned long qio_func = IO$_READPROMPT | IO$M_NOECHO;

			if (!(flags & RPP_TIMEOUT_OFF))
				qio_func |= IO$M_TIMED;
			bzero(iosb, sizeof(iosb));

			status = SYS$QIOW(0,
					  (unsigned long) ttchan,
					  qio_func, &iosb, 0, 0, pbuf, buflen - 1, timeout_secs, 0, myprompt, strlen(myprompt));

			if ($VMS_STATUS_SUCCESS(status)) {
				status = iosb[0];
				result_len = iosb[1];	/* bytes actually read */
			}
			(void) SYS$DASSGN(ttchan);
		}
	} else {
		/* 
		 * We are not suppressing echoing because echoing has been explicitly 
		 * enabled and/or we are not reading from a terminal.  In this case we 
		 * use SMG, which will store commands for recall.  The virtual keyboard 
		 * and key table are static and will only be created if we haven't been 
		 * here before.
		 */
		status = SS$_NORMAL;
		if (keyboard_id == 0) {
			unsigned char recall_size = RECALL_SIZE;

			status = SMG$CREATE_VIRTUAL_KEYBOARD(&keyboard_id, &ttdsc, 0, 0, &recall_size);
		}
		if ($VMS_STATUS_SUCCESS(status) && keytable_id == 0) {
			status = SMG$CREATE_KEY_TABLE(&keytable_id);
		}

		if ($VMS_STATUS_SUCCESS(status)) {
			status = SMG$READ_COMPOSED_LINE(&keyboard_id,
							&keytable_id, &pbuf_dsc, &prompt_dsc, &result_len, 0, 0, 0, timeout_ptr);
		}
	}

	/* 
	 * Process return value from SYS$QIOW or SMG$READ_COMPOSED_LINE.
	 */
	switch (status) {
	case SS$_TIMEOUT:
		errno = ETIMEDOUT;
		break;
	case SMG$_EOF:
		if (result_len != 0) {
			status = SS$_NORMAL;
		}
		/* fall through */
	default:
		if ($VMS_STATUS_SUCCESS(status)) {
			int i;

			if (flags & RPP_FORCELOWER) {
				for (i = 0; i < result_len; i++)
					pbuf[i] = tolower(pbuf[i]);
			}
			if (flags & RPP_FORCEUPPER) {
				for (i = 0; i < result_len; i++)
					pbuf[i] = toupper(pbuf[i]);
			}
			if (flags & RPP_SEVENBIT) {
				for (i = 0; i < result_len; i++)
					pbuf[i] &= 0x7f;
			}
			pbuf[result_len] = '\0';
			retval = pbuf;
		} else {
			errno = EVMSERR;
			vaxc$errno = status;
		}
	}			/* end switch */

	free(myprompt);

	/* 
	 * Reenable previous control processing.
	 */
	status = LIB$ENABLE_CTRL(&saved_ctrl_mask);

	if (!$VMS_STATUS_SUCCESS(status)) {
		errno = EVMSERR;
		vaxc$errno = status;
		return NULL;
	}

	return retval;

}				/* getpassphrase */


static char passbuf[MY_PASSWORD_LEN + 1];

char *
getpass(const char *prompt)
{

	bzero(passbuf, sizeof(passbuf));	/* caller should do this again */

	return (readpassphrase(prompt, passbuf, sizeof(passbuf), RPP_ECHO_OFF)
		);

}				/* getpass */

char *
readline(char *prompt)
{

	char *buf = NULL;
	char *s = readpassphrase((const char *) prompt, passbuf, sizeof(passbuf),
				 RPP_ECHO_ON | RPP_TIMEOUT_OFF);

	if (s != NULL) {
		buf = (char *) malloc(strlen(s) + 1);
		strcpy(buf, s);
	}
	return buf;

}				/* readline */

void
add_history(const char *s)
{
}

/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: util.c,v 1.24 2002-10-26 06:37:45 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* for now all messages go to the log */
int g_debug_lvl = 99;
int g_append_mode = 0;
static char *g_dump_filename = NULL;
static int   write_dump = 0;      /* is TDS stream debug log turned on? */
static FILE *dumpfile   = NULL;   /* file pointer for dump log          */

void 
tds_set_parent(TDSSOCKET *tds, void *the_parent)
{
	if (tds)
      		tds->parent = the_parent;
}

void *
tds_get_parent(TDSSOCKET *tds)
{
      return(tds->parent);
}
void 
tds_ctx_set_parent(TDSCONTEXT *ctx, void* the_parent)
{
	if (ctx)
      		ctx->parent = the_parent;
}
void *
tds_ctx_get_parent(TDSCONTEXT *ctx)
{
      return(ctx->parent);
}

int tds_swap_bytes(unsigned char *buf, int bytes)
{
unsigned char tmp;
int i;

	/* if (bytes % 2) { return 0 }; */
	for (i=0;i<bytes/2;i++) {
		tmp = buf[i];
		buf[i] = buf[bytes-i-1];
		buf[bytes-i-1]=tmp;
	}
	return bytes;
}

/**
 * Returns the version of the TDS protocol in effect for the link
 * as a decimal integer.  
 *	Typical returned values are 42, 50, 70, 80.
 * Also fills pversion_string unless it is null.
 * 	Typical pversion_string values are "4.2" and "7.0".
 */
int tds_version(TDSSOCKET *tds_socket, char *pversion_string)
{
int iversion = 0;

	if (tds_socket) {
		iversion =  10 * tds_socket->major_version
					+ tds_socket->minor_version;

		if (pversion_string) {
			sprintf (pversion_string, "%d.%d", 
					tds_socket->major_version, 
					tds_socket->minor_version );
		}
	}
	
	return iversion;
}

/* ============================== tdsdump_off() ==============================
 *
 * Def:  temporarily turn off logging.  Note- 
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void
tdsdump_off(void)
{
   write_dump = 0;
} /* tdsdump_off()  */


/* ============================== tdsdump_on() ===============================
 *
 * Def:  turn logging back on.  Note-  You must call tdsdump_open() before 
 *       calling this routine.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void
tdsdump_on(void)
{
	write_dump = 1;
} /* tdsdump_on()  */


/* ============================= tdsdump_open() ==============================
 *
 * Def:  This creates and truncates a human readable dump file for the TDS
 *       traffic.  The name of the file is specified by the filename
 *       parameter.  If that is given as NULL, it opens a
 *       file named "tdsdump.out" in the current directory.
 *
 * Ret:  true iff the file was opened, false if it couldn't be opened.
 *
 * ===========================================================================
 */
int tdsdump_open(const char *filename)
{
int   result;   /* really should be a boolean, not an int */

   tdsdump_close();
   if (filename == NULL || filename[0]=='\0') {
      filename = "tdsdump.out";
   }
   if (g_append_mode) {
	 g_dump_filename = strdup(filename);
      tdsdump_on();
      result = 1;
   } else if (!strcmp(filename,"stdout")) {
      dumpfile = stdout;
      result = 1;
   } else if (!strcmp(filename,"stderr")) {
      dumpfile = stderr;
      result = 1;
   } else if (NULL == (dumpfile = fopen(filename, "w"))) {
      tdsdump_off();
      result = 0;
   } else {
      tdsdump_on();
      result = 1;
   }
   return result;
} /* tdsdump_open()  */

int tdsdump_append(void)
{
int result;

	if (!g_dump_filename) {
		return 0;
	}

	if (!strcmp(g_dump_filename,"stdout")) {
		dumpfile = stdout;
		result = 1;
	} else if (!strcmp(g_dump_filename,"stderr")) {
		dumpfile = stderr;
		result = 1;
	} else if (NULL == (dumpfile = fopen(g_dump_filename, "a"))) {
		result = 0;
	} else {
		result = 1;
	}
	return result;
}


/* ============================= tdsdump_close() =============================
 *
 * Def:  Close the TDS dump log file.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void
tdsdump_close(void)
{
   tdsdump_off();
   if (dumpfile!=NULL && dumpfile != stdout && dumpfile != stderr)
   {
      fclose(dumpfile);
   }
   dumpfile = NULL;
   if (g_dump_filename) {
      free(g_dump_filename);
      g_dump_filename = NULL;
   }
} /* tdsdump_close()  */


/* =========================== tdsdump_dump_buf() ============================
 *
 * Def:  Dump the contents of data into the log file in a human readable
 *       format.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void tdsdump_dump_buf(
   const void    *buf,     /* (I) buffer to dump                      */
   int            length)  /* (I) number of bytes in the buffer       */
{
   int                   i;
   int                   j;
   const int             bytesPerLine = 16;
   const unsigned char  *data         = buf;

   if (write_dump && dumpfile!=NULL)
   {
      for(i=0; i<length; i+=bytesPerLine)
      {
         /*
          * print the offset as a 4 digit hex number
          */
         fprintf(dumpfile, "%04x  ", i);

         /*
          * print each byte in hex
          */
         for(j=0; j<bytesPerLine; j++)
         {
            if (j == bytesPerLine/2) fprintf(dumpfile, " ");
            if (j+i >= length) fprintf(dumpfile, "   ");
            else fprintf(dumpfile, "%02x ", data[i+j]);
         }
         
         /*
          * skip over to the ascii dump column
          */
         fprintf(dumpfile, "  |");

         /*
          * print each byte in ascii
          */
         for(j=i; j<length && (j-i)<bytesPerLine; j++)
         {
            if (j-i == bytesPerLine/2) fprintf(dumpfile, " ");
            fprintf(dumpfile, "%c", (isprint(data[j])) ? data[j] : '.');
         }
         fprintf(dumpfile, "|\n");
      }
      fprintf(dumpfile, "\n");
   }
} /* tdsdump_dump_buf()  */


/* ============================== tdsdump_log() ==============================
 * 
 * Def:  This function write a message to the debug log.  fmt is a printf-like
 *       format string.  It recognizes the following format characters:
 *          d     The next argument is printed a decimal number
 *          p     The pid of the running process
 *          s     The next argument is printed as a character string
 *          L     This doesn't consume any arguments, it simply
 *                prints the current local time.
 *          D     This dumps a buffer in hexadecimal and ascii.
 *                The next argument is a pointer to the buffer
 *                and the argument after that is the number 
 *                of bytes in the buffer.
 * 
 * Ret:  void
 * 
 * ===========================================================================
 */
void tdsdump_log(int debug_lvl, const char *fmt, ...)
{
   int ret = 0;

   if (debug_lvl>g_debug_lvl) 
	return;

   if (g_append_mode) {
      ret = tdsdump_append();
   }
   if (write_dump && dumpfile!=NULL)
   {
      const char     *ptr;

      va_list   ap;
      va_start(ap, fmt);
      
   	 if (g_append_mode) {
          fprintf(dumpfile, "pid: %d:", (int)getpid() );
      }
      for(ptr = fmt; *ptr != '\0'; ptr++)
      {
         if (*ptr == '%')
         {
            ptr++;
            switch(*ptr)
            {
               case 's':
               {
                  char   *s = va_arg(ap, char *);
                  fputs(s, dumpfile);
                  break;
               }
               case 'd':
               {
                  int i = va_arg(ap, int);
                  fprintf(dumpfile, "%d", i);
                  break;
               }
               case 'x':
               {
                  int i = va_arg(ap, int);
                  fprintf(dumpfile, "%x", i);
                  break;
               }
               case 'D':
               {
                  char  *buf = va_arg(ap, char *);
                  int    len = va_arg(ap, int);
                  tdsdump_dump_buf(buf, len);
                  break;
               }
               case 'L': /* current local time */
               {
                  char        buf[128];

                  fputs(tds_timestamp_str(buf, 127), dumpfile);
               }
               default:
               {
                  break;
               }
            }
         }
         else
         {
            fputc(*ptr, dumpfile);
         }
      }
      fflush(dumpfile);
      if (g_append_mode && ret) {
         fclose(dumpfile);
      }
   }
} /* tdsdump_log()  */

int
tds_close_socket(TDSSOCKET *tds)
{
int rc = -1;

        if (!IS_TDSDEAD(tds)) {
                rc = CLOSESOCKET(tds->s);
                tds->s = -1;
        }
        return rc;
}

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

#include <config.h>
#include "tdsutil.h"
#include "tds.h"
#include "tdsconvert.h"
#include <time.h>
#include <assert.h>
#include <errno.h>
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifndef HAVE_ATOLL
static long int
atoll(const char *nptr)
{
	return atol(nptr);
}
#endif

static char  software_version[]   = "$Id: convert.c,v 1.35 2002-08-14 10:14:43 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

struct diglist {
	short int dig;
	short int carried;
	struct diglist *nextptr;
};

typedef unsigned short utf16_t;

static int  _tds_pad_string(char *dest, int destlen);
extern char *tds_numeric_to_string(TDS_NUMERIC *numeric, char *s);
extern char *tds_money_to_string(TDS_MONEY *money, char *s);
static int  string_to_datetime(char *datestr, int desttype, CONV_RESULT *cr );
static int  string_to_numeric(char *instr, CONV_RESULT *cr);
static int  store_hour(char *, char *, struct tds_time *);
static int  store_time(char *, struct tds_time * );
static int  store_yymmdd_date(char *, struct tds_time *);
static int  store_monthname(char *, struct tds_time *);
static int  store_numeric_date(char *, struct tds_time *);
static int  store_mday(char *, struct tds_time *);
static int  store_year(int,  struct tds_time *);
static int  is_timeformat(char *);
static int  is_numeric(char *);
static int  is_alphabetic(char *);
static int  is_ampm(char *);
static int  is_monthname(char *);
static int  is_numeric_dateformat(char *);
static TDS_UINT utf16len(const utf16_t* s);

#define test_alloc(x) {if ((x)==NULL) return TDS_FAIL;}
extern int g__numeric_bytes_per_prec[];

extern int (*g_tds_err_handler)(void*);

static int tds_atoi(const char *buf);
#define atoi(x) tds_atoi((x))

#define IS_TINYINT(x) ( 0 <= (x) && (x) <= 0xff )

int tds_get_conversion_type(int srctype, int colsize)
{
	if (srctype == SYBINTN) {
		if (colsize==8)
			 return SYBINT8;
		else if (colsize==4)
			 return SYBINT4;
		else if (colsize==2)
			 return SYBINT2;
		else if (colsize==1)
			 return SYBINT1;
	} else if (srctype == SYBFLTN) {
		if (colsize==8)
			return SYBFLT8;
		else if (colsize==4)
			return SYBREAL;
	} else if (srctype == SYBDATETIMN) {
		if (colsize==8) 
			return SYBDATETIME;
		else if (colsize==4)
			return SYBDATETIME4;
	} else if (srctype == SYBMONEYN) {
		if (colsize==8) 
			return SYBMONEY;
		else if (colsize==4) 
			return SYBMONEY4;
	}
	return srctype;
}

/* reverted function to 1.12 logic; 1.13 patch was mistaken. July 2002 jkl */
static TDS_INT tds_convert_text(TDSCONTEXT *tds_ctx, int srctype,TDS_CHAR *src,TDS_UINT srclen,
	int desttype,TDS_UINT destlen, CONV_RESULT *cr)
{
int cplen;

	switch(desttype) {
		case SYBTEXT:
		case SYBCHAR:
		case SYBVARCHAR:
			cr->c = malloc(srclen + 1);
			test_alloc(cr->c);
			memset(cr->c, '\0', srclen + 1);
			memcpy(cr->c, src, srclen);
			return srclen;
			break;
	
		default:
			fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
			break;
	}
}

static TDS_INT 
tds_convert_ntext(int srctype,TDS_CHAR *src,TDS_UINT srclen,
      int desttype,TDS_UINT destlen, CONV_RESULT *cr)
{
      /* FIX */
      /* 
       * XXX Limit of 255 + 1 needs to be fixed by determining what the 
       *     real limit of [N][VAR]CHAR columns is.
       * XXX What about NCHAR?  Don't see a constant for it in tds.h.
       * XXX Case for -1 in switch statement because upper levels don't
       *     have a way to bind to wide-character types.
       */
      TDS_UINT i, cplen, char_limit = 256;
      utf16_t* wsrc  = (utf16_t*)src;
      utf16_t* wdest; 
/*
      utf16_t* wdest = (utf16_t*)dest;
*/
      assert(sizeof(utf16_t) == 2);
      switch (desttype) {
              case SYBNVARCHAR:
                      if (destlen > char_limit * sizeof(utf16_t))
                              destlen = char_limit * sizeof(utf16_t);
                      /* Fall through ... */
              case SYBNTEXT:
              case -1:
                      cplen = srclen > destlen ? destlen : srclen;
/*
                      memcpy(dest, src, cplen);
*/
                      if (destlen < srclen + sizeof(utf16_t)) {
                              size_t term_pos  = destlen - sizeof(utf16_t);
                              size_t odd_bytes = term_pos % sizeof(utf16_t);
                              term_pos -= odd_bytes;
                              wdest[term_pos / sizeof(utf16_t)] = 0;
                      }
            else
                              wdest[cplen / sizeof(utf16_t)] = 0;
                      return utf16len(wdest) * 2;
              default:
                      /* Assume caller wants conversion to narrow string. */
                      if (destlen > char_limit && desttype != SYBTEXT)
                              destlen = char_limit;
                      cplen = srclen > destlen ? destlen : srclen;
/*
                      for (i = 0; i < cplen; ++i)
                              dest[i] = (unsigned char)wsrc[i];
                      dest[cplen-1] = 0;
                      return strlen(dest);
*/
      }
      return 0;
}


static TDS_INT 
tds_convert_binary(int srctype,TDS_UCHAR *src,TDS_INT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
int cplen;
int d, s;
TDS_VARBINARY *varbin;
char *c;
char hex2[3];
int  ret;

   switch(desttype) {
      case SYBCHAR:
      case SYBTEXT:
      case SYBVARCHAR:

         /* 2 * source length + 1 for terminator */

         cr->c = malloc((srclen * 2) + 1);
		test_alloc(cr->c);

         c = cr->c;

         for (s = 0; s < srclen; s++) {
             sprintf(hex2, "%02x", src[s]);
             *c++ = hex2[0];
             *c++ = hex2[1];
         }

         *c = '\0';
         return (srclen * 2) + 1;
         break;
      case SYBIMAGE:
      case SYBBINARY:
         cr->ib = malloc(srclen);
		test_alloc(cr->ib);
         memcpy(cr->ib, src, srclen);
         return srclen;
         break;
      case SYBVARBINARY:
         cplen = srclen > 256 ? 256 : srclen;
         cr->vb.len = cplen;
         memcpy(cr->vb.array, src, cplen);
         return cplen;
         break;
      default:
         fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
         return TDS_FAIL;
         break;
   }
   return TDS_SUCCEED;
}

static TDS_INT 
tds_convert_char(int srctype,TDS_CHAR *src, TDS_UINT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{

int           ret;
int           i, j;
unsigned char hex1;
int           inp;

TDS_INT8     mymoney;
TDS_INT      mymoney4;
char         mynumber[39];

char *ptr;
int point_found, places;

   
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
      case SYBNVARCHAR:
      case SYBTEXT:
		 cr->c = malloc(srclen + 1);
		test_alloc(cr->c);
		 memset(cr->c, '\0', srclen + 1);
		 memcpy(cr->c, src, srclen);
         return srclen; 
		 break;

      case SYBBINARY:
      case SYBIMAGE:

         /* skip leading "0x" or "0X" */

         if (src[0] == '0' && ( src[1] == 'x' || src[1] == 'X' )) {
            src += 2;
            srclen -= 2;
         }

         /* a binary string output will be half the length of */
         /* the string which represents it in hexadecimal     */

         cr->ib = malloc(srclen / 2);
		test_alloc(cr->ib);

#if 0
         /* hey, I know this looks a bit cruddy,   */
         /* and I'm sure it can all be done in one */
         /* statement, so go on, make my day!      */

         for ( i = 0, j = 0; i < srclen; i++, j++ ) {

            inp = src[i];
            if ( inp > 47 && inp < 58 )            /* '0' thru '9' */
               hex1 = inp - 48;
            else if ( inp > 96 && inp < 103 )      /* 'a' thru 'f' */
                     hex1 = inp - 87;
                 else if ( inp > 64 && inp < 71 )  /* 'A' thru 'F' */
                          hex1 = inp - 55;
                      else {
                          fprintf(stderr,"error_handler:  Attempt to convert data stopped by syntax error in source field \n");
                          return;
                      }
      
            hex1 = hex1 << 4;
      
            i++;
      
            inp = src[i];
            if ( inp > 47 && inp < 58 )            /* '0' thru '9' */
               hex1 = hex1  | (inp - 48);
            else if ( inp > 96 && inp < 103 )      /* 'a' thru 'f' */
                     hex1 = hex1 | (inp - 87);
                 else if ( inp > 64 && inp < 71 )  /* 'A' thru 'F' */
                          hex1 =  hex1 | (inp - 55);
                      else {
                          fprintf(stderr,"error_handler:  Attempt to convert data stopped by syntax error in source field \n");
                          return;
                      }
      
            cr->ib[j] = hex1;
         }
#else
		for ( i=0; i < srclen; i++ ) {
			hex1 = src[i];
			
			if( '0' <= hex1 && hex1 <= '9' )
				hex1 &= 0x0f;
			else {
				hex1 &= 0x20 ^ 0xff;	/* mask off 0x20 to ensure upper case */
				if( 'A' <= hex1 && hex1 <= 'F' ) {
					hex1 &= 0x0f;
					hex1 += 9;
				} else {
					fprintf(stderr,"error_handler:  attempt to convert data stopped by syntax error in source field \n");
					return;
				}
			}
			assert( hex1 < 0x10 );
			
			if( i & 1 ) 
				cr->ib[i/2] <<= 4;
			else
				cr->ib[i/2] = 0;
				
			cr->ib[i/2] |= hex1;
		}
#endif
         return (srclen / 2);
         break;
      case SYBINT1:
	 	if( IS_TINYINT( atoi(src) ) ) {
	         cr->ti = atoi(src);
     	    return 1;
		}
		return 0;
         break;
      case SYBINT2:
         cr->si = atoi(src);
         return 2;
         break;
      case SYBINT4:
         cr->i = atoi(src);
         return 4;
         break;
      case SYBFLT8:
         cr->f = atof(src);
         return 8;
         break;
      case SYBREAL:
         cr->r = atof(src);
         return 4;
         break;
      case SYBBIT:
      case SYBBITN:
         cr->ti = (atoi(src)>0) ? 1 : 0;
         return 1;
         break;
      case SYBMONEY:
      case SYBMONEY4:

         i           = 0;
         places      = 0;
         point_found = 0;

         for (ptr = src; *ptr == ' '; ptr++);  /* skip leading blanks */
         for (ptr = src; *ptr == '+'; ptr++);  /* skip leading '+' */

         if ( *ptr == '-' ) {
            mynumber[i] = '-';
            ptr++;
            i++;
         }

         for(; *ptr; ptr++)                      /* deal with the rest */
         {
            if (isdigit(*ptr) )                   /* it's a number */
            {  
               mynumber[i++] = *ptr;
               if (point_found)                  /* if we passed a decimal point */
                  places++;                      /* count digits after that point  */
            }
            else if (*ptr == '.')                /* found a decimal point */
                 {
                    if (point_found)             /* already had one. lose the rest */
                       break;
                    point_found = 1;
                 }
                 else                            /* first invalid character */
                    break;                       /* lose the rest.          */
         }
         for ( j = places; j < 4; j++ )
             mynumber[i++] = '0';

         if (desttype == SYBMONEY) {
            mymoney = atoll(mynumber);
            memcpy(&(cr->m), &mymoney, sizeof(TDS_MONEY) ); 
            return sizeof(TDS_MONEY);
         } else {
            mymoney4 = atol(mynumber);
            memcpy(&(cr->m4), &mymoney4, sizeof(TDS_MONEY4) ); 
            return sizeof(TDS_MONEY4);
         }
         break;
      case SYBDATETIME:
		 return string_to_datetime(src, SYBDATETIME, cr);
		 break;
      case SYBDATETIME4:
		 return string_to_datetime(src, SYBDATETIME4, cr);
		 break;
      case SYBNUMERIC:
      case SYBDECIMAL:
		 cr->n.precision = 18;
		 cr->n.scale     = 0;
		 return string_to_numeric(src, cr);
		 break;
	  default:
         fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
	     return TDS_FAIL;
	}
}

static TDS_INT 
tds_convert_bit(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
            cr->c = malloc(1);
            *(cr->c) = src[0] ? '1' : '0';
            break;

		case SYBTEXT:
			break;
		case SYBBINARY:
		case SYBIMAGE:
			break;
		case SYBINT1:
			cr->ti = src[0] ? 1 : 0;
           
			break;
		case SYBINT2:
			cr->si = src[0] ? 1 : 0;
			break;
		case SYBINT4:
			cr->i = src[0] ? 1 : 0;
			break;
		case SYBFLT8:
			cr->f = src[0] ? 1.0 : 0.0;
			break;
		case SYBREAL:
			cr->r = src[0] ? 1.0 : 0.0;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = src[0];
			break;
		case SYBMONEY:
		case SYBMONEY4:
		case SYBNUMERIC:
		case SYBDECIMAL:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
            return TDS_FAIL;
	}
    return TDS_SUCCEED;
}

static TDS_INT 
tds_convert_int1(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen , CONV_RESULT *cr)
{
TDS_TINYINT buf;
TDS_CHAR tmp_str[5];

	memcpy(&buf, src, sizeof(buf));
	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			sprintf(tmp_str,"%d",buf);
            cr->c = malloc(strlen(tmp_str) + 1);
			test_alloc(cr->c);
			strcpy(cr->c,tmp_str);
            return strlen(tmp_str);
			break;
		case SYBINT1:
			cr->ti = buf;
            return 1;
			break;
		case SYBINT2:
			cr->si = buf;
            return 2;
			break;
		case SYBINT4:
			cr->i = buf;
            return 4;
			break;
		case SYBFLT8:
			cr->f = buf;
            return 8;
			break;
		case SYBREAL:
			cr->r = buf;
            return 4;
			break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
	}
}
static TDS_INT 
tds_convert_int2(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
TDS_SMALLINT buf;
TDS_CHAR tmp_str[16];
	
	memcpy(&buf,src,sizeof(buf));
	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			sprintf(tmp_str,"%d",buf);
            cr->c = malloc(strlen(tmp_str) + 1);
			test_alloc(cr->c);
			strcpy(cr->c,tmp_str);
            return strlen(tmp_str);
			break;
		case SYBINT1:
			cr->ti = buf;
            return 1;
			break;
		case SYBINT2:
			cr->si = buf;
            return 2;
			break;
		case SYBINT4:
			cr->i = buf;
            return 4;
			break;
		case SYBFLT8:
			cr->f = buf;
            return 8;
			break;
		case SYBREAL:
			cr->r = buf;
            return 4;
			break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
	}
}
static TDS_INT 
tds_convert_int4(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
TDS_INT buf;
TDS_CHAR tmp_str[16];
	
	memcpy(&buf,src,sizeof(buf));
	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			sprintf(tmp_str,"%d",buf);
            cr->c = malloc(strlen(tmp_str) + 1);
			test_alloc(cr->c);
			strcpy(cr->c,tmp_str);
            return strlen(tmp_str);
			break;
		case SYBINT1:
			cr->ti = buf;
            return 1;
			break;
		case SYBINT2:
			cr->si = buf;
            return 2;
			break;
		case SYBINT4:
			cr->i = buf;
            return 4;
			break;
		case SYBFLT8:
			cr->f = buf;
            return 8;
			break;
		case SYBREAL:
			cr->r = buf;
            return 4;
			break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
	}
}
static TDS_INT 
tds_convert_numeric(int srctype,TDS_NUMERIC *src,TDS_INT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
char tmpstr[MAXPRECISION];

	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			tds_numeric_to_string(src,tmpstr);
            cr->c = malloc(strlen(tmpstr) + 1);
			test_alloc(cr->c);
            strcpy(cr->c, tmpstr);
            return strlen(tmpstr);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
            memcpy(&(cr->n), src, sizeof(TDS_NUMERIC));
            return sizeof(TDS_NUMERIC);
            break;
		case SYBFLT8:
            tds_numeric_to_string(src,tmpstr);
            cr->f = atof(tmpstr);
            return 8;
            break;
		case SYBREAL:
            tds_numeric_to_string(src,tmpstr);
            cr->r = atof(tmpstr);
            return 4;
            break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
			break;
	}
}
static TDS_INT 
tds_convert_money4(int srctype,TDS_CHAR *src, int srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
TDS_MONEY4 mny;
long dollars, fraction;
char tmp_str[33];
TDS_INT8 mymoney;

	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			memcpy(&mny, src, sizeof(mny));
			dollars  = mny.mny4 / 10000;
			fraction = mny.mny4 % 10000;
			if (fraction < 0)	{ fraction = -fraction; }
			sprintf(tmp_str,"%ld.%02lu",dollars,fraction/100);
			cr->c = malloc(strlen(tmp_str) + 1);
			test_alloc(cr->c);
			strcpy(cr->c, tmp_str);
			return strlen(tmp_str);
			break;
		case SYBFLT8:
			memcpy(&dollars, src, sizeof(dollars));
			cr->f = ((TDS_FLOAT)dollars) / 10000;
            return 8;
			break;
		case SYBMONEY:
			memcpy(&mny, src, sizeof(mny));
            mymoney = mny.mny4; 
			memcpy(&(cr->m), &mymoney, sizeof(TDS_MONEY));
			return sizeof(TDS_MONEY);
            break;
		case SYBMONEY4:
            memcpy(&(cr->m4), src, sizeof(TDS_MONEY4));
            return sizeof(TDS_MONEY4);
            break;
        default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
            return TDS_FAIL;
    }

}
static TDS_INT 
tds_convert_money(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
TDS_INT high;
TDS_UINT low;
double dmoney;
char *s;

TDS_INT8 mymoney;

char rawlong[64];
int  rawlen;
char tmpstr [64];
int i;

    tdsdump_log(TDS_DBG_FUNC, "%L inside tds_convert_money()\n");
    memcpy(&mymoney, src, sizeof(TDS_INT8)); 
#	if HAVE_ATOLL
    tdsdump_log(TDS_DBG_FUNC, "%L mymoney = %lld\n", mymoney);
#	else
    tdsdump_log(TDS_DBG_FUNC, "%L mymoney = %ld\n", mymoney);
#	endif
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:

#ifdef UseBillsMoney

#		if HAVE_ATOLL
            sprintf(rawlong,"%lld", mymoney);
#		else
            sprintf(rawlong,"%ld", mymoney);
#		endif
            rawlen = strlen(rawlong);

            strncpy(tmpstr, rawlong, rawlen - 4);
            tmpstr[rawlen - 4] = '.';
            strcpy(&tmpstr[rawlen -3], &rawlong[rawlen - 4]); 
            
            cr->c = malloc(strlen(tmpstr) + 1);
		  test_alloc(cr->c);
            strcpy(cr->c, tmpstr);
            return strlen(tmpstr);
#else
			/* use brian's money */
			/* begin lkrauss 2001-10-13 - fix return to be strlen() */
          	s = tds_money_to_string((TDS_MONEY *)src, tmpstr);
			cr->c = malloc(strlen(s) + 1);
			test_alloc(cr->c);
			strcpy(cr->c, s);
			return strlen(s);
          	break;
		
#endif	/* UseBillsMoney */
            break;

		case SYBINT1:
			cr->ti = mymoney / 10000;
            return 1;
			break;
		case SYBINT2:
			cr->si = mymoney / 10000;
            return 2;
			break;
		case SYBINT4:
			cr->i = mymoney / 10000;
            return 4;
			break;
		case SYBFLT8:
            cr->f  = (float) (mymoney / 10000.0);
            return 8;
			break;
		case SYBREAL:
			cr->r  = (float) (mymoney / 10000.0);
            return 4;
			break;
		case SYBMONEY:
			memcpy(&(cr->m), src, sizeof(TDS_MONEY));
			return sizeof(TDS_MONEY);
			break;
	    default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
			break;
	}
}
static TDS_INT 
tds_convert_datetime(TDSCONTEXT *tds_ctx, int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{

unsigned int dt_days, dt_time;
int  dim[12]   = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int dty;

char whole_date_string[30];
struct tds_tm when;

TDS_INT ret;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBNVARCHAR:
		case SYBTEXT:
			if (!src) {
				cr->c = malloc(1);
				*(cr->c) = '\0';
                return 0;
			} else {
				/*
				 * Set up an extended struct tm, and call tds_strftime()
				 * to format the datetime as a string.
				 */
				memset( &when, 0, sizeof(when) );

				memcpy(&dt_days, src, 4);
				memcpy(&dt_time, src + 4, 4);

				/* it's a date before 1900 */
				if (dt_days > 2958463) { 	/* what's 2958463? */
					dt_days = 0xffffffff  - dt_days; 
					/* year */
					when.tm.tm_year = -1;
					dty = days_this_year(when.tm.tm_year);
					while ( dt_days >= dty ) {
						when.tm.tm_year--;
						dt_days -= dty;
						dty = days_this_year(when.tm.tm_year);
					}
					dim[1] = (dty == 366)? 29 : 28;

					/* month, day */
					when.tm.tm_mon = 11;
					while (dt_days > dim[when.tm.tm_mon] ) {
						dt_days -= dim[when.tm.tm_mon];
						when.tm.tm_mon--;
					}

					when.tm.tm_mday = dim[when.tm.tm_mon] - dt_days;
				} else {
					dt_days++;
					/* year */
					when.tm.tm_year = 0;
					dty = days_this_year(when.tm.tm_year);
					while ( dt_days > dty ) {
						when.tm.tm_year++;
						dt_days -= dty;
						dty = days_this_year(when.tm.tm_year);
					}

					dim[1] = (dty == 366)? 29 : 28;

					/* month, day */
					when.tm.tm_mon = 0;
					while (dt_days > dim[when.tm.tm_mon] ) {
						dt_days -= dim[when.tm.tm_mon];
						when.tm.tm_mon++;
					}
					when.tm.tm_mday = dt_days;
				}
				when.tm.tm_sec = dt_time / 300;
				when.milliseconds = ((dt_time - (when.tm.tm_sec * 300)) * 1000) / 300 ;
                tdsdump_log(TDS_DBG_FUNC, "%L inside convert_datetime() ms = %d \n", when.milliseconds);

				/* hours, minutes, seconds */
				when.tm.tm_hour = when.tm.tm_sec / 3600; 
				when.tm.tm_min = (when.tm.tm_sec % 3600) / 60; 
				when.tm.tm_sec =  when.tm.tm_sec %   60; 

				tds_strftime( whole_date_string, sizeof(whole_date_string), tds_ctx->locale->date_fmt, &when );
				cr->c = malloc (strlen(whole_date_string) + 1);
				test_alloc(cr->c);
				strcpy(cr->c , whole_date_string);
                return strlen(whole_date_string);
			}
			break;
		case SYBDATETIME:
			memcpy(&dt_days, src, 4);
			memcpy(&dt_time, src + 4, 4);
			cr->dt.dtdays = dt_days;
			cr->dt.dttime = dt_time;
            return sizeof(TDS_DATETIME);
			break;
		case SYBDATETIME4:
			memcpy(&dt_days, src, 4);
			memcpy(&dt_time, src + 4, 4);
			cr->dt4.days    = dt_days;
			cr->dt4.minutes = (dt_time / 300) / 60;
            return sizeof(TDS_DATETIME4);
			break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
			break;
	}

}

int days_this_year (int years)
{
int year;

   year = 1900 + years;
   if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
      return 366;
   else
      return 365;
}

static TDS_INT 
tds_convert_datetime4(TDSCONTEXT *tds_ctx, int srctype, TDS_CHAR *src,
	int desttype, TDS_INT destlen, CONV_RESULT *cr)
{

TDS_USMALLINT dt_days, dt_mins;
int  dim[12]   = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

int dty = 365;

char whole_date_string[30];
struct tds_tm when;

TDS_INT ret;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBNVARCHAR:
		case SYBTEXT:
			if (!src) {
				cr->c = malloc(1);
				*(cr->c) = '\0';
                return 0;
			} else {
				/*
				 * Set up an extended struct tm, and call tds_strftime()
				 * to format the datetime as a string.
				 */
				memset( &when, 0, sizeof(when) );

				memcpy(&dt_days, src, 2);
				memcpy(&dt_mins, src + 2, 2);

				dt_days++;
				
				/* year */
				while ( dt_days > dty ) {
					when.tm.tm_year++;
					dt_days -= dty;
					dty = days_this_year(when.tm.tm_year);
				}

				dim[1] = (dty == 366)? 29 : 28;		/* February */

				/* month, day */
				while (dt_days > dim[when.tm.tm_mon] ) {
					dt_days -= dim[when.tm.tm_mon];
					when.tm.tm_mon++;
				}
				when.tm.tm_mday = dt_days;

				/* hours, minutes */
				when.tm.tm_hour = dt_mins / 60; 
				when.tm.tm_min =  dt_mins % 60; 

				/* no seconds, milliseconds for smalldatetime */

				tds_strftime( whole_date_string, sizeof(whole_date_string), tds_ctx->locale->date_fmt, &when );

				cr->c = malloc (strlen(whole_date_string) + 1);
				test_alloc(cr->c);
				strcpy(cr->c , whole_date_string);
                return strlen(whole_date_string);
			}
			break;
		case SYBDATETIME:
			memcpy(&dt_days, src, 2);
			memcpy(&dt_mins, src + 2, 2);
			cr->dt.dtdays = dt_days;
			cr->dt.dttime = (dt_mins * 60) * 300;
            return sizeof(TDS_DATETIME);
			break;
		case SYBDATETIME4:
			memcpy(&dt_days, src, 2);
			memcpy(&dt_mins, src + 2, 2);
			cr->dt4.days    = dt_days;
			cr->dt4.minutes = dt_mins;
            return sizeof(TDS_DATETIME4);
			break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
			break;
	}


}

static TDS_INT 
tds_convert_real(int srctype, TDS_CHAR *src,
	int desttype, TDS_INT destlen, CONV_RESULT *cr)
{
TDS_REAL the_value;
char tmp_str[15];
TDS_INT  mymoney4;
TDS_INT8 mymoney;

   memcpy(&the_value, src, 4);

   switch(desttype) {
      case SYBCHAR:
      case SYBTEXT:
      case SYBVARCHAR:
            sprintf(tmp_str,"%.7g", the_value);
            cr->c = malloc(strlen(tmp_str) + 1);
		test_alloc(cr->c);
            strcpy(cr->c, tmp_str);
            return strlen(tmp_str);
            break;

      case SYBFLT8:
            cr->f = the_value;
            return 8;
            break;

      case SYBREAL:
            cr->r = the_value;
            return 4;
            break;

	  case SYBMONEY:
            mymoney = the_value * 10000;
			memcpy(&(cr->m), &mymoney, sizeof(TDS_MONEY));
			return sizeof(TDS_MONEY);
            break;

	  case SYBMONEY4:
            mymoney4 = the_value * 10000;
            memcpy(&(cr->m4), &mymoney4, sizeof(TDS_MONEY4));
            return sizeof(TDS_MONEY4);
            break;

      default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
            return TDS_FAIL;
   }

}

static TDS_INT 
tds_convert_flt8(int srctype, TDS_CHAR *src,
	int desttype, TDS_INT destlen, CONV_RESULT *cr)
{
TDS_FLOAT the_value;
char      tmp_str[25];

   memcpy(&the_value, src, 8);
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
            sprintf(tmp_str,"%.15g", the_value);
            cr->c = malloc(strlen(tmp_str) + 1);
		test_alloc(cr->c);
            strcpy(cr->c, tmp_str);
            return strlen(tmp_str);
            break;
      case SYBMONEY:
            cr->m.mny = the_value * 10000;
            return sizeof(TDS_MONEY);
            break;
      case SYBMONEY4:
            cr->m4.mny4 = the_value * 10000;
            return sizeof(TDS_MONEY4);
            break;
      case SYBREAL:
            cr->r = the_value;
            return 4;
            break;
      case SYBFLT8:
            cr->f = the_value;
            return 8;
            break;
      default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
   }
}

tds_convert_unique(int srctype,TDS_CHAR *src, TDS_INT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{

/* Raw data is equivalent to structure and always aligned, so this cast 
   is portable */

TDS_UNIQUE *u = (TDS_UNIQUE*)src;
TDS_UCHAR buf[37];

	switch(desttype) {
   	   case SYBCHAR:
   	   case SYBTEXT:
   	   case SYBVARCHAR:
		    sprintf(buf,"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			        (int)u->Data1,(int)u->Data2,(int)u->Data3,
        			u->Data4[0], u->Data4[1],
        			u->Data4[2], u->Data4[3], u->Data4[4],
        			u->Data4[5], u->Data4[6], u->Data4[7]);
            cr->c = malloc(strlen(buf) + 1);
		test_alloc(cr->c);
   			strcpy(cr->c,buf);
            return strlen(buf);
   			break;
   		case SYBUNIQUE:
			/* Here we can copy raw to structure because we adjust
			   byte order in tds_swap_datatype */
			memcpy (&(cr->u), src, sizeof(TDS_UNIQUE));
			return sizeof(TDS_UNIQUE);
			break;
		default:
            fprintf(stderr,"error_handler: conversion from %d to %d not supported\n", srctype, desttype);
			return TDS_FAIL;
	}
}

TDS_INT 
tds_convert(TDSCONTEXT *tds_ctx, int srctype, TDS_CHAR *src, TDS_UINT srclen,
		int desttype, TDS_UINT destlen, CONV_RESULT *cr)
{
TDS_VARBINARY *varbin;
char errmsg[255];

	switch(srctype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBNVARCHAR:
			return tds_convert_char(srctype,src, srclen,
				desttype,destlen, cr);
			break;
		case SYBMONEY4:
			return tds_convert_money4(srctype,src,srclen,
				desttype,destlen, cr);
			break;
		case SYBMONEY:
			return tds_convert_money(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			return tds_convert_numeric(srctype,(TDS_NUMERIC *) src,srclen,
				desttype,destlen, cr);
			break;
		case SYBBIT:
		case SYBBITN:
			return tds_convert_bit(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBINT1:
			return tds_convert_int1(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBINT2:
			return tds_convert_int2(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBINT4:
			return tds_convert_int4(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBREAL:
			return tds_convert_real(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBFLT8:
			return tds_convert_flt8(srctype,src,
				desttype,destlen, cr);
			break;
		case SYBDATETIME:
			return tds_convert_datetime(tds_ctx, srctype,src,
				desttype,destlen, cr);
			break;
		case SYBDATETIME4:
			return tds_convert_datetime4(tds_ctx, srctype,src,
				desttype,destlen, cr);
			break;
		case SYBVARBINARY:
			varbin = (TDS_VARBINARY *)src;
			return tds_convert_binary(srctype, (TDS_UCHAR *)varbin->array,
				varbin->len,desttype, destlen, cr);
			break;
		case SYBIMAGE:
		case SYBBINARY:
			return tds_convert_binary(srctype, (TDS_UCHAR *)src,srclen,
				desttype, destlen, cr);
			break;
		case SYBTEXT:
			return tds_convert_text(tds_ctx, srctype,src,srclen,
				desttype,destlen, cr);
			break;
		case SYBNTEXT:
			return tds_convert_ntext(srctype,src,srclen,
				desttype,destlen, cr);
			break;
		case SYBUNIQUE:
			return tds_convert_unique(srctype,src,srclen,
				desttype,destlen, cr);
			break;
		default:
			sprintf(errmsg,"Attempting to convert unknown source type %d\n",srctype);
			tds_client_msg(tds_ctx, NULL, 10001, 1, 4, 1, errmsg); 
			return TDS_FAIL;
		break;

	}
}
static int string_to_datetime(char *instr, int desttype,  CONV_RESULT *cr)
{
enum states { GOING_IN_BLIND,
              PUT_NUMERIC_IN_CONTEXT,
              DOING_ALPHABETIC_DATE,
              STRING_GARBLED };

char *in;
char *tok;
char *lasts;
char last_token[32];
int   monthdone = 0;
int   yeardone  = 0;
int   mdaydone  = 0;
int   timedone  = 0;
int   ampmdone  = 0;

struct tds_time mytime;
struct tds_time *t;

unsigned int dt_days, dt_time;
int          dim[12]   = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
int          dty, i;
int          conv_ms;

int current_state;

	memset(&mytime, '\0', sizeof(struct tds_time));
	t = &mytime;

	in = (char *)malloc(strlen(instr));
	test_alloc(in);
	strcpy (in , instr );

	tok = tds_strtok_r (in, " ,", &lasts);

	current_state = GOING_IN_BLIND;

	while (tok != (char *) NULL) {
		switch (current_state) {
			case GOING_IN_BLIND :
				/* If we have no idea of current context, then if we have */
				/* encountered a purely alphabetic string, it MUST be an  */
				/* alphabetic month name or prefix...                     */

				if (is_alphabetic(tok)) {
					if (is_monthname(tok)) {
						store_monthname(tok, t);
						monthdone++;
						current_state = DOING_ALPHABETIC_DATE;
					} else {
						current_state = STRING_GARBLED;
					}
				}

              /* ...whereas if it is numeric, it could be a number of   */
              /* things...                                              */

              else if (is_numeric(tok)) {
                      switch(strlen(tok)) {
                         /* in this context a 4 character numeric can   */
                         /* ONLY be the year part of an alphabetic date */

                         case 4:
                            store_year(atoi(tok), t);
                            yeardone++;
                            current_state = DOING_ALPHABETIC_DATE;
                            break;

                         /* whereas these could be the hour part of a   */
                         /* time specification ( 4 PM ) or the leading  */
                         /* day part of an alphabetic date ( 15 Jan )   */

                         case 2:
                         case 1:
                            strcpy(last_token, tok);
                            current_state = PUT_NUMERIC_IN_CONTEXT;
                            break;

                         /* this must be a [YY]YYMMDD date             */

                         case 6:
                         case 8:
                            if (store_yymmdd_date(tok, t))
                               current_state = GOING_IN_BLIND;
                            else
                               current_state = STRING_GARBLED;
                            break;

                         /* anything else is nonsense...               */

                         default:
                            current_state = STRING_GARBLED;
                            break;
                      }
                   }

                   /* it could be [M]M/[D]D/[YY]YY format              */

                   else if (is_numeric_dateformat(tok)) {
                            store_numeric_date(tok, t);
                            current_state = GOING_IN_BLIND;
                   } else if (is_timeformat(tok)) {
                                store_time(tok, t);
                                current_state = GOING_IN_BLIND;
                   } else {
                                 current_state = STRING_GARBLED;
						 }

              break;   /* end of GOING_IN_BLIND */                               

           case DOING_ALPHABETIC_DATE:

              if (is_alphabetic(tok)) {
                 if (!monthdone && is_monthname(tok)) {
                     store_monthname(tok, t);
                     monthdone++;
                     if (monthdone && yeardone && mdaydone )
                        current_state = GOING_IN_BLIND;
                     else
                        current_state = DOING_ALPHABETIC_DATE;
                 } else {
                     current_state = STRING_GARBLED;
					  }
              } else if (is_numeric(tok)) {
                      if (mdaydone && yeardone)
                          current_state = STRING_GARBLED;
                      else switch(strlen(tok)) {
                              case 4:
                                 store_year(atoi(tok), t);
                                 yeardone++;
                                 if (monthdone && yeardone && mdaydone )
                                    current_state = GOING_IN_BLIND;
                                 else
                                    current_state = DOING_ALPHABETIC_DATE;
                                 break;

                              case 2:
                              case 1:
                                 if (!mdaydone) {
                                    store_mday(tok, t);

                                    mdaydone++;
                                    if (monthdone && yeardone && mdaydone )
                                       current_state = GOING_IN_BLIND;
                                    else
                                       current_state = DOING_ALPHABETIC_DATE;
                                 } else {
                                    store_year(atoi(tok), t);
                                    yeardone++;
                                    if (monthdone && yeardone && mdaydone )
                                       current_state = GOING_IN_BLIND;
                                    else
                                       current_state = DOING_ALPHABETIC_DATE;
                                 }
                                 break;

                              default:
                                 current_state = STRING_GARBLED;
                           }
                   } else {
                     current_state = STRING_GARBLED;
						 }

              break;   /* end of DOING_ALPHABETIC_DATE */                        

           case PUT_NUMERIC_IN_CONTEXT:

              if (is_alphabetic(tok)) {
                 if (is_monthname(tok)) {
                     store_mday(last_token, t);
                     mdaydone++;
                     store_monthname(tok, t);
                     monthdone++;
                     if (monthdone && yeardone && mdaydone )
                        current_state = GOING_IN_BLIND;
                     else
                        current_state = DOING_ALPHABETIC_DATE;
                 } else if (is_ampm(tok)) {
                     store_hour(last_token, tok, t);
                     current_state = GOING_IN_BLIND;
                 } else {
                         current_state = STRING_GARBLED;
					  }
              } else if (is_numeric(tok)) {
                      switch(strlen(tok)) {
                         case 4:
                         case 2:
                           store_mday(last_token, t);
                           mdaydone++;
                           store_year(atoi(tok), t);
                           yeardone++;

                           if (monthdone && yeardone && mdaydone )
                              current_state = GOING_IN_BLIND;
                           else
                              current_state = DOING_ALPHABETIC_DATE;
                           break;

                         default:
                           current_state = STRING_GARBLED;
                       }
                } else {
                   current_state = STRING_GARBLED;
					 }

              break;   /* end of PUT_NUMERIC_IN_CONTEXT */                       

           case STRING_GARBLED:
                
              fprintf(stderr,"error_handler:  Attempt to convert data stopped by syntax error in source field \n");
              return TDS_FAIL;
        }

        tok = tds_strtok_r((char *)NULL, " ,", &lasts);
    }

    /* 1900 or after */ 
    if (t->tm_year >= 0) {
       dt_days = 0;
       for (i = 0; i < t->tm_year ; i++) {
           dty = days_this_year(i);
           dt_days += dty;
       }

       dty = days_this_year(i);
       if (dty == 366 )
           dim[1] = 29;
       else
           dim[1] = 28;
       for (i = 0; i < t->tm_mon ; i++) {
           dt_days += dim[i];
       }

       dt_days += (t->tm_mday - 1);

    } else {
 	   dt_days = 0xffffffff;
       /* dt_days = 4294967295U;  0xffffffff */
       for (i = -1; i > t->tm_year ; i--) {
           dty = days_this_year(i);
           dt_days -= dty;
       }
       dty = days_this_year(i);
       if (dty == 366 )
           dim[1] = 29;
       else
           dim[1] = 28;

       for (i = 11; i > t->tm_mon ; i--) {
           dt_days -= dim[i];
       }

       dt_days -= dim[i] - t->tm_mday;

    }

    free(in);

    if ( desttype == SYBDATETIME ) {
       cr->dt.dtdays = dt_days;

       dt_time = 0;

       for (i = 0; i < t->tm_hour ; i++)
           dt_time += 3600;

       for (i = 0; i < t->tm_min ; i++)
           dt_time += 60;

       dt_time += t->tm_sec;

       cr->dt.dttime = dt_time * 300;

       conv_ms = (int) ((float)((float)t->tm_ms / 1000.0) * 300.0);
       tdsdump_log(TDS_DBG_FUNC, "%L inside string_to_datetime() ms = %d (%d)\n", conv_ms, t->tm_ms);
       cr->dt.dttime += conv_ms;
       return sizeof(TDS_DATETIME);
    } else {
       /* SYBDATETIME4 */ 
       cr->dt4.days = dt_days;

       dt_time = 0;

       for (i = 0; i < t->tm_hour ; i++)
           dt_time += 60;

       for (i = 0; i < t->tm_min ; i++)
           dt_time += 1;

        cr->dt4.minutes = dt_time;
       return sizeof(TDS_DATETIME4);

    }

}

static TDS_INT 
string_to_numeric(char *instr, CONV_RESULT *cr)
{

char  mynumber[39];
/* unsigned char  mynumeric[16]; */ 

char *ptr;
char c = '\0';

unsigned char masks[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

short int  binarylist[128];
short int  carry_on = 1;
short int  i = 0;
short int  j = 0;
short int  x = 0;
short int bits, bytes, places, point_found, sign;

struct diglist *topptr = (struct diglist *)NULL;
struct diglist *curptr = (struct diglist *)NULL;
struct diglist *freeptr = (struct diglist *)NULL;

  sign        = 0;
  point_found = 0;
  places      = 0;


  if (cr->n.precision == 0)
     cr->n.precision = 18; 


  for (ptr = instr; *ptr == ' '; ptr++);  /* skip leading blanks */

  if (*ptr == '-' || *ptr == '+')         /* deal with a leading sign */
  {
     if (*ptr == '-')
        sign = 1;
     ptr++;
  }

  for(; *ptr; ptr++)                      /* deal with the rest */
  {
     if (isdigit(*ptr))                   /* it's a number */
     {  
        mynumber[i++] = *ptr;
        if (point_found)                  /* if we passed a decimal point */
           places++;                      /* count digits after that point  */
     }
     else if (*ptr == '.')                /* found a decimal point */
          {
             if (point_found)             /* already had one. lose the rest */
                break;
             if (cr->n.scale == 0)       /* no scale...lose the rest  */
                break;
             point_found = 1;
          }
          else                            /* first invalid character */
             break;                       /* lose the rest.          */
  }


  if (cr->n.scale > 0)                   /* scale specified, pad out */
  {                                       /* number with zeroes to the */
                                          /* scale...                  */

     for (j = 0 ; j < (cr->n.scale - places) ; j++ )
         mynumber[i++] = '0';

  }

  mynumber[i] = '\0';


  if (strlen(mynumber) > cr->n.precision )
     strcpy(mynumber, &mynumber[strlen(mynumber) - cr->n.precision]);
 
  for (ptr = mynumber; *ptr; ptr++)
  {
      if (topptr == (struct diglist *)NULL)
      {
          topptr = (struct diglist *)malloc(sizeof(struct diglist));
          curptr = topptr;
          curptr->nextptr = NULL;
          curptr->dig = *ptr - 48;
          curptr->carried = 0;
      }
      else
      {
          curptr->nextptr = (struct diglist *)malloc(sizeof(struct diglist));
          curptr = curptr->nextptr;
          curptr->nextptr = NULL;
          curptr->dig = *ptr - 48;
          curptr->carried = 0;
      }
  }

  memset(&binarylist[0], '\0',  sizeof(short int) * 128);
  i = 127;

  while (carry_on)
  {
     carry_on = 0;
     for (curptr = topptr ; curptr != NULL; curptr = curptr->nextptr)
     {
         if (curptr->dig > 0)
            carry_on = 1;
     
         if (curptr->nextptr != NULL )
         {
            curptr->nextptr->carried = ( ( curptr->carried * 10 ) + curptr->dig ) % 2;
            curptr->dig =  ( ( curptr->carried * 10 ) + curptr->dig ) / 2;
         }
         else
         {
            if (carry_on)
            {
               binarylist[i--] = ( ( curptr->carried * 10 ) + curptr->dig ) % 2;
            }
            curptr->dig = ( ( curptr->carried * 10 ) + curptr->dig ) / 2;
         }
     }

  }

  memset(cr->n.array, '\0', 17);
  bits  = 0;
  bytes = 1;

  cr->n.array[0] =  sign;

  x = g__numeric_bytes_per_prec[cr->n.precision] - 1;

  for (i = 128 - (x * 8); i < 128 ; i++)
  {
     if (binarylist[i])
     {
        c = c | masks[bits]; 
     }
     bits++;
     if (bits == 8)
     {
        cr->n.array[bytes] = c;

        bytes++;
        bits = 0;
        c    = '\0';
     }
  }

  curptr = topptr;
  while (curptr != NULL)
  {
      freeptr = curptr;
      curptr  = curptr->nextptr;
      free(freeptr);
  }

  return sizeof(TDS_NUMERIC);
}

static int _tds_pad_string(char *dest, int destlen)
{
int i=0;

	if (destlen>strlen(dest)) {
		for (i=strlen(dest)+1;i<destlen;i++)
			dest[i]=' ';
		dest[i]='\0';
	}
	return i;
}
 

static int is_numeric_dateformat(char *t)
{
char *instr ;
int   ret   = 1;
int   slashes  = 0;
int   hyphens  = 0;
int   periods  = 0;
int   digits   = 0;

    for (instr = t; *instr; instr++ )
    {
        if (!isdigit(*instr) && *instr != '/' && *instr != '-' && *instr != '.' )
        {
            ret = 0;
            break;
        }
        if (*instr == '/' ) slashes++;
        else if (*instr == '-' ) hyphens++;
             else if (*instr == '.' ) periods++;
                  else digits++;
       
    }
    if (hyphens + slashes + periods != 2)
       ret = 0;
    if (hyphens == 1 || slashes == 1 || periods == 1)
       ret = 0;

    if (digits < 4 || digits > 8)
       ret = 0;

    return(ret);

}

static int is_monthname(char *datestr)
{

int ret = 0;

    if (strlen(datestr) == 3)
    {
       if (strcasecmp(datestr,"jan") == 0) ret = 1;
       else if (strcasecmp(datestr,"feb") == 0) ret = 1;
       else if (strcasecmp(datestr,"mar") == 0) ret = 1;
       else if (strcasecmp(datestr,"apr") == 0) ret = 1;
       else if (strcasecmp(datestr,"may") == 0) ret = 1;
       else if (strcasecmp(datestr,"jun") == 0) ret = 1;
       else if (strcasecmp(datestr,"jul") == 0) ret = 1;
       else if (strcasecmp(datestr,"aug") == 0) ret = 1;
       else if (strcasecmp(datestr,"sep") == 0) ret = 1;
       else if (strcasecmp(datestr,"oct") == 0) ret = 1;
       else if (strcasecmp(datestr,"nov") == 0) ret = 1;
       else if (strcasecmp(datestr,"dec") == 0) ret = 1;
       else ret = 0;
    }
    else
    {
       if (strcasecmp(datestr,"january") == 0) ret = 1;
       else if (strcasecmp(datestr,"february") == 0) ret = 1;
       else if (strcasecmp(datestr,"march") == 0) ret = 1;
       else if (strcasecmp(datestr,"april") == 0) ret = 1;
       else if (strcasecmp(datestr,"june") == 0) ret = 1;
       else if (strcasecmp(datestr,"july") == 0) ret = 1;
       else if (strcasecmp(datestr,"august") == 0) ret = 1;
       else if (strcasecmp(datestr,"september") == 0) ret = 1;
       else if (strcasecmp(datestr,"october") == 0) ret = 1;
       else if (strcasecmp(datestr,"november") == 0) ret = 1;
       else if (strcasecmp(datestr,"december") == 0) ret = 1;
       else ret = 0;

    }
    return(ret);

}
static int is_ampm(char *datestr)
{

int ret = 0;

    if (strcasecmp(datestr,"am") == 0) ret = 1;
    else if (strcasecmp(datestr,"pm") == 0) ret = 1;
    else ret = 0;

    return(ret);

}

static int is_alphabetic(char *datestr)
{
char *s;
int  ret = 1;
    for (s = datestr; *s; s++) {
        if (!isalpha(*s))
           ret = 0; 
    }
    return(ret);
}

static int is_numeric(char *datestr)
{
char *s;
int  ret = 1;
    for (s = datestr; *s; s++) {
        if (!isdigit(*s))
           ret = 0; 
    }
    return(ret);
}

static int is_timeformat(char *datestr)
{
char *s;
int  ret = 1;
    for (s = datestr; *s; s++) 
    {
        if (!isdigit(*s) && *s != ':' && *s != '.' )
          break;
    }
    if ( *s )
    {
       if (strcasecmp(s, "am" ) != 0 && strcasecmp(s, "pm" ) != 0 )
          ret = 0; 
    }
    

    return(ret);
}

static int store_year(int year , struct tds_time *t)
{

    if ( year <= 0 )
       return 0; 

    if ( year < 100 )
    {
       if (year > 49)
          t->tm_year = year;
       else
          t->tm_year = 100 + year ;
       return (1);
    }

    if ( year < 1753 )
       return (0);

    if ( year <= 9999 )
    {
       t->tm_year = year - 1900;
       return (1);
    }

    return (0);

}
static int store_mday(char *datestr , struct tds_time *t)
{
int  mday = 0;

    mday = atoi(datestr);

    if ( mday > 0 && mday < 32 )
    {
       t->tm_mday = mday;
       return (1);
    }
    else
       return 0; 
}

static int store_numeric_date(char *datestr , struct tds_time *t)
{
enum {TDS_MONTH, 
      TDS_DAY, 
      TDS_YEAR};

int  state = TDS_MONTH;
char last_char = 0; 
char *s;
int  month = 0, year = 0, mday = 0;

    for (s = datestr; *s; s++) {
        if (! isdigit(*s) && isdigit(last_char)) {
            state++;
        } else switch(state) {
            case TDS_MONTH:
                month = (month * 10) + (*s - '0');
                break;
            case TDS_DAY:
                mday = (mday * 10) + (*s - '0');
                break;
            case TDS_YEAR:
                year = (year * 10) + (*s - '0');
                break;
        }
        last_char = *s;
    }

    if ( month > 0 && month < 13 )
       t->tm_mon = month - 1;
    else
       return 0; 
    if ( mday > 0 && mday < 32 )
       t->tm_mday = mday;
    else
       return 0; 

    return store_year(year, t);

}

static int store_monthname(char *datestr , struct tds_time *t)
{

int ret = 0;

    if (strlen(datestr) == 3)
    {
       if (strcasecmp(datestr,"jan") == 0) t->tm_mon = 0;
       else if (strcasecmp(datestr,"feb") == 0) t->tm_mon = 1;
       else if (strcasecmp(datestr,"mar") == 0) t->tm_mon = 2;
       else if (strcasecmp(datestr,"apr") == 0) t->tm_mon = 3;
       else if (strcasecmp(datestr,"may") == 0) t->tm_mon = 4;
       else if (strcasecmp(datestr,"jun") == 0) t->tm_mon = 5;
       else if (strcasecmp(datestr,"jul") == 0) t->tm_mon = 6;
       else if (strcasecmp(datestr,"aug") == 0) t->tm_mon = 7;
       else if (strcasecmp(datestr,"sep") == 0) t->tm_mon = 8;
       else if (strcasecmp(datestr,"oct") == 0) t->tm_mon = 9;
       else if (strcasecmp(datestr,"nov") == 0) t->tm_mon = 10;
       else if (strcasecmp(datestr,"dec") == 0) t->tm_mon = 11;
       else ret = 0;
    }
    else
    {
       if (strcasecmp(datestr,"january") == 0) t->tm_mon = 0;
       else if (strcasecmp(datestr,"february") == 0) t->tm_mon = 1;
       else if (strcasecmp(datestr,"march") == 0) t->tm_mon = 2;
       else if (strcasecmp(datestr,"april") == 0) t->tm_mon = 3;
       else if (strcasecmp(datestr,"june") == 0) t->tm_mon = 5;
       else if (strcasecmp(datestr,"july") == 0) t->tm_mon = 6;
       else if (strcasecmp(datestr,"august") == 0) t->tm_mon = 7;
       else if (strcasecmp(datestr,"september") == 0) t->tm_mon = 8;
       else if (strcasecmp(datestr,"october") == 0) t->tm_mon = 9;
       else if (strcasecmp(datestr,"november") == 0) t->tm_mon = 10;
       else if (strcasecmp(datestr,"december") == 0) t->tm_mon = 11;
       else ret = 0;

    }
    return(ret);

}
static int store_yymmdd_date(char *datestr , struct tds_time *t)
{
int  month = 0, year = 0, mday = 0;

int wholedate;

    wholedate = atoi(datestr);

    year  = wholedate / 10000 ;
    month = ( wholedate - (year * 10000) ) / 100 ; 
    mday  = ( wholedate - (year * 10000) - (month * 100) );

    if ( month > 0 && month < 13 )
       t->tm_mon = month - 1;
    else
       return 0; 
    if ( mday > 0 && mday < 32 )
       t->tm_mday = mday;
    else
       return 0; 

    return (store_year(year, t));

}

static int store_time(char *datestr , struct tds_time *t)
{
enum {TDS_HOURS, 
      TDS_MINUTES, 
      TDS_SECONDS,
      TDS_FRACTIONS};

int  state = TDS_HOURS;
char last_sep;
char *s;
int hours = 0, minutes = 0, seconds = 0, millisecs = 0;
int ret = 1;

    for (s = datestr; 
         *s && strchr("apmAPM" , (int) *s) == (char *)NULL; 
         s++) 
    {
        if ( *s == ':' || *s == '.' ) {
            last_sep = *s;
            state++;
        } else switch(state) {
            case TDS_HOURS:
                hours = (hours * 10) + (*s - '0');
                break;
            case TDS_MINUTES:
                minutes = (minutes * 10) + (*s - '0');
                break;
            case TDS_SECONDS:
                seconds = (seconds * 10) + (*s - '0');
                break;
            case TDS_FRACTIONS:
                millisecs = (millisecs * 10) + (*s - '0');
                break;
        }
    }
    if (*s)
    {
       if(strcasecmp(s,"am") == 0)
       {
          if (hours == 12)
              hours = 0;

          t->tm_hour = hours;
       }
       if(strcasecmp(s,"pm") == 0)
       {
          if (hours == 0)
              ret = 0;
          if (hours > 0 && hours < 12)
              t->tm_hour = hours + 12;
          else
              t->tm_hour = hours;
       }
    }
    else
    {
      if (hours >= 0 && hours < 24 )
         t->tm_hour = hours;
      else
         ret = 0;
    }
    if (minutes >= 0 && minutes < 60)
      t->tm_min = minutes;
    else
      ret = 0;
    if (seconds >= 0 && minutes < 60)
      t->tm_sec = seconds;
    else
      ret = 0;
    tdsdump_log(TDS_DBG_FUNC, "%L inside store_time() millisecs = %d\n", millisecs);
    if (millisecs)
    {
      if (millisecs >= 0 && millisecs < 1000 )
      {
         if (last_sep == ':')
            t->tm_ms = millisecs;
         else
         {

            if (millisecs < 10)
               t->tm_ms = millisecs * 100;
            else if (millisecs < 100 )
                    t->tm_ms = millisecs * 10;
                 else 
                    t->tm_ms = millisecs;
         }
      }
      else
        ret = 0;
      tdsdump_log(TDS_DBG_FUNC, "%L inside store_time() tm_ms = %d\n", t->tm_ms);
    }


    return (ret);
}

static int store_hour(char *hour , char *ampm , struct tds_time *t)
{
int ret = 1;
int  hours;

    hours = atoi(hour);

    if (hours >= 0 && hours < 24 )
    {
       if(strcasecmp(ampm,"am") == 0)
       {
          if (hours == 12)
              hours = 0;

          t->tm_hour = hours;
       }
       if(strcasecmp(ampm,"pm") == 0)
       {
          if (hours == 0)
              ret = 0;
          if (hours > 0 && hours < 12)
              t->tm_hour = hours + 12;
          else
              t->tm_hour = hours;
       }
    }
    return (ret);
}

TDS_INT tds_get_null_type(int srctype)
{

	switch(srctype) {
		case SYBCHAR:
			return SYBVARCHAR;
			break;
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
			return SYBINTN;
			break;
		case SYBREAL:
		case SYBFLT8:
			return SYBFLTN;
			break;
		case SYBDATETIME:
		case SYBDATETIME4:
			return SYBDATETIMN;
			break;
		default:
			return srctype;

	}
	return srctype;
}
 
/*
 * format a date string according to an "extended" strftime formatting definition.
 */     
static size_t 
tds_strftime(char *buf, size_t maxsize, const char *format, const struct tds_tm *timeptr)
{
	int length=0;
	char *s, *our_format;
	char millibuf[8];
	
	char *pz = NULL;
	
	our_format = malloc( strlen(format) + 1 );
	if( !our_format ) return 0;
	strcpy( our_format, format );
		
	pz = strstr( our_format, "%z" );
	
	/* 
	 * Look for "%z" in the format string.  If found, replace it with timeptr->milliseconds.
	 * For example, if milliseconds is 124, the format string 
	 * "%b %d %Y %H:%M:%S.%z" would become 
	 * "%b %d %Y %H:%M:%S.124".  
	 */
	 
	/* Skip any escaped cases (%%z) */
	while( pz && *(pz-1) == '%' )
		pz = strstr( ++pz, "%z" );
	
	if( pz && length < maxsize - 1 ) {
		
		sprintf( millibuf, "%03d", timeptr->milliseconds );
		
		/* move everything back one, then overwrite "?%z" with millibuf */
		for( s = our_format + strlen(our_format); s > pz; s-- ) {
			*(s+1) = *s;
		}
		
		strncpy( pz, millibuf, 3 );	/* don't copy the null */
	}
	
	length = strftime( buf, maxsize, our_format, &timeptr->tm );	
	
	free(our_format);
	
	return length;
}
static TDS_UINT 
utf16len(const utf16_t* s)
{
      const utf16_t* p = s;
      while (*p++)
              ;
      return p - s;
}

#ifdef DONT_TRY_TO_COMPILE_THIS
	Try this: "perl -x convert.c > tds_willconvert.h"
	(Perl will generate useful C from the data below.)  
#!perl
	$indent = "\t ";
	printf qq(/* ** %-65s ** */\n), "Please do not edit this file!";
	printf qq(/* ** %-65s ** */\n), "It was generated with 'perl -x convert.c > tds_willconvert.h' ";
	printf qq(/* ** %-65s ** */\n), "See the comments about tds_willconvert in convert.c";
	printf qq(/* ** %-65s ** */\n), "It is much easier to edit the __DATA__ table than this file.  ";
	printf qq(/* ** %-65s ** */\n), " ";
	printf qq(/* ** %65s ** */\n\n), "Thank you.";
	
	while(<DATA>) {
		next if /^\s+To\s+$/;
		next if /^From/;
		if( /^\s+CHAR TEXT/ ) {
			@to = split;
			next;
		}
		last if /^BOUNDARY/;
		
		@yn = split;
		$from = shift @yn;
		$i = 0;
		foreach $to (@to) {
			last if $to =~ /^BOUNDARY/;

			$yn = $yn[$i];	# default
			$yn = 1 if $yn[$i] eq 'T';
			$yn = 0 if $yn[$i] eq 'F';
			
			printf "$indent %-30.30s, %s", "{ SYB${from}, SYB${to}", "$yn }\n"; 

			$i++;
			$indent = "\t,";
		}
	}

__DATA__
          To
From
          CHAR TEXT BINARY IMAGE INT1 INT2 INT4 FLT8 REAL NUMERIC DECIMAL BIT MONEY MONEY4 DATETIME DATETIME4 BOUNDARY SENSITIVITY
CHAR        T   T    T      T     T   T    T    T    T   T       T       T   T    T      T        T         T       T
TEXT        T   T    T      T     T   T    T    T    T   T       T       T   T    T      T        T         T       T
BINARY      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
IMAGE       T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
INT1        T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
INT2        T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
INT4        T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
FLT8        T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
REAL        T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
NUMERIC     T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
DECIMAL     T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
BIT         T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
MONEY       T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
MONEY4      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
DATETIME    T   T    T      T     F   F    F    F    F   F       F       F   F    F      T        T         F       F
DATETIME4   T   T    T      T     F   F    F    F    F   F       F       F   F    F      T        T         F       F
BOUNDARY    T   T    F      F     F   F    F    F    F   F       F       F   F    F      F        F         T       F
SENSITIVITY T   T    F      F     F   F    F    F    F   F       F       F   F    F      F        F         F       T
#endif
unsigned char
tds_willconvert(int srctype, int desttype)
{
typedef struct { int srctype; int desttype; int yn; } ANSWER;
const static ANSWER answers[] = {
#	include "tds_willconvert.h"
};
int i;
	
	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_willconvert()\n");

	for( i=0; i < sizeof(answers)/sizeof(ANSWER); i++ ){
		if( srctype == answers[i].srctype 
		 && desttype == answers[i].desttype )  {
            tdsdump_log(TDS_DBG_FUNC, "%L inside tds_willconvert() %d %d %d\n", answers[i].srctype, answers[i].desttype, answers[i].yn);
		 	return answers[i].yn;
         }
	}

	return 0;

}
TDS_INT tds_datecrack( TDS_INT datetype, void *di, TDSDATEREC *dr )
{

TDS_DATETIME  *dt;
TDS_DATETIME4 *dt4;

unsigned int dt_days;
unsigned int dt_time;


int dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
char mn[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

int dty, years, months, days, ydays, wday, hours, mins, secs, ms;
int i;

    if ( datetype == SYBDATETIME ) {
       dt = (TDS_DATETIME *)di;
   	   dt_days = dt->dtdays;
   	   dt_time = dt->dttime;
    } 
    else if (datetype == SYBDATETIME4 ) {
            dt4 = (TDS_DATETIME4 *)di;
   	        dt_days = dt4->days;
   	        dt_time = dt4->minutes;
         } 
         else
            return TDS_FAIL;
          

	if (dt_days > 2958463) /* its a date before 1900 */ {
	  	dt_days = 0xffffffff - dt_days; 

		wday = 7 - ( dt_days % 7); 
		years = -1;
		dty = days_this_year(years);

        
		while ( dt_days >= dty ) {
			years--; 
			dt_days -= dty;
			dty = days_this_year(years);
		}
		if (dty == 366 )
			dim[1] = 29;
		else
			dim[1] = 28;

		ydays = dty - dt_days;
		months = 11;
 
		while (dt_days > dim[months] ) {
			dt_days -= dim[months];
			months--;
		}

		days = dim[months] - dt_days;
	} else {
		wday = ( dt_days + 1 ) % 7; /* 'cos Jan 1 1900 was Monday */

		dt_days++;
		years = 0;
		dty = days_this_year(years);
		while ( dt_days > dty ) {
			years++; 
			dt_days -= dty;
			dty = days_this_year(years);
		}

		if (dty == 366 )
			dim[1] = 29;
		else
			dim[1] = 28;

		ydays = dt_days;
		months = 0;
		while (dt_days > dim[months] ) {
			dt_days -= dim[months];
			months++;
		}

		days = dt_days;
	}

    if ( datetype == SYBDATETIME ) {

	   secs = dt_time / 300;
   	   ms = ((dt_time - (secs * 300)) * 1000) / 300 ;
   
   	   hours = 0;
   	   while ( secs >= 3600 ) {
   		   hours++; 
   		   secs -= 3600;
   	   }
   
   	   mins = 0;
   
   	   while ( secs >= 60 ) {
   		   mins++; 
   		   secs -= 60;
   	   }
    } 
    else {

      hours = 0;
      mins  = dt_time;
      secs  = 0;
      ms    = 0;

      while ( mins >= 60 ) {
          hours++;
          mins -= 60;
      }

    }

	dr->year        = 1900 + years;
	dr->month       = months;
	dr->day         = days;
	dr->dayofyear   = ydays;
	dr->weekday     = wday;
	dr->hour        = hours;
	dr->minute      = mins;
	dr->second      = secs;
	dr->millisecond = ms;
}

/**
 * sybase's char->int conversion tolerates embedded blanks, 
 * such that "convert( int, ' - 13 ' )" works.  
 * if we find blanks, we copy the string to a temporary buffer, 
 * skipping the blanks.  
 * we return the results of atoi() with a clean string.  
 * 
 * n.b. it is possible to embed all sorts of non-printable characters, but we
 * only check for spaces.  at this time, no one on the project has tested anything else.  
 */
#undef atoi
int
tds_atoi(const char *buf)
{
enum { blank = ' ' };
char *s;
const char *p;
int 	i;
	
	s = strchr( buf, blank );
	if( !s )
		return atoi(buf);
	
	while( *s++ == blank );		/* ignore trailing */
	
	if( *s == '\0' )
		return atoi(buf);
	
	s = (char*) malloc( strlen(buf) );
	
	for( i=0, p=buf; *p != '\0'; p++ ) {
		if( *p != blank )
			s[i++] = *p;
	}
	s[i] = '\0';
	
	i = atoi(s);
	free(s);
	
	return i;
}


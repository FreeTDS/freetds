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

static char  software_version[]   = "$Id: convert.c,v 1.68 2002-09-05 12:07:20 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

typedef unsigned short utf16_t;

/* static int  _tds_pad_string(char *dest, int destlen); */
static TDS_INT tds_convert_int1(int srctype,TDS_CHAR *src, int desttype,TDS_INT destlen , CONV_RESULT *cr);
extern char *tds_numeric_to_string(TDS_NUMERIC *numeric, char *s);
extern char *tds_money_to_string(TDS_MONEY *money, char *s);
static int  string_to_datetime(char *datestr, int desttype, CONV_RESULT *cr );
/**
 * convert a number in string to a TDSNUMERIC
 * @return sizeof(TDS_NUMERIC) on success, TDS_FAIL on failure 
 */
static int string_to_numeric(const char *instr, const char *pend, CONV_RESULT *cr);
/**
 * convert a zero terminated string to NUMERIC
 * @return sizeof(TDS_NUMERIC) on success, TDS_FAIL on failure 
 */
static int stringz_to_numeric(const char *instr, CONV_RESULT *cr);
/**
 * convert a number in string to TDS_INT 
 * @return TDS_FAIL if failure
 */
static TDS_INT string_to_int(const char *buf,const char *pend,TDS_INT* res);

#define TDS_CONVERT_NOAVAIL -2
/**
 * convert, same as tds_convert but not return error to client
 * @return TDS_FAIL on conversion failure
 * TDS_CONVERT_NOAVAIL if conversion impossible
 */
static TDS_INT
tds_convert_noerror(TDSCONTEXT *tds_ctx, int srctype, TDS_CHAR *src, 
	TDS_UINT srclen, int desttype, TDS_UINT destlen, CONV_RESULT *cr);

static size_t 
tds_strftime(char *buf, size_t maxsize, const char *format, const struct tds_tm *timeptr);
static int  store_hour(char *, char *, struct tds_time *);
static int  store_time(char *, struct tds_time * );
static int  store_yymmdd_date(char *, struct tds_time *);
static int  store_monthname(char *, struct tds_time *);
static int  store_numeric_date(char *, struct tds_time *);
static int  store_mday(char *, struct tds_time *);
static int  store_year(int,  struct tds_time *);
static int  days_this_year (int years);
static int  is_timeformat(char *);
static int  is_numeric(char *);
static int  is_alphabetic(char *);
static int  is_ampm(char *);
static int  is_monthname(char *);
static int  is_numeric_dateformat(char *);
static TDS_UINT utf16len(const utf16_t* s);
static const char *tds_prtype(int token);

#define test_alloc(x) {if ((x)==NULL) return TDS_FAIL;}
extern const int g__numeric_bytes_per_prec[];

#define IS_TINYINT(x) ( 0 <= (x) && (x) <= 0xff )
#define IS_SMALLINT(x) ( -32768 <= (x) && (x) <= 32767 )
/* f77: I don't write -2147483648, some compiler seem to have some problem 
 * with this constant although is a valid 32bit value */
#define IS_INT(x) ( (-2147483647l-1l) <= (x) && (x) <= 2147483647l )

#define LOG_CONVERT() \
	tdsdump_log(TDS_DBG_ERROR, "error_handler: conversion from " \
			"%d to %d not supported\n", srctype, desttype)

#define CONVERSION_ERROR( socket, from, varchar, to ) \
	send_conversion_error_msg( (socket), 249, __LINE__, (from), (varchar), (to) )

/* eg "Syntax error during explicit conversion of VARCHAR value ' - 13 ' to a DATETIME field." */

void
send_conversion_error_msg(TDSSOCKET *tds, int err, int line, int from, char *varchar, int to)
{	
	enum { level=16, state=1 };
	/* TODO 249 is the standard explicit conversion error number. 
	 * If this function is passed some other number, it should have a 
	 * static lookup table of message strings (by number and locale). --jkl
	 */
	const static char *message = "Syntax error during explicit conversion of %.30s value '%.3900s' to a %.30s field.";
	char buffer[4096];
	
	sprintf( buffer, message, tds_prtype(from), varchar, tds_prtype(to) );
	
	assert( strlen(buffer) < sizeof(buffer) );

	tds_client_msg(tds->tds_ctx, tds, err, level, state, line, buffer); 
}

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

/**
 * Copy a terminated string to result and return len or TDS_FAIL
 */
static TDS_INT 
string_to_result(const char* s,CONV_RESULT* cr)
{
int len = strlen(s);

	cr->c = malloc(len + 1);
	test_alloc(cr->c);
	memcpy(cr->c, s, len + 1);
	return len;
}

/**
 * Copy binary data to to result and return len or TDS_FAIL
 */
static TDS_INT
binary_to_result(const void* data,size_t len,CONV_RESULT* cr)
{
	cr->ib = malloc(len);
	test_alloc(cr->ib);
	memcpy(cr->ib, data, len);
	return len;
}

/*TODO many conversions to varbinary are not implemented */



/* TODO implement me */
/*
static TDS_INT 
tds_convert_ntext(int srctype,TDS_CHAR *src,TDS_UINT srclen,
      int desttype,TDS_UINT destlen, CONV_RESULT *cr)
{
      return TDS_FAIL;
}
*/

static TDS_INT 
tds_convert_binary(int srctype,TDS_UCHAR *src,TDS_INT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
int cplen;
int s;
char *c;
char hex2[3];

   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
      case SYBTEXT:

	 /* NOTE: Do not prepend 0x to string.  
	  * The libraries all expect a bare string, without a 0x prefix. 
	  * Applications such as isql and query analyzer provide the "0x" prefix. */

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
         return (srclen * 2);
         break;
      case SYBIMAGE:
      case SYBBINARY:
	 return binary_to_result(src, srclen, cr);
         break;
      case SYBVARBINARY:
         cplen = srclen > 256 ? 256 : srclen;
         cr->vb.len = cplen;
         memcpy(cr->vb.array, src, cplen);
         return cplen;
         break;
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBREAL:
	case SYBFLT8:
		cplen = get_size_by_type(desttype);
		if (cplen <= srclen)
			return binary_to_result(src, cplen, cr);
		cr->ib = malloc(cplen);
		test_alloc(cr->ib);
		memcpy(cr->ib, src, srclen);
		memset(cr->ib+srclen, 0, cplen-srclen);
		return cplen;
		break;
	 /* conversion not allowed */
      case SYBDATETIME4:
      case SYBDATETIME:
      case SYBDATETIMN:
	 break;
	/* TODO should we do some test for these types or work as ints ?? */
	case SYBDECIMAL:
	case SYBNUMERIC:
	case SYBBIT:
	case SYBBITN:
      default:
	 LOG_CONVERT();
         return TDS_FAIL;
         break;
   }
   return TDS_FAIL;
}

static TDS_INT 
tds_convert_char(int srctype, TDS_CHAR *src, TDS_UINT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
int           i, j;
unsigned char hex1;

TDS_INT8     mymoney;
TDS_INT      mymoney4;
char         mynumber[39];

char *ptr,*pend;
int point_found, places;
TDS_INT tds_i;

   
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
      case SYBTEXT:
		 cr->c = malloc(srclen + 1);
		test_alloc(cr->c);
		 memcpy(cr->c, src, srclen);
		 cr->c[srclen] = 0;
         return srclen; 
		 break;

		 /* TODO VARBINARY missed */
      case SYBBINARY:
      case SYBIMAGE:

         /* skip leading "0x" or "0X" */

         if (src[0] == '0' && ( src[1] == 'x' || src[1] == 'X' )) {
            src += 2;
            srclen -= 2;
         }

	/* ignore trailing blanks and nulls */
	/* FIXME is good to ignore null ?? */
	while( srclen > 0 && (src[srclen-1] == ' ' || src[srclen-1] == '\0') )
		--srclen;

         /* a binary string output will be half the length of */
         /* the string which represents it in hexadecimal     */
	
	/* if srclen if odd we must add a "0" before ... */
	j = 0; /* number where to start converting */
	if (srclen & 1) {
		++srclen; j = 1; --src;
	}
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
		for ( i=srclen; --i >= j; ) {
			hex1 = src[i];
			
			if( '0' <= hex1 && hex1 <= '9' )
				hex1 &= 0x0f;
			else {
				hex1 &= 0x20 ^ 0xff;	/* mask off 0x20 to ensure upper case */
				if( 'A' <= hex1 && hex1 <= 'F' ) {
					hex1 -= ('A'-10);
				} else {
					tdsdump_log(TDS_DBG_INFO1,"error_handler:  attempt to convert data stopped by syntax error in source field \n");
					return TDS_FAIL;
				}
			}
			assert( hex1 < 0x10 );
			
			if( i & 1 ) 
				cr->ib[i/2] = hex1;
			else
				cr->ib[i/2] |= hex1 << 4;
		}
#endif
         return srclen / 2;
         break;
      case SYBINT1:
		if (string_to_int(src,src + srclen,&tds_i) == TDS_FAIL)
			return TDS_FAIL;
		if( IS_TINYINT( tds_i ) ) {
			cr->ti = tds_i;
     	    return 1;
		}
		return TDS_FAIL;
         break;
      case SYBINT2:
		if (string_to_int(src,src + srclen,&tds_i) == TDS_FAIL)
			return TDS_FAIL;
		if ( !IS_SMALLINT(tds_i) )
			return TDS_FAIL;
		cr->si = tds_i;
         return 2;
         break;
      case SYBINT4:
		if (string_to_int(src,src + srclen,&tds_i) == TDS_FAIL)
			return TDS_FAIL;
		cr->i = tds_i;
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
		if (string_to_int(src,src + srclen,&tds_i) == TDS_FAIL)
			return TDS_FAIL;
		cr->ti = tds_i ? 1 : 0;
         return 1;
         break;
      case SYBMONEY:
      case SYBMONEY4:

	 /* TODO code similar to string_to_numeric... */
         i           = 0;
         places      = 0;
         point_found = 0;
	 pend        = src + srclen;

         /* skip leading blanks */
         for (ptr = src; ptr != pend && *ptr == ' '; ++ptr);

	 switch ( ptr != pend ? *ptr : 0 ) {
		 case '-':
			 mynumber[i++] = '-';
			 /* fall through*/
		 case '+':
			ptr++;
			for (; ptr != pend && *ptr == ' '; ++ptr);
			break;
         }

         for(; ptr != pend; ptr++)                      /* deal with the rest */
         {
            if (isdigit(*ptr) )                   /* it's a number */
            {  
               mynumber[i++] = *ptr;
	       /* assure not buffer overflow */
	       if (i==30) return TDS_FAIL;
               if (point_found) {                 /* if we passed a decimal point */
                  /* count digits after that point  */
		  /* FIXME check rest of buffer */
		  if (++places == 4)
			  break;
	       }
            }
            else if (*ptr == '.')                /* found a decimal point */
                 {
                    if (point_found)             /* already had one. lose the rest */
                       return TDS_FAIL;
                    point_found = 1;
                 }
                 else                            /* first invalid character */
                    return TDS_FAIL;                       /* lose the rest.          */
         }
         for ( j = places; j < 4; j++ )
             mynumber[i++] = '0';
	 mynumber[i] = 0;

	 /* FIXME overflow not handled */
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
		return string_to_numeric(src, src+srclen, cr);
		break;
	 case SYBUNIQUE: {
		int i;
		unsigned n;
		char c;
		 /* parse formats like XXXXXXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX 
		  * or {XXXXXXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX} 
		  * SQL seem to ignore additional character... */
		if (srclen < (32+3)) return TDS_FAIL;
		if (src[0] == '{') {
			if (srclen < (32+5) || src[32+4] != '}')
				return TDS_FAIL;
			++src;
		}
		if (src[8] != '-' || src[8+4+1] != '-' || src[16+2] != '-')
			return TDS_FAIL;
		/* test all characters and get value 
		 * first I tried using sscanf but it work if number terminate
		 * with less digits */
		for(i = 0; i < 32+3; ++i) {
			c = src[i];
			switch(i) {
				case 8:
					if (c!='-') return TDS_FAIL;
					cr->u.Data1 = n;
					n = 0;
					break;
				case 8+4+1:
					if (c!='-') return TDS_FAIL;
					cr->u.Data2 = n;
					n = 0;
					break;
				case 16+2:
					if (c!='-') return TDS_FAIL;
					cr->u.Data3 = n;
					n = 0;
					break;
				default:
					n = n << 4;
					if (c >= '0' && c <= '9') n += c-'0';
					else {
						c &= 0x20 ^ 0xff;
						if (c >= 'A' && c <= 'F')
							n += c-('A'-10);
						else return TDS_FAIL;
					}
					if (i>(16+2) && !(i&1)) {
						cr->u.Data4[(i>>1)-10] = n;
						n = 0;
					}
			}
		}
	 }
	 return sizeof(TDS_UNIQUE);
	 default:
		LOG_CONVERT();
	     return TDS_FAIL;
	} /* end switch */
} /* tds_convert_char */

static TDS_INT 
tds_convert_bit(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
	int canonic = src[0] ? 1 : 0;
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			cr->c = malloc(2);
			test_alloc(cr->c);
			cr->c[0] = '0' + canonic;
			cr->c[1] = 0;
			return 1;
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,1,cr);
			break;
		case SYBINT1:
			cr->ti = canonic;
			return 1;
			break;
		case SYBINT2:
			cr->si = canonic;
			return 2;
			break;
		case SYBINT4:
			cr->i = canonic;
			return 4;
			break;
		case SYBFLT8:
			cr->f = canonic;
			return 8;
			break;
		case SYBREAL:
			cr->r = canonic;
			return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = src[0];
			return 1;
			break;
		case SYBMONEY:
		case SYBMONEY4:
			return tds_convert_int1( SYBINT1, (src[0])? "\1" : "\0", desttype, destlen, cr);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			return stringz_to_numeric(canonic ? "1" : "0",cr);
			break;

		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
		default:
			LOG_CONVERT();
            return TDS_FAIL;
	}
	return TDS_FAIL;
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
			return string_to_result(tmp_str,cr);
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,1,cr);
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
		case SYBBIT:
		case SYBBITN:
			cr->ti = buf ? 1 : 0;
			return 1;
			break;
		case SYBFLT8:
			cr->f = buf;
            return 8;
			break;
		case SYBREAL:
			cr->r = buf;
            return 4;
			break;
		case SYBMONEY4:
			cr->m4.mny4 = buf * 10000;
			return 4;
			break;
		case SYBMONEY:
			cr->m.mny = buf * 10000;
			return sizeof(TDS_MONEY);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			sprintf(tmp_str,"%d",buf);
			return stringz_to_numeric(tmp_str,cr);
			break;
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
		default:
			LOG_CONVERT();
			return TDS_FAIL;
	}
	return TDS_FAIL;
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
			return string_to_result(tmp_str,cr);
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,2,cr);
			break;
		case SYBINT1:
			if (!IS_TINYINT(buf))
				return TDS_FAIL;
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
		case SYBBIT:
		case SYBBITN:
			cr->ti = buf ? 1 : 0;
			return 1;
			break;
		case SYBFLT8:
			cr->f = buf;
            return 8;
			break;
		case SYBREAL:
			cr->r = buf;
            return 4;
			break;
		case SYBMONEY4:
			cr->m4.mny4 = buf * 10000;
			return 4;
			break;
		case SYBMONEY:
			cr->m.mny = buf * 10000;
			return sizeof(TDS_MONEY);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			sprintf(tmp_str,"%d",buf);
			return stringz_to_numeric(tmp_str,cr);
			break;
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
		default:
			LOG_CONVERT();
			return TDS_FAIL;
	}
	return TDS_FAIL;
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
			return string_to_result(tmp_str,cr);
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,4,cr);
			break;
		case SYBINT1:
			if (!IS_TINYINT(buf))
				return TDS_FAIL;
			cr->ti = buf;
            return 1;
			break;
		case SYBINT2:
			if ( !IS_SMALLINT(buf) )
				return TDS_FAIL;
			cr->si = buf;
            return 2;
			break;
		case SYBINT4:
			cr->i = buf;
            return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = buf ? 1 : 0;
			return 1;
			break;
		case SYBFLT8:
			cr->f = buf;
            return 8;
			break;
		case SYBREAL:
			cr->r = buf;
            return 4;
			break;
		case SYBMONEY4:
			if (buf > 214748 || buf < -214748)
				return TDS_FAIL;
			cr->m4.mny4 = buf * 10000;
			return 4;
			break;
		case SYBMONEY:
			cr->m.mny = (TDS_INT8)buf * 10000;
			return sizeof(TDS_MONEY);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			sprintf(tmp_str,"%d",buf);
			return stringz_to_numeric(tmp_str,cr);
			break;
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
		default:
			LOG_CONVERT();
			return TDS_FAIL;
	}
	return TDS_FAIL;
}
static TDS_INT 
tds_convert_numeric(int srctype,TDS_NUMERIC *src,TDS_INT srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
char tmpstr[MAXPRECISION];
TDS_INT i;

	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			tds_numeric_to_string(src,tmpstr);
			return string_to_result(tmpstr,cr);
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_NUMERIC),cr);
			break;
		case SYBINT1:
			tds_numeric_to_string(src,tmpstr);
			i = atoi(tmpstr);
			if (!IS_TINYINT(i))
				return TDS_FAIL;
			cr->ti = i;
			return 1;
			break;
		case SYBINT2:
			tds_numeric_to_string(src,tmpstr);
			i = atoi(tmpstr);
			if ( !IS_SMALLINT(i) )
				return TDS_FAIL;
			cr->si = i;
			return 2;
			break;
		case SYBINT4:
			tds_numeric_to_string(src,tmpstr);
			i = atoi(tmpstr);
			if ( !IS_INT(i) )
				return TDS_FAIL;
			cr->i = i;
			return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = 0;
			for(i=g__numeric_bytes_per_prec[src->precision]; --i > 0;)
				if (src->array[i] != 0) {
					cr->ti = 1;
					break;
				}
			return 1;
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
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
	    /* TODO conversions to money */
		default:
			LOG_CONVERT();
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
}
static TDS_INT 
tds_convert_money4(int srctype,TDS_CHAR *src, int srclen,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
TDS_MONEY4 mny;
long dollars, fraction;
char tmp_str[33];

	memcpy(&mny, src, sizeof(mny));
	switch(desttype) {
		case SYBCHAR:
		case SYBTEXT:
		case SYBVARCHAR:
			/* FIXME should be rounded ??
			 * see also all conversion to int and from money 
			 * rounding with dollars = (mny.mny4 + 5000) /10000
			 * can give arithmetic overflow solution should be
			 * dollars = (mny.mny4/2 + 2500)/5000 */
			dollars  = mny.mny4 / 10000;
			fraction = mny.mny4 % 10000;
			if (fraction < 0)	{ fraction = -fraction; }
			sprintf(tmp_str,"%ld.%02lu",dollars,fraction/100);
			return string_to_result(tmp_str,cr);
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_MONEY4),cr);
			break;
		case SYBINT1:
			dollars  = mny.mny4 / 10000;
			if (!IS_TINYINT(dollars))
				return TDS_FAIL;
			cr->ti = dollars;
			return 1;
			break;
		case SYBINT2:
			dollars  = mny.mny4 / 10000;
			if (!IS_SMALLINT(dollars))
				return TDS_FAIL;
			cr->si = dollars;
			return 2;
			break;
		case SYBINT4:
			cr->i = mny.mny4 / 10000;
			return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = mny.mny4 ? 1 : 0;
			return 1;
			break;
		case SYBFLT8:
			cr->f = ((TDS_FLOAT)mny.mny4) / 10000.0;
            return 8;
			break;
		case SYBREAL:
			cr->r = ((TDS_REAL)mny.mny4) / 10000.0;
			return 4;
			break;
		case SYBMONEY:
			cr->m.mny = (TDS_INT8)mny.mny4; 
			return sizeof(TDS_MONEY);
            break;
		case SYBMONEY4:
            memcpy(&(cr->m4), src, sizeof(TDS_MONEY4));
            return sizeof(TDS_MONEY4);
            break;
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
		case SYBDECIMAL:
		case SYBNUMERIC:
			dollars  = mny.mny4 / 10000;
			fraction = mny.mny4 % 10000;
			if (fraction < 0)	{ fraction = -fraction; }
			sprintf(tmp_str,"%ld.%04lu",dollars,fraction);
			return stringz_to_numeric(tmp_str,cr);
        default:
			LOG_CONVERT();
            return TDS_FAIL;
    }
	return TDS_FAIL;
}

static TDS_INT 
tds_convert_money(int srctype,TDS_CHAR *src,
	int desttype,TDS_INT destlen, CONV_RESULT *cr)
{
char *s;

TDS_INT8 mymoney,dollars;

char rawlong[64];
int  rawlen;
char tmpstr [64];
int i;

    tdsdump_log(TDS_DBG_FUNC, "%L inside tds_convert_money()\n");
#if defined(WORDS_BIGENDIAN) || !defined(HAVE_INT64)
    memcpy(&mymoney, src, sizeof(TDS_INT8)); 
#else
    memcpy(((char*)&mymoney)+4, src, 4);
    memcpy(&mymoney, src+4, 4);
#endif

#	if HAVE_ATOLL
    tdsdump_log(TDS_DBG_FUNC, "%L mymoney = %lld\n", mymoney);
#	else
    tdsdump_log(TDS_DBG_FUNC, "%L mymoney = %ld\n", mymoney);
#	endif
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:

#ifdef UseBillsMoney
			if (mymoney <= -10000 || mymoney >= 10000) {

#		if HAVE_ATOLL
            sprintf(rawlong,"%lld", mymoney);
#		else
            sprintf(rawlong,"%ld", mymoney);
#		endif
            rawlen = strlen(rawlong);

            strncpy(tmpstr, rawlong, rawlen - 4);
            tmpstr[rawlen - 4] = '.';
            strcpy(&tmpstr[rawlen -3], &rawlong[rawlen - 4]); 
			} else {
				i = mymoney;
				s = tmpstr;
				if (i < 0) {
					*s++ = '-';
					i = -i;
				}
				sprintf(s,"0.%04d",i);
			}
            
            return string_to_result(tmpstr,cr);
#else
			/* use brian's money */
			/* begin lkrauss 2001-10-13 - fix return to be strlen() */
          	s = tds_money_to_string((TDS_MONEY *)src, tmpstr);
                return string_to_result(s,cr);
          	break;
		
#endif	/* UseBillsMoney */
            break;

		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_MONEY),cr);
			break;
		case SYBINT1:
			dollars  = mymoney / 10000;
			if (!IS_TINYINT(dollars))
				return TDS_FAIL;
			cr->ti = dollars;
			return 1;
			break;
		case SYBINT2:
			dollars  = mymoney / 10000;
			if (!IS_SMALLINT(dollars))
				return TDS_FAIL;
			cr->si = dollars;
			return 2;
			break;
		case SYBINT4:
			dollars  = mymoney / 10000;
			if (!IS_INT(dollars))
				return TDS_FAIL;
			cr->i = dollars;
			return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = mymoney ? 1 : 0;
			return 1;
			break;
		case SYBFLT8:
			cr->f  = ((TDS_FLOAT)mymoney) / 10000.0;
            return 8;
			break;
		case SYBREAL:
			cr->r  = ((TDS_REAL)mymoney) / 10000.0;
            return 4;
			break;
		case SYBMONEY4:
			if (!IS_INT(mymoney))
				return TDS_FAIL;
			cr->m4.mny4 = mymoney;
			break;
		case SYBMONEY:
			cr->m.mny = mymoney;
			return sizeof(TDS_MONEY);
			break;
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
			break;
		case SYBDECIMAL:
		case SYBNUMERIC:
			s = tds_money_to_string((TDS_MONEY *)src, tmpstr);
			return stringz_to_numeric(tmpstr,cr);
			break;
	    default:
			LOG_CONVERT();
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
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

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			if (!src) {
				cr->c = malloc(1);
				test_alloc(cr->c);
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
				return string_to_result(whole_date_string,cr);
			}
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_DATETIME),cr);
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
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBBIT:
		case SYBBITN:
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBMONEY4:
		case SYBMONEY:
		case SYBNUMERIC:
		case SYBDECIMAL:
			break;
		default:
			LOG_CONVERT();
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
}

static int days_this_year (int years)
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

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			if (!src) {
				cr->c = malloc(1);
				test_alloc(cr->c);
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

				return string_to_result(whole_date_string,cr);
			}
			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_DATETIME4),cr);
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
		/* conversions not allowed */
		case SYBUNIQUE:
		case SYBBIT:
		case SYBBITN:
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBMONEY4:
		case SYBMONEY:
		case SYBNUMERIC:
		case SYBDECIMAL:
			break;
		default:
			LOG_CONVERT();
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
}

static TDS_INT 
tds_convert_real(int srctype, TDS_CHAR *src,
	int desttype, TDS_INT destlen, CONV_RESULT *cr)
{
TDS_REAL the_value;
/* FIXME how many big should be this buffer ?? */
char tmp_str[128];
TDS_INT  mymoney4;
TDS_INT8 mymoney;

   memcpy(&the_value, src, 4);

   switch(desttype) {
      case SYBCHAR:
      case SYBTEXT:
      case SYBVARCHAR:
            sprintf(tmp_str,"%.7g", the_value);
	    return string_to_result(tmp_str,cr);
            break;

		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_REAL),cr);
			break;
		case SYBINT1:
			if (!IS_TINYINT(the_value))
				return TDS_FAIL;
			cr->ti = the_value;
			return 1;
			break;
		case SYBINT2:
			if (!IS_SMALLINT(the_value))
				return TDS_FAIL;
			cr->si = the_value;
			return 2;
			break;
		case SYBINT4:
			if (!IS_INT(the_value))
				return TDS_FAIL;
			cr->i = the_value;
			return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = the_value ? 1 : 0;
			return 1;
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
		case SYBNUMERIC:
		case SYBDECIMAL:
			sprintf(tmp_str,"%.*f", cr->n.scale,  the_value);
			return stringz_to_numeric(tmp_str, cr);
			break;
	    /* not allowed */
		case SYBUNIQUE:
	  case SYBDATETIME4:
	  case SYBDATETIME:
	  case SYBDATETIMN:
	    break;
      default:
	    LOG_CONVERT();
            return TDS_FAIL;
   }
	return TDS_FAIL;
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
      case SYBTEXT:
            sprintf(tmp_str,"%.15g", the_value);
	    return string_to_result(tmp_str,cr);
            break;

		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_FLOAT),cr);
			break;
		case SYBINT1:
			if (!IS_TINYINT(the_value))
				return TDS_FAIL;
			cr->ti = the_value;
			return 1;
			break;
		case SYBINT2:
			if (!IS_SMALLINT(the_value))
				return TDS_FAIL;
			cr->si = the_value;
			return 2;
			break;
		case SYBINT4:
			if (!IS_INT(the_value))
				return TDS_FAIL;
			cr->i = the_value;
			return 4;
			break;
		case SYBBIT:
		case SYBBITN:
			cr->ti = the_value ? 1 : 0;
			return 1;
			break;

      case SYBMONEY:
            cr->m.mny = (TDS_INT8)the_value * 10000.0;
            return sizeof(TDS_MONEY);
            break;
      case SYBMONEY4:
            cr->m4.mny4 = the_value * 10000.0;
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
		case SYBNUMERIC:
		case SYBDECIMAL:
            		sprintf(tmp_str,"%.15g", the_value);
			return stringz_to_numeric(tmp_str, cr);
			break;
	    /* not allowed */
		case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
		break;
      default:
		LOG_CONVERT();
			return TDS_FAIL;
   }
	return TDS_FAIL;
}

static TDS_INT
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
			return string_to_result(buf,cr);
   			break;
		case SYBBINARY:
		case SYBIMAGE:
			return binary_to_result(src,sizeof(TDS_UNIQUE),cr);
			break;
   		case SYBUNIQUE:
			/* Here we can copy raw to structure because we adjust
			   byte order in tds_swap_datatype */
			memcpy (&(cr->u), src, sizeof(TDS_UNIQUE));
			return sizeof(TDS_UNIQUE);
			break;
		/* no not warning for not convertible types */
		case SYBBIT:
		case SYBBITN:
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBMONEY4:
		case SYBMONEY:
		case SYBDATETIME4:
		case SYBDATETIME:
		case SYBDATETIMN:
		case SYBREAL:
		case SYBFLT8:
			break;
		default:
			LOG_CONVERT();
			return TDS_FAIL;
	}
	return TDS_FAIL;
}

/**
 * tds_convert
 * convert a type to another
 * If you convert to SYBDECIMAL/SYBNUMERIC you MUST initialize precision 
 * and scale of cr
 * Do not expect string to be zero terminate. Databases support zero inside
 * string. Doing strlen on result may result on data loss or even core.
 * Use memcpy to copy destination using length returned.
 * This function do not handle NULL, srclen should be >0, if not undefinited 
 * behaviour...
 * @param tds_ctx context (used in conversion to data and to return messages)
 * @param srctype  type of source
 * @param srclen   length in bytes of source (not counting terminator or strings)
 * @param desttype type of destination
 * @param destlen  length in bytes of output
 * @param cr       structure to old result
 * @return length of result or TDS_FAIL on failure
 */
TDS_INT
tds_convert(TDSCONTEXT *tds_ctx, int srctype, TDS_CHAR *src, 
		TDS_UINT srclen, int desttype, TDS_UINT destlen, 
		CONV_RESULT *cr)
{
int length;
	
/* For now, construct a TDSSOCKET.  It's what we should have been passed.
 * The only real consequence is that the user-supplied error handler's return
 * code can't be used to close the connection.  
 */
TDSSOCKET fake_socket, *tds=&fake_socket;
char varchar[2056];
CONV_RESULT result;
int len;
		

	/* FIXME this method can cause core dump, we call tds_client_msg with 
	 * invalid socket structure but handler do not know this...*/
	memset( &fake_socket, 0, sizeof(fake_socket) );
	fake_socket.tds_ctx = tds_ctx;

	length = tds_convert_noerror(tds_ctx,srctype,src,srclen,
			desttype,destlen,cr);

	switch(length) {
	case TDS_CONVERT_NOAVAIL:
		send_conversion_error_msg( tds, 20029, __LINE__, srctype, "[unable to display]", desttype );
		LOG_CONVERT();
		return TDS_FAIL;
		break;
	case TDS_FAIL:
		break;
	default:
		return length;
		break;
	}

	switch(srctype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			len= (srclen < (sizeof(varchar)-1))? srclen : (sizeof(varchar)-1);
			strncpy( varchar, src, len );
			varchar[len] = 0;
			break;
		default:
			/* recurse once to convert whatever it was to varchar */
			len = tds_convert_noerror(tds_ctx, srctype, src, srclen, SYBCHAR, sizeof(varchar), &result);
			if (len < 0) len = 0;
			if (len > (sizeof(varchar)-1))
				len = sizeof(varchar)-1;
			strncpy( varchar, result.c, len );
			varchar[len] = '\0';
			free(result.c);
			break;
	}

	CONVERSION_ERROR( tds, srctype, varchar, desttype );
	return TDS_FAIL;
}

static TDS_INT 
tds_convert_noerror(TDSCONTEXT *tds_ctx, int srctype, TDS_CHAR *src, TDS_UINT srclen,
		int desttype, TDS_UINT destlen, CONV_RESULT *cr)
{
TDS_INT length=0;
TDS_VARBINARY *varbin;

	switch(srctype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			length= tds_convert_char(srctype,src, srclen, desttype,destlen, cr);
			break;
		case SYBMONEY4:
			length= tds_convert_money4(srctype,src, srclen, desttype,destlen, cr);
			break;
		case SYBMONEY:
			length= tds_convert_money(srctype,src, desttype, destlen, cr);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			length= tds_convert_numeric(srctype,(TDS_NUMERIC *) src,srclen, desttype,destlen, cr);
			break;
		case SYBBIT:
		case SYBBITN:
			length= tds_convert_bit(srctype,src, desttype,destlen, cr);
			break;
		case SYBINT1:
			length= tds_convert_int1(srctype,src, desttype,destlen, cr);
			break;
		case SYBINT2:
			length= tds_convert_int2(srctype,src, desttype,destlen, cr);
			break;
		case SYBINT4:
			length= tds_convert_int4(srctype,src, desttype,destlen, cr);
			break;
		case SYBREAL:
			length= tds_convert_real(srctype,src, desttype,destlen, cr);
			break;
		case SYBFLT8:
			length= tds_convert_flt8(srctype,src, desttype,destlen, cr);
			break;
		case SYBDATETIME:
			length= tds_convert_datetime(tds_ctx, srctype,src, desttype,destlen, cr);
			break;
		case SYBDATETIME4:
			length= tds_convert_datetime4(tds_ctx, srctype,src, desttype,destlen, cr);
			break;
		case SYBVARBINARY:
			varbin = (TDS_VARBINARY *)src;
			length= tds_convert_binary(srctype, (TDS_UCHAR *)varbin->array,
				varbin->len,desttype, destlen, cr);
			break;
		case SYBIMAGE:
		case SYBBINARY:
			length= tds_convert_binary(srctype, (TDS_UCHAR *)src,srclen,
				desttype, destlen, cr);
			break;
		case SYBUNIQUE:
			length= tds_convert_unique(srctype,src,srclen, desttype,destlen, cr);
			break;
		case SYBNVARCHAR:
		case SYBNTEXT:
		default:
			return TDS_CONVERT_NOAVAIL;
			break;
	}

/* fix MONEY case */
#if !defined(WORDS_BIGENDIAN) && defined(HAVE_INT64)
	if (length != TDS_FAIL && desttype == SYBMONEY) {
		cr->m.mny = 
			((TDS_UINT8)cr->m.mny) >> 32 | (cr->m.mny << 32);
	}
#endif
	return length;
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

struct tds_time mytime;
struct tds_time *t;

unsigned int dt_days, dt_time;
int          dim[12]   = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
int          dty, i;
int          conv_ms;

int current_state;

	memset(&mytime, '\0', sizeof(struct tds_time));
	t = &mytime;

	in = (char *)malloc(strlen(instr)+1);
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
                
              tdsdump_log(TDS_DBG_INFO1,"error_handler:  Attempt to convert data stopped by syntax error in source field \n");
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

static int 
stringz_to_numeric(const char *instr, CONV_RESULT *cr)
{
	return string_to_numeric(instr,instr+strlen(instr),cr);
}

static int 
string_to_numeric(const char *instr, const char *pend, CONV_RESULT *cr)
{

char  mynumber[40];
/* num packaged 8 digit, see below for detail */
TDS_UINT packed_num[5];

char *ptr;
const char *pdigits;
const char* pstr;

TDS_UINT  carry = 0;
char  not_zero = 1;
int  i = 0;
int  j = 0;
short int bytes, places, point_found, sign, digits;

  sign        = 0;
  point_found = 0;
  places      = 0;

  /* FIXME: application can pass invalid value for precision and scale ?? */
  if (cr->n.precision > 38)
  	return TDS_FAIL;

  if (cr->n.precision == 0)
     cr->n.precision = 38; /* assume max precision */
      
  if ( cr->n.scale > cr->n.precision )
	return TDS_FAIL;


  /* skip leading blanks */
  for (pstr = instr;; ++pstr)
  {
  	if (pstr == pend) return TDS_FAIL;
  	if (*pstr != ' ') break;
  }

  if ( *pstr == '-' || *pstr == '+' )         /* deal with a leading sign */
  {
     if (*pstr == '-')
        sign = 1;
     pstr++;
  }

  digits = 0;
  pdigits = pstr;
  for(; pstr != pend; ++pstr)             /* deal with the rest */
  {
     if (isdigit(*pstr))                  /* its a number */
     {  
        if (point_found)                  /* if we passed a decimal point */
           ++places;                      /* count digits after that point  */
        else
           ++digits;                      /* count digits before point  */
     }
     else if (*pstr == '.')               /* found a decimal point */
          {
             if (point_found)             /* already had one. return error */
                return TDS_FAIL;
             if (cr->n.scale == 0)       /* no scale...lose the rest  */
                break; /* FIXME: check other characters */
             point_found = 1;
          }
          else                            /* first invalid character */
             return TDS_FAIL;                    /* return error.          */

  }

  /* no digits? no number!*/
  if (!digits)
  	return TDS_FAIL;

  /* truncate decimal digits */
  if ( cr->n.scale > 0 && places > cr->n.scale)
  	places = cr->n.scale;

  /* too digits, error */
  if ( (digits+cr->n.scale) > cr->n.precision)
 	return TDS_FAIL;


  /* TODO: this can be optimized in a single step */

  /* scale specified, pad out number with zeroes to the scale...  */
  ptr = mynumber+40-(cr->n.scale-places);
  memset(ptr,48,cr->n.scale-places);
  ptr -= places;
  /* copy number without point */
  memcpy(ptr,pdigits+digits+1,places);
  ptr -= digits;
  memcpy(ptr,pdigits,digits);
  memset(mynumber,48,ptr-mynumber);

  /* transform ASCII string into a numeric array */
  for (ptr = mynumber; ptr != mynumber+40; ++ptr)
  	*ptr -= 48;

  /*
   * Packaged number explanation
   * I package 8 decimal digit in one number
   * This because 10^8 = 5^8 * 2^8 = 5^8 * 256
   * So dividing 10^8 for 256 make no remainder
   * So I can split for bytes in an optmized way
   */
 
  /* transform to packaged one */
  for(j=0;j<5;++j)
      {
  	TDS_UINT n = mynumber[j*8];
 	for(i=1;i<8;++i)
      {
  		n = n * 10 + mynumber[j*8+i];
      }
  	packed_num[j] = n;
  }

  memset(cr->n.array,0,sizeof(cr->n.array));
  cr->n.array[0] =  sign;
  bytes = g__numeric_bytes_per_prec[cr->n.precision];

  while (not_zero)
  {
     not_zero = 0;
     carry = 0;
     for (i = 0; i < 5; ++i)
     {
     	TDS_UINT tmp;
     
        if (packed_num[i] > 0)
            not_zero = 1;

     	/* divide for 256 for find another byte */
     	tmp = packed_num[i];
     	/* carry * (25u*25u*25u*25u) = carry * 10^8 / 256u
     	 * using unsigned number is just an optimization
     	 * compiler can translate division to a shift and remainder 
     	 * to a binary and
     	 */
     	packed_num[i] = carry * (25u*25u*25u*25u) + packed_num[i] / 256u;
     	carry = tmp % 256u;

        if ( i == 4 && not_zero)
     {
           /* source number is limited to 38 decimal digit
            * 10^39-1 < 2^128 (16 byte) so this cannot make an overflow
            */
	   cr->n.array[--bytes] = carry;
     }
  }
  }
  return sizeof(TDS_NUMERIC);
}

/*
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
*/

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
	
	our_format = malloc( strlen(format) + (1+1) ); /* 1 for terminator and 1 for added millisecond character */
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
		if( /^\s+VARCHAR CHAR/ ) {
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
			$yn = 0 if $yn[$i] eq 't';	# means it should be true, but isnt so far.

			printf "$indent %-30.30s, %s", "{ SYB${from}, SYB${to}", "$yn }\n"; 

			$i++;
			$indent = "\t,";
		}
	}

__DATA__
          To
From
          VARCHAR CHAR TEXT BINARY IMAGE INT1 INT2 INT4 FLT8 REAL NUMERIC DECIMAL BIT MONEY MONEY4 DATETIME DATETIME4 BOUNDARY SENSITIVITY
VARCHAR     T      T   T    T 	T	 T	T	T	T	T   T	  T  	T   T    T	 T		T		T	   T
CHAR        T      T   T    T 	T	 T	T	T	T	T   T	  T  	T   T    T	 T		T		T	   T
TEXT        T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      T        T         T       T
BINARY      T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
IMAGE       T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
INT1        T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
INT2        T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
INT4        T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
FLT8        T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
REAL        T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
NUMERIC     T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
DECIMAL     T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
BIT         T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
MONEY       T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
MONEY4      T      T   T    T      T     T   T    T    T    T   T       T       T   T    T      F        F         F       F
DATETIME    T      T   T    T      T     F   F    F    F    F   F       F       F   F    F      T        T         F       F
DATETIME4   T      T   T    T      T     F   F    F    F    F   F       F       F   F    F      T        T         F       F
BOUNDARY    T      T   T    F      F     F   F    F    F    F   F       F       F   F    F      F        F         T       F
SENSITIVITY T      T   T    F      F     F   F    F    F    F   F       F       F   F    F      F        F         F       T
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
	return TDS_SUCCEED;
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
static TDS_INT
string_to_int(const char *buf,const char *pend,TDS_INT* res)
{
enum { blank = ' ' };
const char *p;
int	sign;
unsigned int num; /* we use unsigned here for best overflow check */
	
	p = buf;
	
	/* ignore leading spaces */
	while( p != pend && *p == blank )
		++p;
	if (p==pend) return TDS_FAIL;

	/* check for sign */
	sign = 0;
	switch ( *p ) {
	case '-':
		sign = 1;
		/* fall thru */
	case '+':
		/* skip spaces between sign and number */
		++p;
		while( p != pend && *p == blank )
			++p;
		break;
	}
	
	/* a digit must be present */
	if (p == pend )
		return TDS_FAIL;

	num = 0;
	for(;p != pend;++p) {
		/* check for trailing spaces */
		if (*p == blank) {
			while( p != pend && *++p == blank);
			if (p!=pend) return TDS_FAIL;
			break;
		}
	
		/* must be a digit */
		if (!isdigit(*p))
			return TDS_FAIL;
	
		/* add a digit to number and check for overflow */
		if (num > 214748364u)
			return TDS_FAIL;
		num = num * 10u + (*p-'0');
	}
	
	/* check for overflow and convert unsigned to signed */
	if (sign) {
		if (num > 2147483648u)
			return TDS_FAIL;
		*res = 0 - num;
	} else {
		if (num >= 2147483648u)
			return TDS_FAIL;
		*res = num;
	}
	
	return TDS_SUCCEED;
}

/* 
 * Offer string equivalents of conversion tokens.  
 */
static const char *
tds_prtype(int token)
{
   const char  *result = "???";

   switch (token)
   {
      case SYBBINARY:       result = "SYBBINARY";		break;
      case SYBBIT:          result = "SYBBIT";		break;
      case SYBBITN:         result = "SYBBITN";		break;
      case SYBCHAR:         result = "SYBCHAR";		break;
      case SYBDATETIME4:    result = "SYBDATETIME4";	break;
      case SYBDATETIME:     result = "SYBDATETIME";	break;
      case SYBDATETIMN:     result = "SYBDATETIMN";	break;
      case SYBDECIMAL:      result = "SYBDECIMAL";	break;
      case SYBFLT8:         result = "SYBFLT8";		break;
      case SYBFLTN:         result = "SYBFLTN";		break;
      case SYBIMAGE:        result = "SYBIMAGE";		break;
      case SYBINT1:         result = "SYBINT1";        break;
      case SYBINT2:         result = "SYBINT2";        break;
      case SYBINT4:         result = "SYBINT4";        break;
      case SYBINT8:         result = "SYBINT8";       	break;
      case SYBINTN:         result = "SYBINTN";    	break;
      case SYBMONEY4:       result = "SYBMONEY4";      break;
      case SYBMONEY:        result = "SYBMONEY";       break;
      case SYBMONEYN:       result = "SYBMONEYN";      break;
      case SYBNTEXT:  	   result = "SYBNTEXT";      	break;
      case SYBNVARCHAR:     result = "SYBNVARCHAR";	break;
      case SYBNUMERIC:      result = "SYBNUMERIC";     break;
      case SYBREAL:         result = "SYBREAL";        break;
      case SYBTEXT:         result = "SYBTEXT";        break;
      case SYBUNIQUE:       result = "SYBUNIQUE";		break;
      case SYBVARBINARY:    result = "SYBVARBINARY";   break;
      case SYBVARCHAR:      result = "SYBVARCHAR";     break;

      case SYBVARIANT  :    result = "SYBVARIANT";	break;
      case SYBVOID	   :    result = "SYBVOID";	   	break;
      case XSYBBINARY  :    result = "XSYBBINARY";	break;
      case XSYBCHAR    :    result = "XSYBCHAR";	    	break;
      case XSYBNCHAR   :    result = "XSYBNCHAR";		break;
      case XSYBNVARCHAR:    result = "XSYBNVARCHAR";	break;
      case XSYBVARBINARY:   result = "XSYBVARBINARY";	break;
      case XSYBVARCHAR :    result = "XSYBVARCHAR";	break;

      default:	break;
   }
   return result;
} 

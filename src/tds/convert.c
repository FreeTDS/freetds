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

static char  software_version[]   = "$Id: convert.c,v 1.5 2001-12-03 00:06:14 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

typedef union dbany {
        TDS_TINYINT	ti;
        TDS_SMALLINT	si;
        TDS_INT		i;
	TDS_FLOAT		f;
	TDS_REAL		r;
	TDS_CHAR		*c;
	TDS_MONEY         m;
	TDS_MONEY4	m4;
	TDS_DATETIME	dt;
	TDS_DATETIME4	dt4;
/*
	TDS_NUMERIC	n;
*/
} DBANY; 

typedef unsigned short utf16_t;

TDS_INT tds_convert_any(unsigned char *dest, TDS_INT dtype, TDS_INT dlen, DBANY *any);
static int _string_to_tm(char *datestr, struct tm *t);
static int _tds_pad_string(char *dest, int destlen);
extern char *tds_numeric_to_string(TDS_NUMERIC *numeric, char *s);
extern char *tds_money_to_string(TDS_MONEY *money, char *s);


/* 
this needs to go... 
it won't handle binary or text/image when they are added
it's not thread safe
it works for the moment though til i decide how i really want to handle it
*/
static TDS_CHAR tmp_str[4096];  


int tds_get_conversion_type(int srctype, int colsize)
{
	if (srctype == SYBINTN) {
		if (colsize==8)
			 return SYBINT8;
		if (colsize==4)
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
TDS_INT tds_convert_text(int srctype,unsigned char *src,TDS_UINT srclen,
	int desttype,unsigned char *dest,TDS_UINT destlen)
{
int cplen;

   switch(desttype) {
      case SYBTEXT:
	 cplen = srclen > destlen ? destlen : srclen;
         memcpy(dest, src, cplen);
         /* 2001-06-15 Deutsch changed [cplen-1] to [cplen] */
         dest[cplen] = '\0';
         return strlen(dest);
      case SYBCHAR:
	 cplen = srclen > destlen ? destlen : srclen;
	 /* if (cplen>255) cplen=255; */
         memcpy(dest, src, cplen);
		dest[cplen]='\0';
         return strlen(dest);
   }
   return 0;
}
static TDS_UINT utf16len(const utf16_t* s)
{
      const utf16_t* p = s;
      while (*p++)
              ;
      return p - s;
}

TDS_INT tds_convert_ntext(int srctype,unsigned char *src,TDS_UINT srclen,
      int desttype,unsigned char *dest,TDS_UINT destlen)
{
      /* 
       * XXX Limit of 255 + 1 needs to be fixed by determining what the 
       *     real limit of [N][VAR]CHAR columns is.
       * XXX What about NCHAR?  Don't see a constant for it in tds.h.
       * XXX Case for -1 in switch statement because upper levels don't
       *     have a way to bind to wide-character types.
       */
      TDS_UINT i, cplen, char_limit = 256;
      utf16_t* wsrc  = (utf16_t*)src;
      utf16_t* wdest = (utf16_t*)dest;
      assert(sizeof(utf16_t) == 2);
      switch (desttype) {
              case SYBNVARCHAR:
                      if (destlen > char_limit * sizeof(utf16_t))
                              destlen = char_limit * sizeof(utf16_t);
                      /* Fall through ... */
              case SYBNTEXT:
              case -1:
                      cplen = srclen > destlen ? destlen : srclen;
                      memcpy(dest, src, cplen);
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
                      for (i = 0; i < cplen; ++i)
                              dest[i] = (unsigned char)wsrc[i];
                      dest[cplen-1] = 0;
                      return strlen(dest);
      }
	return 0;
}


TDS_INT tds_convert_binary(int srctype,unsigned char *src,TDS_INT srclen,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
int cplen;
int d, s;
TDS_VARBINARY *varbin;

   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
         /* FIX ME */
	 if (destlen>=0 && destlen<3) {
		return 0;
	 }
	 d=0;
	 dest[d++]='0';
	 dest[d++]='x';

	 /* length = -1 means assume dest is large enough, ugly */
	 if (destlen==-1) {
          for (s=0;s<srclen;s++,d+=2) {
             dest[d]=src[s]/16 > 9 ? 87 + (src[s]/16) : '0' + (src[s]/16);
             dest[d+1]=src[s]%16 > 9 ? 87 + (src[s]%16) : '0' + (src[s]%16);
          }
	 } else {
          for (s=0;s<srclen && d < destlen-2;s++,d+=2) {
             dest[d]=src[s]/16 > 9 ? 87 + (src[s]/16) : '0' + (src[s]/16);
             dest[d+1]=src[s]%16 > 9 ? 87 + (src[s]%16) : '0' + (src[s]%16);
          }
	}
	dest[d++]='\0';
        return d;
      case SYBIMAGE:
      case SYBBINARY:
	 cplen = srclen > destlen ? destlen : srclen;
         memcpy(dest, src, cplen);
	 return cplen;
      case SYBVARBINARY:
	 cplen = srclen > destlen ? destlen : srclen;
	 varbin = (TDS_VARBINARY *) dest;
	 varbin->len = cplen;
         memcpy(varbin->array, src, cplen);
	 return sizeof(TDS_VARBINARY);
   }
   return TDS_FAIL;
}

TDS_INT tds_convert_char(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
DBANY any;
struct tm t;
time_t secs_from_epoch;
   
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
      case SYBNVARCHAR:
         any.c = src;
         break;
      case SYBTEXT:
         break;
      case SYBBINARY:
      case SYBIMAGE:
         break;
      case SYBINT1:
         any.ti = atoi(src);
         break;
      case SYBINT2:
         any.si = atoi(src);
         break;
      case SYBINT4:
         any.i = atol(src);
         break;
      case SYBFLT8:
         any.f = atof(src);
         break;
      case SYBREAL:
         any.r = atof(src);
         break;
      case SYBBIT:
      case SYBBITN:
         any.ti = (atoi(src)>0) ? 1 : 0;
         break;
      case SYBMONEY:
         break;
      case SYBMONEY4:
         break;
      case SYBDATETIME:
	 _string_to_tm(src, &t);
	 secs_from_epoch = mktime(&t);
	 any.dt.dtdays = (secs_from_epoch/60/60/24)+25567;
	 any.dt.dttime = (secs_from_epoch%60%60%24)*300;
         break;
      case SYBDATETIME4:
	 _string_to_tm(src, &t);
	 secs_from_epoch = mktime(&t);
	 any.dt4.days = (secs_from_epoch/60/60/24)+25567;
	 any.dt4.minutes = (secs_from_epoch%60%60%24)/60;
         break;
      case SYBNUMERIC:
      case SYBDECIMAL:
         break;
/*
		case SYBBOUNDRY:
			break;
		case SYBSENSITIVITY:
			break;
*/
      default:
         return TDS_FAIL;
   }
   return tds_convert_any(dest, desttype, destlen, &any);
}
TDS_INT tds_convert_bit(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
DBANY any;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			sprintf(tmp_str,"%c",src[0] ? '1' : '0');
			any.c = tmp_str;
			break;
		case SYBTEXT:
			break;
		case SYBBINARY:
		case SYBIMAGE:
			break;
		case SYBINT1:
			any.ti = src[0] ? 1 : 0;
			break;
		case SYBINT2:
			any.si = src[0] ? 1 : 0;
			break;
		case SYBINT4:
			any.i = src[0] ? 1 : 0;
			break;
		case SYBFLT8:
			any.f = src[0] ? 1.0 : 0.0;
			break;
		case SYBREAL:
			any.r = src[0] ? 1.0 : 0.0;
			break;
		case SYBBIT:
		case SYBBITN:
			any.ti = src[0];
			break;
		case SYBMONEY:
			break;
		case SYBMONEY4:
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			break;
/*
		case SYBBOUNDRY:
			break;
		case SYBSENSITIVITY:
			break;
*/
	}
	return tds_convert_any(dest, desttype, destlen, &any);
}
TDS_INT tds_convert_int1(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
DBANY any;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			sprintf(tmp_str,"%d",src[0]);
			any.c = tmp_str;
			break;
		case SYBINT1:
			any.ti = *((TDS_TINYINT *) src);
			break;
		case SYBINT2:
			any.si = *((TDS_TINYINT *) src);
			break;
		case SYBINT4:
			any.i = *((TDS_TINYINT *) src);
			break;
		default:
			return TDS_FAIL;
	}
	return tds_convert_any(dest, desttype, destlen, &any);
}
TDS_INT tds_convert_int2(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
unsigned char buf[4];
DBANY any;
	
	memcpy(buf,src,2);
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			sprintf(tmp_str,"%d",*((TDS_SMALLINT *) buf));
			any.c = tmp_str;
			break;
		case SYBINT1:
			any.ti = *((TDS_SMALLINT *) buf);
			break;
		case SYBINT2:
			any.si = *((TDS_SMALLINT *) buf);
			break;
		case SYBINT4:
			any.i = *((TDS_SMALLINT *) buf);
			break;
	}
	return tds_convert_any(dest, desttype, destlen, &any);
}
TDS_INT tds_convert_int4(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
unsigned char buf[4];
DBANY any;
	
	memcpy(buf,src,4);
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			sprintf(tmp_str,"%ld",*((TDS_INT *) buf));
			any.c = tmp_str;
			break;
		case SYBINT1:
			any.ti = *((TDS_INT *) buf);
			break;
		case SYBINT2:
			any.si = *((TDS_INT *) buf);
			break;
		case SYBINT4:
			any.i = *((TDS_INT *) buf);
			break;
		default:
			return TDS_FAIL;
	}
	return tds_convert_any(dest, desttype, destlen, &any);
}
TDS_INT tds_convert_numeric(int srctype,TDS_NUMERIC *src,TDS_INT srclen,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
char tmpstr[MAXPRECISION];
TDS_FLOAT d;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			tds_numeric_to_string(src,dest);
                        return strlen(dest);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			memcpy(dest, src, sizeof(TDS_NUMERIC));
			return sizeof(TDS_NUMERIC);
			break;
		case SYBFLT8:
			/* FIX ME -- temporary fix */
			tds_numeric_to_string(src,tmpstr);
			d = atof(tmpstr);
			memcpy(dest,&d,sizeof(double));
			break;
		default:
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
}
TDS_INT tds_convert_money4(int srctype,unsigned char *src, int srclen,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
TDS_MONEY4 mny;
long dollars, fraction;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			mny = *((TDS_MONEY4 *) src);
			dollars = mny.mny4 / 10000;
			fraction = mny.mny4 % 10000;
			if (fraction < 0)	{ fraction = -fraction; }
			sprintf(dest,"%ld.%02lu",dollars,fraction/100);
			if (desttype==SYBCHAR) {
				/* ugly hack to emulate sybase behaviour
				** which PHP relies on */
				if (destlen==-1)
					_tds_pad_string(dest,srclen);
				else
					_tds_pad_string(dest,destlen);
			}
			return strlen(dest);
			break;
		case SYBFLT8:
			memcpy(&dollars, src, 4);
			*(TDS_FLOAT *)dest = ((TDS_FLOAT)dollars) / 10000;
			break;
		case SYBMONEY4:
			memcpy(dest, src, sizeof(TDS_MONEY4));
			break;
                default:
                        return TDS_FAIL;
                        break;
	}
	return sizeof(TDS_MONEY4);
}
TDS_INT tds_convert_money(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen)
{
TDS_INT high;
TDS_UINT low;
double dmoney;
char *s;

	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
			/* begin lkrauss 2001-10-13 - fix return to be strlen() */
			s = tds_money_to_string((TDS_MONEY *)src, dest);
			return strlen(s);
			break;
		case SYBFLT8:
			/* Used memcpy to avoid alignment/bus errors */
			memcpy(&high, src, 4);
			memcpy(&low, src+4, 4);
			dmoney = (TDS_FLOAT)high * 65536 * 65536 + (TDS_FLOAT)low;
			dmoney = dmoney / 10000;
			*(TDS_FLOAT *)dest = dmoney;
			return sizeof(TDS_FLOAT); /* was returning FAIL below (mlilback, 11/7/01) */
			break;
		case SYBMONEY:
			memcpy(dest, src, sizeof(TDS_MONEY));
			break;
	       default:
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
}
TDS_INT tds_convert_datetime(int srctype,unsigned char *src,int desttype,unsigned char *dest,TDS_INT destlen)
{
TDS_INT dtdays, dttime;
time_t tmp_secs_from_epoch;
   
	switch(desttype) {
		case SYBCHAR:
		case SYBVARCHAR:
		/* FIX ME -- This fails for dates before 1902 or after 2038 */
			if (destlen<0) {
					memset(dest,' ',30);
			} else {
				memset(dest,' ',destlen);
			}
			if (!src) {
				*dest='\0';
				return 0;
			}
			memcpy(&dtdays, src, 4);
			memcpy(&dttime, src+4, 4);
			/* begin <lkrauss@wbs-blank.de> 2001-10-13 */
			if (dtdays==0 && dttime==0) {
				*dest='\0';
				return 0;
			}
			/* end lkrauss */
			tmp_secs_from_epoch = ((dtdays - 25567)*24*60*60) + (dttime/300);
			if (destlen<20) {
				strftime(dest, destlen-1, "%b %d %Y %I:%M%p",
				(struct tm*)gmtime(&tmp_secs_from_epoch));
				return destlen;
			} else {
				strftime(dest, 20, "%b %d %Y %I:%M%p",
				(struct tm*)gmtime(&tmp_secs_from_epoch));
				return (strlen(dest));
			}
			break;
		case SYBDATETIME:
			memcpy(dest,src,sizeof(TDS_DATETIME));
			return sizeof(TDS_DATETIME);
			break;
		case SYBDATETIME4:
			break;
		default:
			return TDS_FAIL;
			break;
	}
	return TDS_FAIL;
}
TDS_INT tds_convert_datetime4(int srctype,unsigned char *src,int desttype,unsigned char *dest,TDS_INT destlen)
{
   TDS_USMALLINT days, minutes;
   time_t tmp_secs_from_epoch;
   
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
	if (destlen<0) {
	 	memset(dest,' ',30);
	} else {
		memset(dest,' ',destlen);
	}
	if (!src) {
		*dest='\0';
		return 0;
	}
	memcpy(&days, src, 2);
	memcpy(&minutes, src+2, 2);
	if (days==0 && minutes==0) {
		*dest='\0';
		return 0;
	}
	tdsdump_log(TDS_DBG_INFO1, "%L inside tds_convert_datetime4() days = %d minutes = %d\n", days, minutes);
        tmp_secs_from_epoch = (days - 25567)*(24*60*60) + (minutes*60);
        /* if (strlen(src)>destlen) { */
	   if (destlen<20) {
           strftime(dest, destlen-1, "%b %d %Y %I:%M%p",
                    (struct tm*)gmtime(&tmp_secs_from_epoch));
           return destlen;
        } else {
           strftime(dest, 20, "%b %d %Y %I:%M%p",
                    (struct tm*)gmtime(&tmp_secs_from_epoch));
           return (strlen(dest));
	}
	break;
      case SYBDATETIME:
	break;
      case SYBDATETIME4:
	memcpy(dest,src,sizeof(TDS_DATETIME4));
	return(sizeof(TDS_DATETIME4));
      default:
         return TDS_FAIL;
	break;
   }
	return TDS_FAIL;
}

TDS_INT tds_convert_real(int srctype,unsigned char *src,int desttype,unsigned
char *dest,TDS_INT destlen)
{
TDS_REAL the_value;

   memcpy(&the_value, src, 4);
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
      /* A real type has a 7 digit precision plus a two digit exponent
       *   "-d.dddddde+dd"
       * so it should have >=14 bytes allocated (one for the terminating
       * NULL), right?
       */
         if (destlen >= 0 && destlen < 14)  return TDS_FAIL;
         sprintf(dest,"%.7g", the_value);
         return strlen(dest);
      case SYBFLT8:
         *((TDS_FLOAT *)dest) = the_value;
	 return sizeof(TDS_FLOAT);
      case SYBREAL:
	 memcpy(dest,src,sizeof(TDS_REAL));
         return sizeof(TDS_REAL);
   }
   return TDS_FAIL;
}

TDS_INT tds_convert_flt8(int srctype,unsigned char *src,int desttype,unsigned
char *dest,TDS_INT destlen)
{
TDS_FLOAT the_value;

   memcpy(&the_value, src, 8);
   switch(desttype) {
      case SYBCHAR:
      case SYBVARCHAR:
      /* A flt8 type has a 15 digit precision plus a three digit exponent
       *   "-d.dddddddddddddde+ddd"
       * so it should have >=23 bytes allocated (one for the terminating
       * NULL), right?
       */
         if (destlen >= 0 && destlen < 23)  return TDS_FAIL;
         sprintf(dest,"%.15g", the_value);
         return strlen(dest);
      case SYBREAL:
         *((TDS_REAL *)dest) = the_value;
	 return sizeof(TDS_REAL);
      case SYBFLT8:
	 memcpy(dest,src,sizeof(TDS_FLOAT));
	 return sizeof(TDS_FLOAT);
   }
   return TDS_FAIL;
}

TDS_INT tds_convert_any(unsigned char *dest, TDS_INT dtype, TDS_INT dlen, DBANY *any)
{
int i;

	switch(dtype) {
		case SYBCHAR:
		case SYBVARCHAR:
			tdsdump_log(TDS_DBG_INFO1, "%L converting string dlen = %d dtype = %d string = %s\n",dlen,dtype,any->c);
			if (dlen && strlen(any->c)>dlen) {
				strncpy(dest,any->c,dlen-1);
				dest[dlen-1]='\0';
				for (i=strlen(dest)-1;dest[i]==' ';i--)
					dest[i]='\0';
				return dlen;
			} else {
				strcpy(dest, any->c);
				for (i=strlen(dest)-1;dest[i]==' ';i--)
					dest[i]='\0';
				return strlen(dest);
			}
			break;
		case SYBTEXT:
			break;
		case SYBBINARY:
		case SYBIMAGE:
			break;
		case SYBINT1:
			memcpy(dest,&(any->ti),1);
			return 1;
			break;
		case SYBINT2:
			memcpy(dest,&(any->si),2);
			return 2;
			break;
		case SYBINT4:
			memcpy(dest,&(any->i),4);
			return 4;
			break;
		case SYBFLT8:
			memcpy(dest,&(any->f),8);
			return 8;	
			break;
		case SYBREAL:
			memcpy(dest,&(any->r),4);
			return 4;	
			break;
		case SYBBIT:
		case SYBBITN:
			memcpy(dest,&(any->ti),1);
			return 1;
			break;
		case SYBMONEY:
			break;
		case SYBMONEY4:
			break;
		case SYBDATETIME:
			memcpy(dest,&(any->dt),sizeof(TDS_DATETIME));
			break;
		case SYBDATETIME4:
			memcpy(dest,&(any->dt4),sizeof(TDS_DATETIME4));
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			break;
/*
		case SYBBOUNDRY:
			break;
		case SYBSENSITIVITY:
			break;
*/
	}
	return TDS_FAIL;
}
TDS_INT tds_convert(int srctype,
		TDS_CHAR *src,
		TDS_UINT srclen,
		int desttype,
		TDS_CHAR *dest,
		TDS_UINT destlen)
{
TDS_VARBINARY *varbin;

	switch(srctype) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBNVARCHAR:
			return tds_convert_char(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBMONEY4:
			return tds_convert_money4(srctype,src,srclen,
				desttype,dest,destlen);
			break;
		case SYBMONEY:
			return tds_convert_money(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBNUMERIC:
		case SYBDECIMAL:
			return tds_convert_numeric(srctype,(TDS_NUMERIC *) src,srclen,
				desttype,dest,destlen);
			break;
		case SYBBIT:
		case SYBBITN:
			return tds_convert_bit(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBINT1:
			return tds_convert_int1(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBINT2:
			return tds_convert_int2(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBINT4:
			return tds_convert_int4(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBREAL:
			return tds_convert_real(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBFLT8:
			return tds_convert_flt8(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBDATETIME:
			return tds_convert_datetime(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBDATETIME4:
			return tds_convert_datetime4(srctype,src,
				desttype,dest,destlen);
			break;
		case SYBVARBINARY:
			varbin = (TDS_VARBINARY *)src;
			return tds_convert_binary(srctype,varbin->array,
				varbin->len,desttype,dest,destlen);
			break;
		case SYBIMAGE:
		case SYBBINARY:
			return tds_convert_binary(srctype,src,srclen,
				desttype,dest,destlen);
			break;
		case SYBTEXT:
			return tds_convert_text(srctype,src,srclen,
				desttype,dest,destlen);
			break;
		case SYBNTEXT:
			return tds_convert_ntext(srctype,src,srclen,
				desttype,dest,destlen);
			break;
		default:
			fprintf(stderr,"Attempting to convert unknown source type %d\n",srctype);

	}
	return TDS_FAIL;
}
static int _string_to_tm(char *datestr, struct tm *t)
{
enum {TDS_MONTH = 1, TDS_DAY, TDS_YEAR, TDS_HOUR, TDS_MIN, TDS_SEC, TDS_MILLI};
int state = TDS_MONTH;
char last_char=0, *s;

	memset(t,'\0',sizeof(struct tm));

	for (s=datestr;*s;s++) {
		if (! isdigit(*s) && isdigit(last_char)) {
			state++;
		} else switch(state) {
			case TDS_MONTH:
				t->tm_mon = (t->tm_mon * 10) + (*s - '0');
				break;
			case TDS_DAY:
				t->tm_mday = (t->tm_mday * 10) + (*s - '0');
				break;
			case TDS_YEAR:
				t->tm_year = (t->tm_year * 10) + (*s - '0');
				break;
			case TDS_HOUR:
				t->tm_hour = (t->tm_hour * 10) + (*s - '0');
				break;
			case TDS_MIN:
				t->tm_min = (t->tm_min * 10) + (*s - '0');
				break;
			case TDS_SEC:
				t->tm_sec = (t->tm_sec * 10) + (*s - '0');
				break;
		}
		last_char=*s;
	}
	return 0;
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

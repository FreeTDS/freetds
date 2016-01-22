/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2005-2008 Frediano Ziglio
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

#ifndef _tdsbytes_h_
#define _tdsbytes_h_

#ifndef _tds_h_
#error tds.h must be included before bytes.h
#endif

/*
 * read a word of n bytes aligned, architecture dependent endian
 *  TDS_GET_An
 * read a word of n bytes aligned, little endian
 *  TDS_GET_AnLE
 * read a word of n bytes aligned, big endian
 *  TDS_GET_AnBE
 * read a word of n bytes unaligned, architecture dependent endian
 *  TDS_GET_UAn
 * read a word of n bytes unaligned, little endian
 *  TDS_GET_UAnLE
 * read a word of n bytes unaligned, big endian
 *  TDS_GET_UAnBE
 */

/* TODO optimize (use swap, unaligned platforms) */

/* one byte, easy... */
#define TDS_GET_A1LE(ptr)  (((TDS_UCHAR*)(ptr))[0])
#define TDS_GET_A1BE(ptr)  TDS_GET_A1LE(ptr)
#define TDS_GET_UA1LE(ptr) TDS_GET_A1LE(ptr)
#define TDS_GET_UA1BE(ptr) TDS_GET_A1LE(ptr)

#define TDS_PUT_A1LE(ptr,val)  do { ((TDS_UCHAR*)(ptr))[0] = (val); } while(0)
#define TDS_PUT_A1BE(ptr,val)  TDS_PUT_A1LE(ptr,val)
#define TDS_PUT_UA1LE(ptr,val) TDS_PUT_A1LE(ptr,val)
#define TDS_PUT_UA1BE(ptr,val) TDS_PUT_A1LE(ptr,val)

/* two bytes */
#define TDS_GET_UA2LE(ptr) (((TDS_UCHAR*)(ptr))[1] * 0x100u + ((TDS_UCHAR*)(ptr))[0])
#define TDS_GET_UA2BE(ptr) (((TDS_UCHAR*)(ptr))[0] * 0x100u + ((TDS_UCHAR*)(ptr))[1])
#define TDS_GET_A2LE(ptr) TDS_GET_UA2LE(ptr)
#define TDS_GET_A2BE(ptr) TDS_GET_UA2BE(ptr)

#define TDS_PUT_UA2LE(ptr,val) do {\
 ((TDS_UCHAR*)(ptr))[1] = (TDS_UCHAR)((val)>>8); ((TDS_UCHAR*)(ptr))[0] = (TDS_UCHAR)(val); } while(0)
#define TDS_PUT_UA2BE(ptr,val) do {\
 ((TDS_UCHAR*)(ptr))[0] = (TDS_UCHAR)((val)>>8); ((TDS_UCHAR*)(ptr))[1] = (TDS_UCHAR)(val); } while(0)
#define TDS_PUT_A2LE(ptr,val) TDS_PUT_UA2LE(ptr,val)
#define TDS_PUT_A2BE(ptr,val) TDS_PUT_UA2BE(ptr,val)

/* four bytes */
#define TDS_GET_UA4LE(ptr) \
	(((TDS_UCHAR*)(ptr))[3] * 0x1000000u + ((TDS_UCHAR*)(ptr))[2] * 0x10000u +\
	 ((TDS_UCHAR*)(ptr))[1] * 0x100u + ((TDS_UCHAR*)(ptr))[0])
#define TDS_GET_UA4BE(ptr) \
	(((TDS_UCHAR*)(ptr))[0] * 0x1000000u + ((TDS_UCHAR*)(ptr))[1] * 0x10000u +\
	 ((TDS_UCHAR*)(ptr))[2] * 0x100u + ((TDS_UCHAR*)(ptr))[3])
#define TDS_GET_A4LE(ptr) TDS_GET_UA4LE(ptr)
#define TDS_GET_A4BE(ptr) TDS_GET_UA4BE(ptr)

#define TDS_PUT_UA4LE(ptr,val) do {\
 ((TDS_UCHAR*)(ptr))[3] = (TDS_UCHAR)((val)>>24); ((TDS_UCHAR*)(ptr))[2] = (TDS_UCHAR)((val)>>16);\
 ((TDS_UCHAR*)(ptr))[1] = (TDS_UCHAR)((val)>>8); ((TDS_UCHAR*)(ptr))[0] = (TDS_UCHAR)(val); } while(0)
#define TDS_PUT_UA4BE(ptr,val) do {\
 ((TDS_UCHAR*)(ptr))[0] = (TDS_UCHAR)((val)>>24); ((TDS_UCHAR*)(ptr))[1] = (TDS_UCHAR)((val)>>16);\
 ((TDS_UCHAR*)(ptr))[2] = (TDS_UCHAR)((val)>>8); ((TDS_UCHAR*)(ptr))[3] = (TDS_UCHAR)(val); } while(0)
#define TDS_PUT_A4LE(ptr,val) TDS_PUT_UA4LE(ptr,val)
#define TDS_PUT_A4BE(ptr,val) TDS_PUT_UA4BE(ptr,val)

#if defined(__GNUC__)
#  define TDS_MAY_ALIAS __attribute__((__may_alias__))
#else
#  define TDS_MAY_ALIAS
#endif

typedef union {
	TDS_USMALLINT usi;
	TDS_UCHAR uc[2];
} TDS_MAY_ALIAS TDS_BYTE_CONVERT2;

typedef union {
	TDS_UINT ui;
	TDS_UCHAR uc[4];
} TDS_MAY_ALIAS TDS_BYTE_CONVERT4;

/* architecture dependent */
/* map to generic macros or redefine for aligned and same endianess */
#ifdef WORDS_BIGENDIAN
# define TDS_GET_A1(ptr)  TDS_GET_A1BE(ptr)
# define TDS_GET_UA1(ptr) TDS_GET_UA1BE(ptr)
# define TDS_GET_A2(ptr)  TDS_GET_A2BE(ptr)
# define TDS_GET_UA2(ptr) TDS_GET_UA2BE(ptr)
# define TDS_GET_A4(ptr)  TDS_GET_A4BE(ptr)
# define TDS_GET_UA4(ptr) TDS_GET_UA4BE(ptr)
# undef TDS_GET_A2BE
# undef TDS_GET_A4BE
# define TDS_GET_A2BE(ptr) (((TDS_BYTE_CONVERT2*)(ptr))->usi)
# define TDS_GET_A4BE(ptr) (((TDS_BYTE_CONVERT4*)(ptr))->ui)

# define TDS_PUT_A1(ptr,val)  TDS_PUT_A1BE(ptr,val)
# define TDS_PUT_UA1(ptr,val) TDS_PUT_UA1BE(ptr,val)
# define TDS_PUT_A2(ptr,val)  TDS_PUT_A2BE(ptr,val)
# define TDS_PUT_UA2(ptr,val) TDS_PUT_UA2BE(ptr,val)
# define TDS_PUT_A4(ptr,val)  TDS_PUT_A4BE(ptr,val)
# define TDS_PUT_UA4(ptr,val) TDS_PUT_UA4BE(ptr,val)
# undef TDS_PUT_A2BE
# undef TDS_PUT_A4BE
# define TDS_PUT_A2BE(ptr,val) (((TDS_BYTE_CONVERT2*)(ptr))->usi = (val))
# define TDS_PUT_A4BE(ptr,val) (((TDS_BYTE_CONVERT4*)(ptr))->ui = (val))
# define TDS_HOST2LE(val) TDS_BYTE_SWAP16(val)
# define TDS_HOST4LE(val) TDS_BYTE_SWAP32(val)
# define TDS_HOST2BE(val) (val)
# define TDS_HOST4BE(val) (val)
#else
# define TDS_GET_A1(ptr)  TDS_GET_A1LE(ptr)
# define TDS_GET_UA1(ptr) TDS_GET_UA1LE(ptr)
# define TDS_GET_A2(ptr)  TDS_GET_A2LE(ptr)
# define TDS_GET_UA2(ptr) TDS_GET_UA2LE(ptr)
# define TDS_GET_A4(ptr)  TDS_GET_A4LE(ptr)
# define TDS_GET_UA4(ptr) TDS_GET_UA4LE(ptr)
# undef TDS_GET_A2LE
# undef TDS_GET_A4LE
# define TDS_GET_A2LE(ptr) (((TDS_BYTE_CONVERT2*)(ptr))->usi)
# define TDS_GET_A4LE(ptr) (((TDS_BYTE_CONVERT4*)(ptr))->ui)

# define TDS_PUT_A1(ptr,val)  TDS_PUT_A1LE(ptr,val)
# define TDS_PUT_UA1(ptr,val) TDS_PUT_UA1LE(ptr,val)
# define TDS_PUT_A2(ptr,val)  TDS_PUT_A2LE(ptr,val)
# define TDS_PUT_UA2(ptr,val) TDS_PUT_UA2LE(ptr,val)
# define TDS_PUT_A4(ptr,val)  TDS_PUT_A4LE(ptr,val)
# define TDS_PUT_UA4(ptr,val) TDS_PUT_UA4LE(ptr,val)
# undef TDS_PUT_A2LE
# undef TDS_PUT_A4LE
# define TDS_PUT_A2LE(ptr,val) (((TDS_BYTE_CONVERT2*)(ptr))->usi = (val))
# define TDS_PUT_A4LE(ptr,val) (((TDS_BYTE_CONVERT4*)(ptr))->ui = (val))
# define TDS_HOST2LE(val) (val)
# define TDS_HOST4LE(val) (val)
# define TDS_HOST2BE(val) TDS_BYTE_SWAP16(val)
# define TDS_HOST4BE(val) TDS_BYTE_SWAP32(val)
#endif

/* these platform support unaligned fetch/store */
/* map unaligned macro to aligned ones */
#if defined(__i386__) || defined(__amd64__) || defined(__CRIS__) ||\
  defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__) ||\
  defined(__s390__) || defined(__s390x__) || defined(__m68k__) ||\
  (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_IX86) || defined(_M_X64))) ||\
  defined(__ARM_FEATURE_UNALIGNED) ||\
  defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_8__) ||\
  (defined(_M_ARM) && (_M_ARM >= 7))
# ifdef WORDS_BIGENDIAN
#  undef TDS_GET_UA2BE
#  undef TDS_GET_UA4BE
#  define TDS_GET_UA2BE(ptr) TDS_GET_A2BE(ptr)
#  define TDS_GET_UA4BE(ptr) TDS_GET_A4BE(ptr)

#  undef TDS_PUT_UA2BE
#  undef TDS_PUT_UA4BE
#  define TDS_PUT_UA2BE(ptr,val) TDS_PUT_A2BE(ptr,val)
#  define TDS_PUT_UA4BE(ptr,val) TDS_PUT_A4BE(ptr,val)
# else
#  undef TDS_GET_UA2LE
#  undef TDS_GET_UA4LE
#  define TDS_GET_UA2LE(ptr) TDS_GET_A2LE(ptr)
#  define TDS_GET_UA4LE(ptr) TDS_GET_A4LE(ptr)

#  undef TDS_PUT_UA2LE
#  undef TDS_PUT_UA4LE
#  define TDS_PUT_UA2LE(ptr,val) TDS_PUT_A2LE(ptr,val)
#  define TDS_PUT_UA4LE(ptr,val) TDS_PUT_A4LE(ptr,val)
# endif
#endif

#if defined(__linux__) && defined(__GNUC__) && (defined(__i386__) || defined(__amd64__))
# include <byteswap.h>
# undef TDS_GET_UA2BE
# undef TDS_GET_UA4BE
# define TDS_GET_UA2BE(ptr) ({ TDS_USMALLINT _tds_si = TDS_GET_UA2LE(ptr); bswap_16(_tds_si); })
# define TDS_GET_UA4BE(ptr) ({ TDS_UINT _tds_i = TDS_GET_UA4LE(ptr); bswap_32(_tds_i); })

# undef TDS_PUT_UA2BE
# undef TDS_PUT_UA4BE
# define TDS_PUT_UA2BE(ptr,val) do {\
   TDS_USMALLINT _tds_si = bswap_16(val); TDS_PUT_UA2LE(ptr,_tds_si); } while(0)
# define TDS_PUT_UA4BE(ptr,val) do {\
   TDS_UINT _tds_i = bswap_32(val); TDS_PUT_UA4LE(ptr,_tds_i); } while(0)
#endif

#if defined(__GNUC__) && defined(__powerpc__)
# undef TDS_GET_UA2LE
# undef TDS_GET_UA4LE
static inline TDS_USMALLINT
TDS_GET_UA2LE(void *ptr)
{
	unsigned long res;
	__asm__ ("lhbrx %0,0,%1\n" : "=r" (res) : "r" (ptr), "m"(*(TDS_USMALLINT*)ptr));
	return (TDS_USMALLINT) res;
}
static inline TDS_UINT
TDS_GET_UA4LE(void *ptr)
{
	unsigned long res;
	__asm__ ("lwbrx %0,0,%1\n" : "=r" (res) : "r" (ptr), "m"(*(TDS_UINT*)ptr));
	return (TDS_UINT) res;
}

# undef TDS_PUT_UA2LE
# undef TDS_PUT_UA4LE
static inline void
TDS_PUT_UA2LE(void *ptr, unsigned data)
{
    __asm__ ("sthbrx %1,0,%2\n" : "=m" (*(TDS_USMALLINT *)ptr) : "r" (data), "r" (ptr));
}
static inline void
TDS_PUT_UA4LE(void *ptr, unsigned data)
{
    __asm__ ("stwbrx %1,0,%2\n" : "=m" (*(TDS_UINT *)ptr) : "r" (data), "r" (ptr));
}
#endif

#endif

/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef DJTYPES_H
#define DJTYPES_H

// WARNING - DEVIATION FROM STRICT ISO C PORTABILITY.
// My C code is normally designed for efficiency, and for normal but not "extreme"
// portability. In particular I assume that the CPU implements twos complement
// arithmetic, that a right shift of a signed integer preserves the msb, and
// probably also that little-endian byte order is used. These assumptions are
// valid on all processors that I care about.

// -------------------------------------------------------------------
#define UNICODE_FILENAMES 0
#define PUBLIC
#define PRIVATE static
// -------------------------------------------------------------------

// generic integer types to be used as loop counters, array indices etc
// when I don't care too much about range or size. Often I won't bother
// using these, instead I'll just use "int" or "unsigned"
typedef unsigned int uint_t;
typedef signed int   int_t;

// -------------------------------------------------------------------
// remaining integer types have an explicit size.

// 8 bit type, unsigned
typedef unsigned char UI08;
typedef UI08 *PUI08;
typedef const UI08 *CPUI08;

// 8 bit type, signed
typedef signed char CHAR;
typedef CHAR SI08;
typedef CHAR *PCHAR;
typedef const CHAR *CPCHAR;

// 16 bit type, unsigned
typedef unsigned short UI16;
typedef UI16 *PUI16;
typedef const UI16 *CPUI16;

// 16 bit type, signed
typedef signed short SI16;
typedef SI16 *PSI16;
typedef const SI16 *CPSI16;

// 32 bit type, unsigned
typedef unsigned int UI32;
typedef UI32 *PUI32;
typedef const UI32 *CPUI32;

// 32 bit type, signed
typedef signed int SI32;
typedef SI32 *PSI32;
typedef const SI32 *CPSI32;

// 64 bit type, unsigned
typedef unsigned long long UI64;
typedef UI64 *PUI64;
typedef const UI64 *CPUI64;

// 64 bit type, signed
typedef long long SI64;
typedef SI64 *PSI64;
typedef const SI64 *CPSI64;
typedef SI64 HUGE;
typedef SI64 *PHUGE;
typedef const SI64 *CPHUGE;

// -------------------------------------------------------------------

#if UNICODE_FILENAMES
typedef unsigned short FNCHAR;
#else
typedef unsigned char FNCHAR;
#endif
typedef FNCHAR *PFN;
typedef const FNCHAR *CPFN;

// -------------------------------------------------------------------
/* Some compilers I use do not have native 64bit arithmetic. I've struggled for
 * a long time to get 64 bit integer arithmetic support in a portable way.
 * These macros sometimes help. This is legacy support, sometimes I use these
 * macros, other times I'll just assume native 64bit arithmetic.
 *
 * Portability notes: shifts by 32 may not work on all architectures, even
 * if they have a native 64 bit int. In that case the macros may need to
 * be replaced with something else, eg. a function call or an intrinsic.
 */
#define LO32(h) ((UI32)(h))
#define HI32(h) ((UI32)((h)>>32))
#define MAKEHUGE(lo,hi) ((lo)+(((HUGE)(hi))<<32))

#define hugeop_shl(dest, src, c) dest = ((src)<<(c))
#define hugeop_shr(dest, src, c) dest = ((src)>>(c))

#define hugeop_add(dest, x, y) dest = ((x)+(y))
#define hugeop_sub(dest, x, y) dest = ((x)-(y))

#define hugeop_fromuint(dest, u32) dest = (u32)
#define hugeop_adduint(dest, src, u32) dest = ((src)+(u32))
#define hugeop_subuint(dest, src, u32) dest = ((src)-(u32))

#define hugeop_compare(x,y) ((x)-(y))

// -------------------------------------------------------------------
/* If I'm doing object oriented type programming in C then these macros
 * are not required, but do help make my intentions clearer. I still
 * have to pass a "pThis" argument in method calls explicitly - but I
 * actually prefer that (I'm deeply suspicious of code doing things
 * behind my back).
 */
#define CLASS(clsname) struct t_##clsname
#define PUBLIC_METHOD(name) (*name)

// -------------------------------------------------------------------
/* Define a set of Windows compatible types.
 */
#ifndef WINVER

typedef void *PVOID;

typedef unsigned char BYTE;
typedef BYTE *PBYTE;

typedef CHAR *PSTR;

typedef UI32 BOOL;
typedef BOOL *PBOOL;

#define TRUE  1
#define FALSE 0

#define NULL  ((CPVOID)0)

typedef UI32 UINT;
typedef UI16 WORD;
typedef UI32 DWORD;
typedef UI16 WCHAR;

typedef UI32 *PUINT;
typedef UI16 *PWORD;
typedef UI32 *PDWORD;
typedef UI16 *PWCHAR;

#define MAKEWORD(lo,hi) ((WORD)((lo)+(((WORD)(hi))<<8)))
#define MAKELONG(lo,hi) ((SI32)((lo)+(((SI32)(hi))<<16)))

#endif

typedef const void  *CPVOID;
typedef const BYTE  *CPBYTE;
typedef const BOOL  *CPBOOL;
typedef const CHAR  *CPSTR;
typedef const WCHAR *CPWCHAR;

#ifndef WINVER
typedef CPVOID HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)(PSI32)-1)
#endif

// -------------------------------------------------------------------
// UUID type, as used by VirtualBox.
//
typedef union {
   UI08 au8[16];  // 8-bit form.
   UI16 au16[8];  // 16-bit form.
   UI32 au32[4];  // 32-bit form.
   UI64 au64[2];  // 64-bit form.
   struct {       // Official DCE form.
      UI32 u32TimeLow;
      UI16 u16TimeMid;
      UI16 u16TimeHiAndVersion;
      UI08 u8ClockSeqHiAndReserved;
      UI08 u8ClockSeqLow;
      UI08 au8Node[6];
   } Gen;
} S_UUID;

// -------------------------------------------------------------------

#endif

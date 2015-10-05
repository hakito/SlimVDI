/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djstring.h"

/*.....................................................*/

PUBLIC SI32
String_Length(CPCHAR s)
{
   SI32 L;
   for (L=0; *s; s++) L++;
   return L;
}

/*.....................................................*/

PUBLIC SI32
String_LengthW(CPWCHAR s)
{
   SI32 L;
   for (L=0; *s; s++) L++;
   return L;
}

/*.....................................................*/

PUBLIC SI32
String_Copy(PCHAR szDest, CPCHAR szSrc, int MaxLen)
{
   SI32 len = 0;
   if (MaxLen) {
      MaxLen--; // leave room for NUL
      for (; MaxLen; MaxLen--) {
         CHAR c = *szSrc++;
         if (!c) break;
         *szDest++ = c;
         len++;
      }
      *szDest = (CHAR)0;
   }
   return len;
}

/*.....................................................*/

PUBLIC SI32
String_CopyW(PWCHAR szDest, CPWCHAR szSrc, int MaxLen)
{
   SI32 len = 0;
   if (MaxLen) {
      MaxLen--; // leave room for NUL
      for (; MaxLen; MaxLen--) {
         WCHAR c = *szSrc++;
         if (!c) break;
         *szDest++ = c;
         len++;
      }
      *szDest = (WCHAR)0;
   }
   return len;
}

/*.....................................................*/

PUBLIC SI32
String_Compare(CPCHAR s1, CPCHAR s2)
{
   SI32 d,c1,c2;
   do {
      c1 = *s1++;
      c2 = *s2++;
      d = c1-c2;
      if (d!=0) return d;
   } while (c1);
   return 0;
}

/*.....................................................*/

PUBLIC SI32
String_CompareW(CPWCHAR s1, CPWCHAR s2)
{
   SI32 d,c1,c2;
   do {
      c1 = *s1++;
      c2 = *s2++;
      d = c1-c2;
      if (d!=0) return d;
   } while (c1);
   return 0;
}

/*.....................................................*/

PUBLIC SI32
String_CompareCI(CPCHAR s1, CPCHAR s2)
{
   SI32 d,c1,c2;
   do {
      c1 = *s1++;
      if (c1>='a' && c1<='z') c1-=32;
      c2 = *s2++;
      if (c2>='a' && c2<='z') c2-=32;
      d = c1-c2;
      if (d!=0) return d;
   } while (c1);
   return 0;
}

/*.....................................................*/

PUBLIC SI32
String_CompareCIW(CPWCHAR s1, CPWCHAR s2)
{
   SI32 d,c1,c2;
   do {
      c1 = *s1++;
      if (c1>='a' && c1<='z') c1-=32; // only works with roman char set.
      c2 = *s2++;
      if (c2>='a' && c2<='z') c2-=32; // only works with roman char set.
      d = c1-c2;
      if (d!=0) return d;
   } while (c1);
   return 0;
}

/*.....................................................*/

/* end of module djstring.c */


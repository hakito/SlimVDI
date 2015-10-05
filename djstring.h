/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef DJSTRING_H
#define DJSTRING_H

/*======================================================================*/
/*                      Basic string manipulation                       */
/*======================================================================*/

#include "djtypes.h"

SI32 String_Length(CPCHAR s);
SI32 String_LengthW(CPWCHAR s);

SI32 String_Copy(PCHAR szDest, CPCHAR szSrc, SI32 MaxLen);
SI32 String_CopyW(PWCHAR szDest, CPWCHAR szSrc, SI32 MaxLen);
// Returns string length. MaxLen is the size of the buffer 'szDest' in
// characters, hence the max return length is MaxLen-1.

SI32 String_Compare(CPCHAR sz1, CPCHAR sz2);
SI32 String_CompareW(CPWCHAR sz1, CPWCHAR sz2);
// Case sensitive comparison. Returns <0 if sz1<sz2, ==0 if sz1==sz2,
// and >0 if sz1>sz2.

SI32 String_CompareCI(CPCHAR sz1, CPCHAR sz2);
SI32 String_CompareCIW(CPWCHAR sz1, CPWCHAR sz2);
// Case insensitive comparison. Result meaning as above.

#endif


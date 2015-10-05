/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef MEM_H
#define MEM_H

#include "djtypes.h"

#define MEMF_ZEROINIT 1
#define MEMF_DDESHARE 2
#define MEMF_FIXED    4

PVOID Mem_Alloc(UINT flags, UINT size);
/* Allocate a block of memory and return a pointer. Returns NULL on failure.
 */

PVOID Mem_ReAlloc(PVOID p, UINT flags, UINT size);
/* Changes the size of a memory object. Bear in mind the following :-
 *
 *    1. If this function succeeds then the old pointer will be invalidated.
 *    2. If this function fails then the function returns NULL, but the old pointer
 *       is still valid.
 *    3. Passing NULL to this function is perfectly valid. In that case it simply
 *       does a normal allocation.
 */


PVOID Mem_Free(PVOID p);
/* Frees a memory block. This is a nop (no crash) if the p argument is NULL.
 */

void Mem_Copy(PVOID pDest, CPVOID pSrc, UINT nBytes);
void Mem_Move(PVOID pDest, CPVOID pSrc, UINT nBytes);

void Mem_Zero(PVOID pDest, UINT nBytes);
void Mem_Fill(PVOID pDest, UINT nBytes, BYTE FillByte);

int  Mem_Compare(CPVOID pBlock1, CPVOID pBlock2, UINT nBytes);
/* Works like a string compare, except no NUL terminator is expected, instead
 * the compare length is bounded by nBytes argument. Returns 0 if the memory
 * blocks are equal (up to nBytes), negative if the first different byte value
 * in block 1 is less than the same byte in block 2, positive if greater.
 */

#endif


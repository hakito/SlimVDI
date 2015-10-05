/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef FILENAME_H
#define FILENAME_H

/*======================================================================*/
/* Module which provides widely useful manipulations of filenames       */
/*======================================================================*/

#include "djtypes.h"

/*------------------------ Filename Processing -------------------------*/

void  Filename_SplitPath(CPFN path, PFN head, PFN tail);
/* Separates the path argument into head and tail elements. The head is
 * everything up to and not including the last path separator, tail is everything
 * after the last path separator. Either of the head or tail arguments
 * can be NULL if you don't need that result. Yes, both can be NULL, but
 * that would be rather pointless!
 */
 
void  Filename_SplitTail(CPFN path, PFN tail);
/* Legacy function, equivalent to calling SplitPath(path,NULL,tail);
 */

void  Filename_MakePath(PFN path, CPFN head, CPFN tail);
/* Extends an existing path (head) with a new filename or subdirectory name
 * in "tail", returning the result in the "path" argument. path and head
 * may point to the same variable, but tail should be a different
 * buffer.
 */

void  Filename_AddExtension(PFN fn, CPFN ext);
void  Filename_GetExtension(CPFN fn, PFN ext);
void  Filename_RemoveExtension(PFN s);
void  Filename_ChangeExtension(PFN s, CPFN ext);
BOOL  Filename_IsExtension(CPFN s, CPFN ext);

int   Filename_Length(CPFN fn);
int   Filename_Copy(PFN fnDest, CPFN fnSrc, int MaxLen); // result is the number of FNCHARS written to fnDest, INCLUDING the NUL terminator.

int   Filename_Compare(CPFN fn1, CPFN fn2);
/* This works like a standard case-insensitive C string compare,
 * returning <0, 0, or >0 to indicate the order of the match.
 */

/*----------------------------------------------------------------------*/

#endif


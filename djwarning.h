/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef DJWARNING_H
#define DJWARNING_H

#define STRICT

/* Disable warning 4057: arg x differs from SLIGHTLY different base type - too bloody annoying to keep */
/* Disable warning 4100: "Unreferenced formal parameter" - because in my experience this is never an error */

/* Disable warning 4201: "Nonstandard extension - nameless struct */
/* Disable warning 4214: "Nonstandard extension - bitfield type not an int */
/* these warnings disabled because they are used in Win32 header files, which I can do little about */

#pragma warning(disable:4057 4100 4201 4214)

#endif

/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/
#ifndef CLONE_H
#define CLONE_H

#include "vddr.h"
#include "parms.h"

BOOL Clone_Proceed(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm);
/* Clones a source disk image to a destination VDI, leaving the cloned VDI in the dest
 * folder - unless an error occurs, in which case the bad dest VDI is deleted.
 *
 * The clone options and other parameters are passed in the "parm" argument (see parms.h
 * for a definition).
 *
 * The source and destination filenames may be the same. In that case this function writes the
 * clone to a temp name first, and if cloning is successful it will later rename both files,
 * leaving the new clone with old name (the old file "xxx" is renamed to "Original xxx").
 */

BOOL Clone_CompareImages(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm);
/* Debug function not used by release app.
 */

#endif


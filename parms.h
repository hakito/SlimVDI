/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef PARMS_H
#define PARMS_H

#include "djtypes.h"
#include "vddr.h"

// bits in 'flags' field of parameters structure.
#define PARM_FLAG_KEEPUUID   1 /* copy the creation UUID from the source drive */
#define PARM_FLAG_ENLARGE    2 /* enlarge dest max drive size */
#define PARM_FLAG_REPART     4 /* resize main partition to fill enlarged drive */
#define PARM_FLAG_COMPACT    8 /* discard unused blocks from guest filesystem */
#define PARM_FLAG_FIXMBR    16 /* flag automatically set if enlarging a VDI which starts off less than 8GB */
#define PARM_FLAG_NOMERGE   PARM_FLAG_FIXMBR << 1 /* do not merge snapshot chain */
#define PARM_FLAG_CLIMODE   0x80000000 /* command line interface mode - errors written to stdout instead of MessageBox() */

typedef struct {
   UINT flags;                  // see above flags.
   FNCHAR srcfn[1024];          // source filename and path.
   FNCHAR dstfn[1024];          // dest filename. If this has no path then "same as source path" is assumed.
   BYTE MBR[512];               // The source disk MBR is read validation, and kept around for later checks.
   HVDDR hVDIsrc;               // The cloning code makes no used of this source disk handle, it's a legacy of validation.
   CHAR  szDestSize[32];        // Destination disk size supplied by user. Ignored if ENLARGE flag not set.
   UINT  DestSectors;           // clone code internally converts szDestSize[] string into this.
   UINT  dst_nBlocks;           // clone code calculates this: private.
   UINT  dst_nBlocksAllocated;  // clone code calculates this: private.
   UINT  nMappedParts;          // clone code calculates this: private.
} s_CLONEPARMS;

#endif


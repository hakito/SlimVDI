/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef PARALLELS_H
#define PARALLELS_H

extern PSTR HEADER_MAGIC /* = "WithoutFreeSpace" */;

#define HEADER_VERSION 2
#define HEADER_SIZE    64

// special values for block map
#define HDD_PAGE_FREE  0

typedef struct {
   CHAR szSig[16];       // "WithoutFreeSpace"
   UI32 u32Version;      // Should match HEADER_VERSION
   UI32 u32Heads;
   UI32 u32Cylinders;
   UI32 u32BlockSize;    // In sectors. (qemu-img called this field "tracks"
   UI32 nBlocks;         // (qemu-img called this field "catalog_entries")
   UI32 DriveSize;       // In sectors.
   BYTE pad[24];
// UINT BlockMap[nBlocks]; // each entry seems to be an absolute sector offset into HDD file.
// BYTE pad2[pad to 512 byte boundary]
// BYTE DiskImage[512*DriveSize];
} HDD_HEADER;

#endif

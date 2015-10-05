/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VHDSTRUCTS_H
#define VHDSTRUCTS_H

/*=========================================================================*/
/* Structures embedded in Virtual PC VHD files.                            */
/*=========================================================================*/

#include "djtypes.h"

// Currently supported VHD format
#define VHD_FORMAT_MAJOR_VER  1

// VHD image types
#define VHD_TYPE_NONE         0 /* I have no idea what this means */
#define VHD_TYPE_RESERVED_1   1 /* deprecated */
#define VHD_TYPE_FIXED        2
#define VHD_TYPE_DYNAMIC      3
#define VHD_TYPE_DIFFERENCING 4 /* always dynamic, since a block map is implied */
#define VHD_TYPE_RESERVED_2   5 /* deprecated */
#define VHD_TYPE_RESERVED_3   6 /* deprecated */
/* all other types are also reserved */

// VHD footer flags
#define VHD_FOOTFLAG_TEMP     1
#define VHD_FOOTFLAG_RESERVED 2

// Special values in block map
#define VHD_PAGE_FREE 0xFFFFFFFF /* indicates a free (unallocated) block. Reads from this block return random data. */

// Disk Geometry Structure
typedef struct {
   WORD u16Cylinders;
   BYTE u8Heads;
   BYTE u8SectorsPerTrack;
} VHD_DISK_GEOMETRY;

// Parent locator data (only used by differencing VHDs).
typedef struct {
   UINT u32PlatformCode;
   UINT u32PlatformDataSpace;
   UINT u32PlatformDataLength;
   UINT u32Reserved;
   HUGE u64PlatformDataOffset;
} VHD_PARENT_LOCATOR;

// VHD footer format. Remember that all structures in the VHD use big endian byte order.
// In a fixed type VHD the footer is mirrored at the start of the file, followed by the image data (I think).
// In a dynamic or differencing VHD this structure is mirrored at start of file, followed by dyn header, then block map and image.
typedef struct {
   BYTE cookie[8];        // The string "conectix"
   UINT u32Features;      // flag bits. See VHD_FOOTFLAG_xxx defs. RESERVED bit must always be 1.
   UINT u32FmtVer;        // VHD format version.
   HUGE u64DataOffset;    // Absolute byte offset to "next structure", whatever that means in a footer!
   UINT u32TimeStamp;     // Despite what the docs say, this has to be a modification time stamp. Secs since 12:00:00am on Jan 1st, 2000 (UTC/GMT).
   BYTE u32CreatorApp[4]; // This is a 4 byte string. Eg. "vpc "==Virtual PC, "vs  "==Virtual Server.
   UINT u32CreatorVer;    // Version number of app that created this VHD. VPC 2007 sets this to 0x50003, VirtServ sets 0x10000.
   BYTE u32CreatorOS[4];  // Another 4-byte string. "wi2k"==Windows. "mac "==Mac.
   HUGE u64OriginalSize;  // Max size of VHD when it was created (virtual drive size, not current allocation size).
   HUGE u64CurrentSize;   // Max size of VHD now. Matches OriginalSize unless VHD has been expanded.
   VHD_DISK_GEOMETRY geometry;
   UINT u32DiskType;      // See VHD_TYPE_xxx defs.
   UINT u32Checksum;      // Ones complement of sum of all bytes in the footer, excluding the checksum field.
   S_UUID uuidCreate;
   BYTE u8SavedState;
   // 85 bytes to this point.
   BYTE reserved[427];    // pad with 0 bytes up to 512 bytes.
} VHD_FOOTER;

// VHD dynamic header format. Remember that all structures in the VHD use big endian byte order.
// Dynamic header is present on dynamic and differencing VHD types only (but cookie makes it detectable if
// that rule is ever broken).
typedef struct {
   BYTE cookie[8];          // The string "cxsparse"
   HUGE u64DataOffset;      // Offset to image data. Field not used, should be set to (HUGE)(-1).
   HUGE u64TableOffset;     // Absolute byte offset of block table.
   UINT u32HeaderVer;       // Header version, VPC writes 0x00010000
   UINT u32BlockCount;      // Number of entries in block map.
   UINT u32BlockSize;       // The size in bytes of one block. Typical value for VPC is 2MB.
   UINT u32Checksum;        // Ones complement of sum of all bytes in the dyn-header, excluding the checksum field.
   S_UUID uuidParent;     // In snapshot VHDs, this identifies the parent VHD.
   UINT u32ParentTimeStamp; // Modification time stamp of parent. This is used by snapshots to detect modified base VHD.
   UINT u32Reserved;        // Set to zero.
   WORD wcParentName[256];  // Tail name of parent VHD file (Unicode).
   VHD_PARENT_LOCATOR ParLoc[8];
   BYTE reserved[256];      // pad with 0 bytes up to 1024 bytes.
   
   // This may be followed by :-
// UINT BlockMap[u32BlockCount]; // Each map entry is an absolute sector offset. 0xFFFFFFFF==not allocated.
// BYTE padding[];               // pad to a sector boundary.
// BLOCK image[u32BlockCount];   // The disk image.
} VHD_DYN_HEADER;

// Additional notes.
//
// o Although I show the typical arrangement with BlockMap immediately following the Dynamic Header, this need
//   not be the case - the BlockMap can be anywhere.
// o The BlockMap[] table contains sector offsets, hence overlapping blocks are a possibility that may need to
//   be checked for (sort into offset order then check for deltas between entries < ((BlockSize/512)+1). Also
//   possible is that additional data is squeezed between blocks in the image.
// o A block itself consists of 1 sector for a "Sectors Used" bitmap, then the block data (BlockSize bytes).
//   The bitmap is used in differencing VHDs - a 0 bit means that the sector has not been modified in the current
//   VHD, thus we should fetch that sector from the parent. The bitmap is also used in non-differencing VHDs,
//   though it serves little purpose then.
//
#endif


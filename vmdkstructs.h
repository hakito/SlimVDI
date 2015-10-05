/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VMDKSTRUCTS_H
#define VMDKSTRUCTS_H

/*=========================================================================*/
/* Structures related to VMWare VMDK files.                                */
/*=========================================================================*/

#include "djtypes.h"
#include "djfile.h"

#define VMDK_FORMAT_VER          1
#define VMDK_MAGIC_NUM           0x564D444B /* 'VMDK' */
#define VMDK_MAX_EXTENTS         32
#define VMDK_PAGE_FREE           0

#define VMDK_COMPRESSION_NONE    0
#define VMDK_COMPRESSION_DEFLATE 0

// parse flags, used to detect when something important is missing from descriptor.
#define VMDK_PARSED_VERSION      0x0001
#define VMDK_PARSED_ENCODING     0x0002
#define VMDK_PARSED_CID          0x0004
#define VMDK_PARSED_PARENTCID    0x0008
#define VMDK_PARSED_CREATETYPE   0x0010
#define VMDK_PARSED_SECTORSPT    0x0020
#define VMDK_PARSED_HEADS        0x0040
#define VMDK_PARSED_CYLINDERS    0x0080
#define VMDK_PARSED_VHWVER       0x0100
#define VMDK_PARSED_EXTENT       0x0200
#define VMDK_PARSED_UUID         0x0400
#define VMDK_PARSED_UUID_PARENT  0x0800
#define VMDK_PARSED_UUID_MODIFY  0x1000
#define VMDK_PARSED_UUID_PMODIFY 0x2000
#define VMDK_HEADER_MUST_HAVE    (VMDK_PARSED_EXTENT|VMDK_PARSED_CREATETYPE|VMDK_PARSED_VERSION)

// Creation types: I only list the ones I support
#define VMDK_CRTYPE_UNKNOWN      0
#define VMDK_CRTYPE_MONOSPARSE   1 /* monolithic sparse (dynamic, one big file) */
#define VMDK_CRTYPE_MONOFLAT     2 /* monolithic flat (fixed size, one big file) */
#define VMDK_CRTYPE_2GBSPARSE    3 /* split dynamic */
#define VMDK_CRTYPE_2GBFLAT      4 /* split flat */
#define VMDK_CRTYPE_FULLDEVICE   5 /* Direct access to a whole raw disk */
#define VMDK_CRTYPE_PARTDEVICE   6 /* Direct access to a raw disk partition */
#define VMDK_CRTYPE_STREAMOPT    7 /* Compressed data stream (not supported) */

// Extent I/O modes (first field of descriptor line.
#define VMDK_EXT_IOMODE_RW       1
#define VMDK_EXT_IOMODE_RDONLY   2
#define VMDK_EXT_IOMODE_NOACCESS 3

// Extent types: I only list the ones I support
#define VMDK_EXT_TYPE_UNKNOWN    0
#define VMDK_EXT_TYPE_FLAT       1
#define VMDK_EXT_TYPE_SPARSE     2
#define VMDK_EXT_TYPE_ZERO       3

// Disk geometry info. Modern drives use LBA addressing modes, but VBox supports operating
// systems which pre-date that.
typedef struct {
   UINT cCylinders;
   UINT cHeads;
   UINT cSectorsPerTrack;
   UINT cBytesPerSector;
} DISK_GEOMETRY;

// This structure is used to stored information gathered about a single VMDK extent as
// the descriptor file is parsed.
typedef struct {
   UINT flags;       // I set a bit in here for each extent descriptor field successfully parsed.
   UINT sectors;     // size in sectors (as stated in descriptor, may not match actual file).
   UINT iomode;      // see VMDK_EXT_IOMODE_xxxx defines.
   UINT type;        // see VMDK_EXT_TYPE_xxxx defines.
   UINT offset;      // offset (in sectors?) into the extent file where image starts. Usually 0.
   UINT BlkSize;     // (in sectors) put here so "ShowHeader" can access it.
   UINT nBlocks;     // put here so "ShowHeader" can access it.
   UINT nAlloc;      // put here so "ShowHeader" can access it.
   char fn[2048];    // filename of extent file.
} VMDK_EXTENT_DESCRIPTOR, *PVMDKED;

// The following header is synthesized, it doesn't correspond to what's physically
// stored in the file(s).
typedef struct {
   UINT     flags;     // I set a bit in here for each descriptor section successfully parsed.
   UINT     version;
   UINT     CID;
   UINT     CIDParent;
   UINT     CreateType; // some kind of code indicating twoGbMaxExtentSparse etc.
   int      nExtents,nMaxExtents;
   UINT     vhwver;
   S_UUID   uuidCreate,uuidParent,uuidModify,uuidPModify;
   DISK_GEOMETRY geometry;
   VMDK_EXTENT_DESCRIPTOR extdes[VMDK_MAX_EXTENTS]; // actually variable length, but grows in chunks of VMDK_MAX_EXTENTS.
} VMDK_HEADER;
#define VMDK_HEADER_BASE_SIZE (sizeof(VMDK_HEADER)-sizeof(VMDK_EXTENT_DESCRIPTOR)*VMDK_MAX_EXTENTS)

typedef struct { // official header for a VMDK extent (512 bytes).
   UINT magic;
   UINT version;

   UINT flags;
   UINT capacity;

   UINT capacityHi;
   UINT grainSize;

   UINT grainSizeHi;
   UINT descriptorOffset;

   UINT descriptorOffsetHi;
   UINT descriptorSize;

   UINT descriptorSizeHi;
   UINT numGTEsPerGT;

   HUGE rgdOffset;
   HUGE gdOffset;
   HUGE overHead;
   BYTE bUncleanShutdown;
   char singleEndLineChar;
   char nonEndLineChar;
   char doubleEndLineChar1;
   char doubleEndLineChar2;
   BYTE compressAlgorithm;
   BYTE compressAlgorithmHi;
   BYTE pad[432];
} VMDK_SPARSE_HEADER;

typedef struct { // runtime info about a descriptor.
   VMDK_SPARSE_HEADER hdr; // how much of this do I need? ImageOffset. Type(flat,sparse)
   FILE f;
   UINT type;
   UINT ExtentSize;                // in sectors. This is redundant but makes finding the right extent easier.
   UINT nBlocks;
   UINT nBlocksAllocated;
   UINT SectorsPerBlock;
   UINT SPBshift;                  // Only valid for sparse extents.
   int  PageReadResult;
   BYTE *buff;                     // only used by dummy extents built in memory.
   UINT *blockmap;
} VMDK_EXTENT, *PEXTENT;

#endif


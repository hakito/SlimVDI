/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VDISTRUCTS_H
#define VDISTRUCTS_H

/*=========================================================================*/
/* Structures embedded in VirtualBox VDI files.                            */
/*=========================================================================*/

#include "djtypes.h"

#define VDI_SIGNATURE   (0xBEDA107F)

extern PSTR pszVdiInfoCloneVDI /* = "<<< CloneVDI VirtualBox Disk Image >>>\n" */ ;
extern PSTR pszVdiInfo         /* = "<<< Sun VirtualBox Disk Image >>>\n" */ ;
extern PSTR pszVdiInfoAlt      /* = "<<< Sun xVM VirtualBox Disk Image >>>\n" */ ;
extern PSTR pszVdiInfoInno     /* = "<<< innotek VirtualBox Disk Image >>>\n" */ ;

// VDI image types
#define VDI_TYPE_NORMAL  1  /* Normal, dynamically growing VDI */
#define VDI_TYPE_DYNAMIC 1  /* Normal, dynamically growing VDI */
#define VDI_TYPE_FIXED   2  /* Fixed size VDI */
#define VDI_TYPE_UNDO    3  /* Dynamic. Not sure of purpose - "Undo" implies keeping record of previous content of linked VDI */
#define VDI_TYPE_DIFF    4  /* Dynamic. Differencing VDI, tbd, I think it means this VDI has changed blocks relative to another VDI */

// VDI flags
#define VDI_FLAG_ZERO_EXPAND (0x0100) /* Fill newly allocated blocks with zeroes. Only valid in newly created VDIs */

// Special values in block map
#define VDI_PAGE_FREE 0xFFFFFFFF /* indicates a free (unallocated) block. Reads from this block return random data. */
#define VDI_PAGE_ZERO 0xFFFFFFFE /* indicates a zero block. Reads from this block return zeroes. */
#define VDI_BLOCK_ALLOCATED(blkptr) ((blkptr)<VDI_PAGE_ZERO)

#define VDI_VERSION_1_0 0x00000001
#define VDI_VERSION_1_1 0x00010001
#define VDI_LATEST_VERSION (VDI_VERSION_1_1)

// Legacy disk geometry info. Modern drives use LBA addressing modes, but VBox supports operating
// systems which pre-date that.
typedef struct {
   UINT cCylinders;
   UINT cHeads;
   UINT cSectorsPerTrack;
   UINT cBytesPerSector;
} VDIDISKGEOMETRY;

typedef struct {
   char szFileInfo[64];   // Typically "<<< Sun VirtualBox Disk Image >>>\n". Must include terminating NUL.
   UINT u32Signature;     // == VDI_SIGNATURE
   UINT u32Version;       // VDI format version. Typically 0x00010001. Hi word=major version, Lo word=minor version.
} VDI_PREHEADER;

typedef struct {          // header proper.
// UINT cbSize;           // not present in old header.
   UINT vdi_type;         // VDI type (see VDI_TYPE_xxxx).
   UINT vdi_flags;        // VDI flags (see VDI_FLAG_xxxx). Typically 0.
   char vdi_comment[256]; // User supplied comment. Must include a terminating NUL.
// UINT offset_Blocks;    // not present in old header.
// UINT offset_Image;     // not present in old header.
   VDIDISKGEOMETRY LegacyGeometry;
// UINT dwDummy;          // not present in old header.
   HUGE DiskSize;         // Maximum size in bytes of virtual disk.
   UINT BlockSize;        // Size of an image block. Typically 1M (1024^2) bytes.
// UINT cbBlockExtra;     // not present in old header.
   UINT nBlocks;          // Number of blocks == number of entries in blocks array.
   UINT nBlocksAllocated; // Number of blocks allocated so far.
   S_UUID uuidCreate;     // UUID assigned to VDI on creation.
   S_UUID uuidModify;     // UUID signature identifying a particular version of a VDI file (important when VDI is a base for snapshots).
   S_UUID uuidLinkage;    // In differencing VDIs, this is the UUID of the parent VDI.
// S_UUID uuidParentModify;    // not present in old header.
// VDIDISKGEOMETRY LCHSGeometry; // not present in old header.
} VDI_OLD_HEADER;

typedef struct {          // header proper.
   UINT cbSize;           // Size in bytes of header proper.
   UINT vdi_type;         // VDI type (see VDI_TYPE_xxxx).
   UINT vdi_flags;        // VDI flags (see VDI_FLAG_xxxx). Typically 0.
   char vdi_comment[256]; // User supplied comment. Must include a terminating NUL.
   UINT offset_Blocks;    // Offset in bytes from beginning of VDI file to blocks array (should be sector aligned).
   UINT offset_Image;     // Offset in bytes from beginning of VDI file to disk image (should be sector aligned).
   VDIDISKGEOMETRY LegacyGeometry;
   UINT dwDummy;          // No longer used.
   HUGE DiskSize;         // Maximum size in bytes of virtual disk.
   UINT BlockSize;        // Size of an image block. Typically 1M (1024^2) bytes.
   UINT cbBlockExtra;     // Size of additional service information of every data block. Prepended before block data. May be 0. Should be a power of 2 and sector-aligned for optimization reasons.
   UINT nBlocks;          // Number of blocks == number of entries in blocks array.
   UINT nBlocksAllocated; // Number of blocks allocated so far.
   S_UUID uuidCreate;     // UUID assigned to VDI on creation.
   S_UUID uuidModify;     // Modify UUID: when the VDI content changes, this also changes. See uuidParentModify.
   S_UUID uuidLinkage;    // AKA uuidParent. In differencing VDIs, this is the creation UUID of the parent.
   S_UUID uuidParentModify; // In differencing VDIs, this is the modification UUID we expect to see in the parent.

   /* LCHS image geometry (new field in VDI1.2 version. */
   /* Note that version signature didn't change (still 0x10001), you can only tell this is here by checking cbSize. */
   VDIDISKGEOMETRY LCHSGeometry;
} VDI_HEADER;

// Following the header is this structure
// typedef struct {
//   // Blocks array
//   BYTE padding1[offset_Blocks - cbSize - sizeof(preheader)];
//   UINT Blocks[nBlocks];
//
//   // Image data
//   BYTE padding2[offset_Image - Offset(Blocks) - nBlocks*sizeof(UINT)];
//   BYTE ImageData[variable];
// } VDI_IMAGE_DATA;

// Each element of the Blocks array contains the SID of the associated block. A SID can be turned into a file offset
// by multiplying by block size then adding the offset of the image data. Two SID values have special meaning, see
// the VDI_PAGE_xxxx symbols defined above.

#endif


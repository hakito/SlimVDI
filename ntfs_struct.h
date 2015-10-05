/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef NTFS_STRUCT_H
#define NTFS_STRUCT_H

// the on-disk boot sector format uses misaligned fields, so this is not the
// on disk format. Instead I have a function which unpacks the boot sector into
// this format.
typedef struct {
   char SystemID[16];
   WORD BytesPerSector;
   BYTE SectorsPerCluster;
   BYTE MediaDescriptor;
   WORD SectorsPerTrack;
   WORD NumberOfHeads;
   UINT MediaType;
   UINT SectorsPerClusterShift; // not in actual boot sector
   HUGE SectorsInVolume;
   HUGE LCN_MFT;
   HUGE LCN_MFTMirr;
   UINT BytesPerMFTRec;
   UINT BytesPerIndexRec;
   HUGE VolumeSerialNumber;
   HUGE BootSectorLBA; // not in actual boot sector
   HUGE LastSectorLBA; // not in actual boot sector == BootSectorLBA + SectorsInVolume
   HUGE TruePartitionSectors; // not in actual boot sector (SectorsInVolume is rounded down to a multiple of cluster size)
} NTFS_BOOT_SECTOR;

#define NTFS_SIG_FILE 0x454C4946

/* The Boot sector contains the cluster number of the MFT, where all file records are
 * stored. An MFT file record consist of a FILE header followed by a list of attributes,
 * each of which may be resident (stored inside the MFT) or non-resident.
 * MFT[0] is the MFT itself!  Everything in NTFS is stored in files (streams),
 * even things like the MFT itself. The only exception is the boot sector, and even that
 * has an entry in the MFT.
 */
/* flags for MFT FILE records */
#define MFT_FLAG_USED      1
#define MFT_FLAG_DIRECTORY 2
#define MFT_FLAG_UNKNOWN_1 4
#define MFT_FLAG_UNKNOWN_2 8

/* flags for MFT Attributes */
#define ATTR_FLAG_COMPRESSED 0x0001
#define ATTR_FLAG_ENCRYPTED  0x4000
#define ATTR_FLAG_SPARSE     0x8000

typedef struct { // format of file records in the MFT. This is usually followed by a header and a list of attributes.
   UINT signature;       // 0x454C4946 == "FILE".
   WORD UpdateSeqOffset; // offset to update sequence from start of file record (see note below on update sequences)
   WORD UpdateSeqLength; // length of update sequence field in WORDs.
   HUGE LogFileSeqNum;   // $LogFile sequence number (LSN)
   WORD SeqNum;          // Sequence number.
   WORD HardLinkCount;
   WORD FirstAttrOffset;
   WORD Flags;           // see MFT_FLAG_xxx defines.
   UINT FileRecSizeReal;
   UINT FileRecSizeAlloc;
   HUGE BaseFileRecRef;
   WORD idNextAttr;
   WORD wAlign;          // align to UINT boundary (XP)
   UINT MFTRecNo;        // MFT record number (XP)
// potentially more fields here in future versions of NTFS
// WORD UpdateSequence[UpdateSequenceLength];  // in XP this is at offset 0x30. It consists of a Sequence Number word, followed by the sequence fixups.
// ATTRIBUTE LIST GOES HERE - LIST TERMINATED BY ATTRIBUTE ID OF 0xFFFFFFFF or $End depending on who you believe.
} NTFS_FILE_RECORD, *PNTFS_FILE_RECORD;

// Note on update sequences (see UpdateSeqxxx fields and UpdateSequence[] array in above record).
// Every time the host OS modifies an MFT FILE entry, it calculates a new WORD signature called an "update sequence
// number", and writes this sequence number into the last two bytes of every 512 byte block that makes up the
// file record. Obviously this displaces the original data from those two bytes, so copies of the latter are
// preserved in the update sequence array. UpdateSequence[0] contains the last sequence number generated, this
// should match what you see at the end of each sector. UpdateSequence[1..N] contains the saved original
// values of the last two bytes from each sector.
//
// IMPORTANT: This of course means that you can't ignore update sequences, because the OS has effectively
// corrupted the last WORD of each sector in the MFT!


/* known MFT attribute IDs */
#define MFT_ATTR_STANDARD   0x10 /* $STANDARD_INFORMATION */
#define MFT_ATTR_FILENAME   0x30 /* $FILE_NAME */
#define MFT_ATTR_DATA       0x80 /* $DATA */
#define MFT_ATTR_BITMAP     0xB0 /* $BITMAP (bitmap attribute, not to be confused with $Bitmap file) */

/* MFT attribute format */
typedef struct {      // generic attribute structure
   UINT type;         // attribute type (see MFT_ATTR_xxxx values).
   UINT len;          // attribute length in bytes, including this header.
   BYTE bNonResident; // 0==resident, 1==non-resident. Other values == ?
   BYTE NameLength;   // Length in characters of attribute name. This may be 0 - attributes don't always have names.
   WORD NameOffset;   // Offset to the name (in bytes, relative to attribute start).
   WORD flags;        // see ATTR_FLAG_xxx
   WORD id;           // attribute id.
   // Resident vs Non-resident formats differ from here on.
   union {
      struct { // resident attributes
         UINT AttrLen;      // attribute length in bytes.
         WORD AttrOffset;
         BYTE bIndexed;
         BYTE bPadding;
      // WORD AttrName[NameLength];
      // BYTE Attrib[AttrLen];
      } res;
      struct { // non-resident attributes (see description of runlists below)
         HUGE StartVCN;
         HUGE LastVCN;
         WORD DataRunOffset; // Offset from attribute record start to the Runs[] array. Apparantly this is always aligned on quad boundary.
         WORD CompressionUnitSize;
         UINT padding;
         HUGE AttrSizeAlloc; // This is the attribute size rounded up to a cluster boundary.
         HUGE AttrSizeReal;
         HUGE InitDataSize;
      // WORD AttrName[NameLength];  // actually variable length.
      // WORD Runs[];  // This must be on a UINT boundary.
      } nonres;
   } u;
} MFT_ATTRIBUTE, *PMFT_ATTRIBUTE;

// Non Resident Attributes and Run lists
// -------------------------------------
// When an MFT attribute is non-resident the body of the MFT attribute record contains a packed list
// of runs which you decode to find out which external clusters contain the attribute data. Files above
// a few hundred bytes will be implemented as non-resident DATA attribute in an MFT FILE record. However,
// a file can have multiple data attributes (forked streams), and other attributes could also in theory
// be non-resident.
//
// See the "nonres" structure in the MFT attribute record. The StartVCN and LastVCN fields define
// what part of the data is mapped by the run length codes (VCN = Virtual Cluster Number, i.e. stream
// relative cluster number, starting at 0). You also need to check CompressionUnitSize (non-zero means
// uncompressed, >1 is a compression unit size in clusters, "compression unit" meaning that compression
// is individually applied to blocks of that size).
//
// A single run looks like this :-
// typedef struct {
//    BYTE OsLs;       // ls nibble == Ls == size of Length field, ms nibble == Os == size of Offset field.
//    BYTE Length[Ls]; // Length of run in clusters, I expect this is unsigned.
//    BYTE Offset[Os]; // Start LCN of this run, encoded as signed delta from start LCN of previous run.
// } NRES_ATTR_RUN;    // (for the first run we take "start LCN of previous run" to be 0).
//
// Notice that there is no "Number of Runs" field in the attribute record. The list is terminated by an OsLs
// byte of zero, or by the end of the attribute record. Remember that the LCN Offset in the run is signed,
// i.e. it can be negative, indicating that the next LCN comes physically before the current one on the
// volume. I believe the maximum size of an offset is eight bytes, ie. an int64. Ditto for the length (note
// however that the boot sector gives the volume size in clusters as a uint32).
//
// Special cases:
//   - An Os (offset field size) of 0 indicates a run of zeroed clusters, this is NTs "sparse files"
//     feature. Length in this case should be 16, but I see no harm in catering for other values. You would expect
//     the ATTR_FLAG_SPARSE flag to be set in the attribute in this case.
//   - NTFS files can also be compressed (ATTR_FLAG_COMPRESSED flag set). In that case the file is compressed in
//     units of CompressionUnitSize clusters (usually 16). A run length(Length) < CompressionUnitSize indicates that
//     compression was effective. This will be followed by a run with an offset of 0 and a length (CompressionUnitSize-L).
//   - Check the StartVCN field in the attribute. The first run isn't always StartVCN=0. If StartVCN>0 it implies
//     that the stream starts with VCN clusters filled with zeroes.
//
// TODO: If the runlist is too large to fit in the MFT (only happens with very fragmented big files) then I
// believe the runlist can be stored outside the MFT, eventually forming an inode-like hierarchy. I don't know
// the details of how this works yet, and shouldn't need to, just ro read the $Bitmap file (the latter is created
// at full size when the volume is formatted and empty, hence $Bitmap shouldn't be fragmented at all, nor
// especially large).
//

typedef struct { // format of MFT $FILE_NAME attribute
   HUGE ParentFolder;
   HUGE CreateTime;
   HUGE ModifyTime;
   HUGE MFTChangeTime;
   HUGE LastAccessTime;
   HUGE FileSizeAlloc;
   HUGE FileSizeReal;
   UINT Flags;
   UINT ReparseStuff;
   BYTE FileNameLen; // Filename length in characters (not bytes).
   BYTE FileNameNameSpace;
   WORD Name[8]; // Unicode name, variable length.
} MFT_FILENAME;

#endif


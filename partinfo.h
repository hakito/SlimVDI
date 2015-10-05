/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef PARTINFO_H
#define PARTINFO_H

#include "djtypes.h"

// Structure of a legacy MBR partition table entry.
typedef struct { // 16 bytes, usually four entries in the MBR with first entry at offset 446.
   BYTE Status;           // 0x80==bootable, 0x00==not bootable, other values invalid.
   BYTE pstart_chs_head;  // CHS address of first sector of volume: head
   BYTE pstart_chs_sect;  // CHS address of first sector of volume: sector number in bits 0..5, bits 6..7 have two msbs of cylinder number.
   BYTE pstart_chs_cyl;   // CHS address of first sector of volume: 8 lbs of cylinder number.
   BYTE PartType;         // partition type code.
   BYTE pend_chs_head;    // CHS address of last sector of volume: head
   BYTE pend_chs_sect;    // CHS address of last sector of volume: sector number in bits 0..5, bits 6..7 have two msbs of cylinder number.
   BYTE pend_chs_cyl;     // CHS address of last sector of volume: 8 lbs of cylinder number.
   WORD loStartLBA;       // Low word of LBA address of first sector of volume.
   WORD hiStartLBA;       // High word of LBA address of first sector of volume.
   WORD loNumSectors;     // Low word of number of sectors in the partition.
   WORD hiNumSectors;     // High word of number of sectors in the partition.
} PartTableEntry, *PPART;

// Structure of PRIVHEAD record in a Windows Dynamic Disk (stored in sector 6 of track 0).
// All dynamic disk partitions have a partition type 0x42.
// !NOTE! THIS STRUCTURE APPEARS TO USE BIG-ENDIAN BYTE ORDER (NOT INTEL)
#if 0

typedef struct {
   BYTE magic[8];          // "PRIVHEAD"
   UINT reserved1;         // dont know
   UINT FormatVersion;     // Major version in hi word, minor in lo word - but remember "hi" refers to big endian byte order.
   HUGE timestamp;         // creation timestamp?
   HUGE reserved2;         // =9. ?
   HUGE LDMReservedSectors; // =2047. Size in sectors of LDM database reserved area? Third PRIVHEAD copy is at LDMstart+LDMReservedSectors-1.
   HUGE LDMUsedSectors;    // =1856. Length in sectors of used part of LDM reserved area? Second PRIVHEAD copy is at LDMstart+LDMUsedSectors-1.
   BYTE guidDisk[64];      // text uuid (32bytes), NUL padded to 64 bytes.
   BYTE guidHost[64];      // ditto.
   BYTE guidDiskGroup[64]; // ditto.
   BYTE DiskGroupName[32]; // text, NUL padded.
   UINT Reserved3;         // =0x20000.
   BYTE pad[7];            // why suddenly misalign the data?
   HUGE BootDiskStart;     // =63. Note that to be compatible with a BIOS boot, a Windows boot partition is always "simple".
   HUGE BootDiskSize;      // =67103442 in the 32GB disk I tested with (MBR showed two partions of 67087377+16065 sectors respectively). Shell reported drive C having capacity of 67087376 sectors, so MBR seems right.
   HUGE ConfigStart;       // =67106816 in the 32GB test disk I created (visible LVM data seemed to start at 67106817).
   HUGE ConfigSectors;     // =2048
   HUGE NumTOCs;           // =1
   HUGE TOCsize;           // =2046
   UINT NumConfigs;        // =1
   UINT NumLogs;           // =1
   HUGE ConfigSize;        // =1452
   HUGE LogSize;           // =220
   UINT DiskSignature;     // Matches disk signature stored at MBR offset 0x440 (with reversed byte order of course).
   S_UUID guidDiskSet1;
   S_UUID guidDiskSet2;
} PRIVHEAD, *PPRIVHEAD;

#endif

#ifdef WINVER
void PartInfo_Show(HINSTANCE hInstRes, HWND hWndParent, BYTE *mbr);
#endif

#endif


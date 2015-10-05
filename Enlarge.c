/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

// This module is enabled by the "Increase partition size" option. It makes use of
// the COW features to manipulate a drive, i.e. enlarge the drive, move partitions
// around. I then call on the filesytem modules to increase the partition size if
// the main partition uses a supported filesystem.

#include "djwarning.h"
#include "djtypes.h"
#include "enlarge.h"
#include "cow.h"
#include "partinfo.h"
#include "fsys.h"

// Searching for the boot partition doesn't always identify the real main partition,
// eg. when a boot manager is being used. Searching for the largest existing
// partition may work better for the majority of cases.
#define PICK_LARGEST_PART 1

static BYTE MBR[512];

/*.....................................................*/

#if !PICK_LARGEST_PART

static UINT
FindBootPartition(BYTE *MBR, UINT *iEndDisk)
// Verify that this really is an MBR, and find the byte offset of the boot partition.
{
   if (MBR[510]==0x55 && MBR[511]==0xAA) {
      PPART pPart = (PPART)(MBR+446);
      UINT i,LBA,LBAsize,LBAtest=1,iMainPart=0;
      for (i=0; i<4; i++) { // check that boot codes and LBA values seem ok.
         if (pPart->Status==0x80) {
            if (iMainPart) break; // more than one bootable partition!
            else iMainPart = (446+(i<<4));
         } else if (pPart->Status != 0) {
            break;
         }
         LBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (LBA==0) continue;
         LBAsize = MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
         if (LBA<LBAtest) break; // if this is really an MBR then the partitions overlap!
         LBAtest = LBA + LBAsize; // where next partition should start
         pPart++;
      }
      *iEndDisk = LBAtest;
      if (i==4) return iMainPart;
   }
   return 0;
}

#endif

/*.....................................................*/

#if PICK_LARGEST_PART

static UINT
FindLargestPartition(BYTE *MBR, UINT *iEndDisk)
// Verify that this really is an MBR, and find the byte offset of the largest partition.
{
   if (MBR[510]==0x55 && MBR[511]==0xAA) {
      PPART pPart = (PPART)(MBR+446);
      UINT i,LBA,LBAsize,LBAtest=1,Largest,iLargest=0;
      Largest = 0;
      for (i=0; i<4; i++) { // check that boot codes and LBA values seem ok.
         if (pPart->Status!=0x80 && pPart->Status!=0) break; // illegal partition status.
         LBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (LBA==0) continue; // blank partition entry
         if (LBA<LBAtest) break; // if this is really an MBR then the partitions overlap!
         LBAsize = MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
         if (LBAsize>Largest) {
            iLargest = (446+(i<<4));
            Largest = LBAsize;
         }
         LBAtest = LBA + LBAsize; // where next partition should start
         pPart++;
      }
      *iEndDisk = LBAtest;
      if (i==4) return iLargest;
   }
   return 0;
}

#endif

/*.....................................................*/

static UINT
CalcHeads(UINT nSectors)
// Calculate the "Heads" parameter for CHS addressing in a drive whose size is "nSectors" sectors.
{
   UINT cHeads;
   if (nSectors >= (1024*255*63)) cHeads = 255;
   else {
      UINT C = nSectors / (16*63);
      cHeads = 16;
      while (C>1024) {
         cHeads += cHeads;
         C >>= 1;
      }
      if (cHeads>255) cHeads = 255;
   }
   return cHeads;
}

/*.....................................................*/

static void
LBA2CHS(BYTE *pC, BYTE *pH, BYTE *pS, UINT LBA, UINT cHeads)
{
   UINT C,H,S,temp;
   if (LBA>=(1023*254*63)) {
      C = 1023; H=254; S=63;
   } else {
      C = LBA / (cHeads*63);
      temp = LBA - (C*cHeads*63);
      H = temp / 63;
      S = (temp - H*63)+1;
   }
   *pC = (BYTE)(C & 0xFF);
   *pH = (BYTE)H;
   *pS = (BYTE)(((C>>2)&0xC0) + S);
}

/*....................................................*/

static void
SlideOtherPartitions(UINT iMainPart, UINT cSlideSectors, UINT cHeads)
// Partitions after the main one are moved out of the way using the COW
// API. All this function does it correct the partition table entries
// in the MBR.
{
   UINT i;
   for (i=iMainPart+16; i<510; i+=16) {
      PPART pPart = (PPART)(MBR+i);
      UINT LBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
      UINT PartSize = MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
      if (PartSize==0) break;
      LBA += cSlideSectors;
      pPart->loStartLBA = (WORD)LBA;
      pPart->hiStartLBA = (WORD)(LBA>>16);
      LBA2CHS(&pPart->pstart_chs_cyl,&pPart->pstart_chs_head,&pPart->pstart_chs_sect,LBA,cHeads);
      LBA2CHS(&pPart->pend_chs_cyl,&pPart->pend_chs_head,&pPart->pend_chs_sect,LBA+PartSize-1,cHeads);
   }
}

/*.....................................................*/

static UINT
PartitionMaxSize(UINT iPart, UINT DiskSectors)
// iPart is the offset into the boot sector of a partition. This
// function returns the size (in sectors) which that partition
// can grow to.
{
   PPART pPart = (PPART)(MBR+iPart);
   UINT LBAstart = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
   UINT LBAend = DiskSectors; // grows space defaults to end of disk
   UINT NewSize,OldSize = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
   if (pPart->PartType==7) LBAend -= 2048; // on Windows disks, reserve space for "Dynamic disk" data at end of drive.
   if (iPart<(510-16)) { // if there's a following partition then reduce growth space further.
      pPart = (PPART)(MBR+iPart+16);
      if (pPart->loNumSectors || pPart->hiNumSectors) LBAend = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
   }
   NewSize = (((LBAend - LBAstart)/63)*63); // partition size rounded to cylinder boundary.
   if (NewSize<OldSize) NewSize = OldSize; // on no account allow a partition to shrink!
   return NewSize;
}

/*.....................................................*/

PUBLIC HVDDR
Enlarge_Drive(HVDDR hVDI, UINT NewDiskSize)
// Enlarge drive then also enlarge the main partition if possible.
{
   HCOW  cow;
   HUGE  DriveSize;
   UINT  LBA_part_start,part_sectors,ExtraSectors,Residue,iEndDisk;
   UINT  iMainPart,cHeads;
   PPART pPart;
   
   // Find main partition info.
   hVDI->ReadSectors(hVDI,MBR,0,1);
#if PICK_LARGEST_PART
   iMainPart = FindLargestPartition(MBR,&iEndDisk); // returns byte index into MBR, or 0 if no partitions were bootable.
#else
   iMainPart = FindBootPartition(MBR,&iEndDisk); // returns byte index into MBR, or 0 if no partitions were bootable.
#endif
   if (!iMainPart) return hVDI;

   // Make sure that drive size is increasing, not shrinking.
   hVDI->GetDriveSize(hVDI, &DriveSize);
   DriveSize >>= 9;    // convert bytes to sectors
   NewDiskSize <<= 11; // Convert MB to sectors.
   if (DriveSize>NewDiskSize) return hVDI;
   cHeads = CalcHeads(NewDiskSize);

   // Calculate how much space to insert after the main partition, and
   // how much padding to put on the end of the drive. We make sure that
   // partitions begin and end on cylinder boundaries (63 sectors).
   pPart = (PPART)(MBR+iMainPart);
   LBA_part_start = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
   part_sectors   = MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
   ExtraSectors   = (UINT)(NewDiskSize - DriveSize);
   Residue        = ExtraSectors % 63;
   ExtraSectors   -= Residue;
// if (ExtraSectors==0) return hVDI; // main partition must grow by at least one track.

   // Create COW overlay
   cow = (HCOW)COW_CreateCowOverlay(hVDI);
   if (!cow) return hVDI;

   // Enlarge the drive by inserting new sectors into the space following the main partition.
   if (ExtraSectors|Residue) {
      if ((LBA_part_start+part_sectors)==iEndDisk) { // if main partition is the last one on the drive then we only need one insert.
         cow->InsertSectors(cow,LBA_part_start+part_sectors,ExtraSectors+Residue);
      } else {
         // if a partition follows the main one then insert space after the main partition first, aligning
         // the next partition on a cyclinder boundary.
         if (ExtraSectors) cow->InsertSectors(cow,LBA_part_start+part_sectors,ExtraSectors);

         // Then insert remaining space (residue after cylinder alignment) at the end of the last
         // partition. Note that Windows may have unpartitioned data in the last 1MB of the drive, this
         // ensures that it's still in the last 1MB.
         if (Residue) cow->InsertSectors(cow,iEndDisk+ExtraSectors,Residue);
      }

      // Patch MBR to adjust position of any partitions beyond the main partition.
      if (ExtraSectors) SlideOtherPartitions(iMainPart,ExtraSectors,cHeads);
   }

   // Now grow the main partition to fill the available space.
   // (part_sectors will be unchanged if the main filesystem is not supported).
   //
   // OVERSIGHT - ARGUABLY A BUG: I don't make use of space which existed prior to enlarging the drive. E.g. if I choose
   // the enlarge disk option but don't actually enlarge it, the main partition is also not enlarged, even if it only
   // occupies half of the drive. I should fix that.
   //
   part_sectors = FSys_GrowPartition(pPart->PartType,(HVDDR)cow,LBA_part_start,part_sectors,PartitionMaxSize(iMainPart,NewDiskSize),cHeads);

   // Record adjusted main partition size in MBR then write the MBR back to the (COW) drive.
   pPart->loNumSectors = (WORD)(part_sectors);
   pPart->hiNumSectors = (WORD)(part_sectors>>16);
   LBA2CHS(&pPart->pend_chs_cyl,&pPart->pend_chs_head,&pPart->pend_chs_sect,LBA_part_start+part_sectors-1,cHeads);
   cow->WriteSectors(cow,MBR,0,1);

   // TODO: I should really check for additional NTFS and FAT partitions, check if they have a boot sector,
   // and patch the cHeads field there too if necessary.

   return (HVDDR)cow;
}

/*.....................................................*/

/* end of enlarge.c */

/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* CloneVDI up to and including v1.43 detects unused space only inside a supported
 * filesystem. Hence I have FSYS instances to handle FATx, NTFS and EXTx. I previously
 * overlooked the possibility that unpartitioned regions of the disk may also contain
 * data that which could be discarded (e.g. after a partition has been shrunk or
 * deleted). So, in v1.44 I added this module to map the unpartitioned regions of the
 * drive and hence make them eligible to be discarded from the clone when the compact
 * option is enabled.
 *
 * There IS a danger in doing this: unpartitioned spaces are unmanaged, so anyone
 * is free to hide anything there. One known offender is Windows "Dynamic Disk"
 * feature, which uses the top 1MB of unpartitioned space to hold the volume
 * database (why on earth didn't they just create a 1MB partition up there!?).
 * Anyway, this module specifically caters for that Dynamic Disk feature.
 *
 * Another "data in unpartitioned space" offender is the MBR!  In the past this
 * usually wasn't a problem because the first partition normally started inside
 * the first 1MB block (in fact 63 sectors in), hence compaction would never
 * discard that first block. However Vista and later now put the first partition
 * on a 1MB boundary, plus I've seen examples where the first partition has been
 * deleted, leaving the new first partition starting way up the disk.
 */
 
#include "djwarning.h"
#include "djtypes.h"
#include "unpart.h"
#include "vddr.h"
#include "mem.h"
#include "partinfo.h"

#define MAX_PARTITIONS 8
#define _1_MEG         2048

typedef struct {
   CLASS(FSYS) Base;
   HUGE cDriveSectors;
   UINT nPartitions;
   HUGE FreeSpaceLBA; // LBA of first sector beyond end of last partition.
   HUGE PartStart[MAX_PARTITIONS];
   HUGE PartSectors[MAX_PARTITIONS];
} UNPARTINF, *PUNPART;

/*.....................................................*/

static BOOL
LargeUnallocSpace(PUNPART pUnpart, BYTE *MBR, HUGE cDriveSectors)
{
   BOOL bBigFreeSpace = FALSE;
   if (MBR[510]==0x55 && MBR[511]==0xAA) {
      UINT i,j,cLBA;
      PPART pPart;
      HUGE startLBA,prevEnd;
      BOOL IsLDM = FALSE;

      // Create a dummy partition to reserve the MBR sector. As a side effect this in fact protects the
      // first 1MB of the disk from elimination during compaction, because 1MB is the granularity of
      // the output VDI.
      pUnpart->PartStart[0] = 0;
      pUnpart->PartSectors[0] = 1;
      j=1; prevEnd=1;

      // copy partition info to a more convenient form.
      pPart = (PPART)(MBR+446);
      for (i=0; i<4; i++,pPart++) {
         cLBA = (HUGE)MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
         if (cLBA==0) continue; // skip blank MBR partition map entries.
         if (pPart->PartType==0x42) IsLDM = TRUE; // Windows Dynamic disk format stores data outside MBR partitioned regions.
         startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         pUnpart->PartStart[j] = startLBA;
         pUnpart->PartSectors[j++] = cLBA;
         if (((startLBA - prevEnd)>=_1_MEG)) bBigFreeSpace = TRUE; // look for big space between partitions.
         prevEnd = startLBA+cLBA;
      }
      if (IsLDM) {
         // create a dummy partition to represent LDM (Windows Dynamic Disk) database region.
         startLBA = cDriveSectors-_1_MEG;
         pUnpart->PartStart[j] = startLBA;
         pUnpart->PartSectors[j++] = _1_MEG;
         if ((startLBA - prevEnd)>=_1_MEG) bBigFreeSpace = TRUE; // look for big space between partitions.
         prevEnd = startLBA+_1_MEG;
      }
      if (prevEnd>cDriveSectors) prevEnd = cDriveSectors;
      pUnpart->nPartitions = j;
      pUnpart->FreeSpaceLBA = prevEnd; // note that prevEnd will be 0 in the degenerate case when no partitions are defined.
      if ((cDriveSectors - prevEnd)>=_1_MEG) bBigFreeSpace = TRUE;
   }
   return bBigFreeSpace;
}

/*.....................................................*/

PUBLIC HFSYS
Unpart_OpenVolume(HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize)
{
   PUNPART pUnpart = Mem_Alloc(MEMF_ZEROINIT, sizeof(UNPARTINF));
   if (pUnpart) {
      BYTE MBR[512];
      pUnpart->Base.CloseVolume = Unpart_CloseVolume;
      pUnpart->Base.IsBlockUsed = Unpart_IsBlockUsed;
      pUnpart->cDriveSectors    = cLBA;
      if (hVDI->ReadSectors(hVDI, MBR, 0, 1)==VDDR_RSLT_NORMAL) {
         if (LargeUnallocSpace(pUnpart,MBR,cLBA)) {
            return (HFSYS)pUnpart;
         }
      }
      Mem_Free(pUnpart);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC HFSYS
Unpart_CloseVolume(HFSYS hUnpart)
{
   if (hUnpart) {
      PUNPART pUnpart = (PUNPART)hUnpart;
      Mem_Free(pUnpart);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC int
Unpart_IsBlockUsed(HFSYS hUnpart, UINT iBlock, UINT SectorsPerBlockShift)
{
   if (hUnpart) {
      PUNPART pUnpart = (PUNPART)hUnpart;
      HUGE LBA = iBlock;
      LBA <<= SectorsPerBlockShift;
      if (LBA>=pUnpart->FreeSpaceLBA) { // check if free space falls beyond last partition - also works if no partitions are defined
         return FSYS_BLOCK_UNUSED;
      } else if (pUnpart->nPartitions>1) { // or, if more than one partition is defined then check if block falls inside a gap between partitions.
         UINT i;                           // note that we always treat data before the first partition as in use (this is usually MBR and track0).
         HUGE FreeSpaceLBA = pUnpart->PartStart[0]+pUnpart->PartSectors[0]; // start of first gap.
         HUGE BlockEndLBA = LBA+(HUGE)(1<<SectorsPerBlockShift);
         for (i=1; i<pUnpart->nPartitions; i++) {
            if (LBA<FreeSpaceLBA) break; // block starts inside partition before gap.
            if (BlockEndLBA<=pUnpart->PartStart[i]) return FSYS_BLOCK_UNUSED;
            FreeSpaceLBA = pUnpart->PartStart[i]+pUnpart->PartSectors[i];
         }
      }
   }
   return FSYS_BLOCK_OUTSIDE;
}

/*.....................................................*/

/* end of unpart.c */


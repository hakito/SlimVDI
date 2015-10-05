/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "fat.h"
#include "vddr.h"
#include "mem.h"
#include "djfile.h"
#include "cow.h"

#define CLUSTERS_PER_BUFF 16

#define FAT_TYPE_FAT12 0
#define FAT_TYPE_FAT16 1
#define FAT_TYPE_FAT32 2

#define MAX_CLUSTERS_FAT12 0xFF4
#define MAX_CLUSTERS_FAT16 0xFFF4
#define MAX_CLUSTERS_FAT32 0x0FFFFFF4

typedef struct {   // not a literal image of the boot sector, instead a translation into more convenient form.
   HUGE FATBeginLBA;
   HUGE ClusterBeginLBA; // The area from ClusterBeginLBA to (ClusterBeginLBA+(nClusters*SectorsPerCluster)-1)
   UINT nClusters;       // ... is what we map with the bitmap.
   UINT nSectors;
   UINT SectorsPerFAT;
   UINT RootDirFirstCluster; // FAT32
   UINT SectorsPerCluster;
   UINT ReservedSectors;
   UINT RootDirEntries; // will be zero with FAT32 partitions.
   UINT RootDirSectors; // will be zero with FAT32 partitions.
   UINT NumFATs;
   UINT FSInfoSector;
   UINT FATtype;
// UINT iNextCluster;
// UINT *pFAT;
} FAT_BOOT_SECTOR;

typedef struct {
   CLASS(FSYS) Base;
   HVDDR hVDIsrc;
   FAT_BOOT_SECTOR boots;
   UINT SectorsPerClusterShift;
   HUGE LastSectorLBA;
   UINT ClusterSize;  // in bytes
   PVOID pFATmem; // copy of FAT table.
   UINT *Bitmap;
} FATVOLINF, *PFATVOL;

// Layout of a FAT volume (not to scale):
//
//   +------+------+------+
//   | Reserved sectors   | boot, FSInfo(FAT32 only), backup of boot and FSInfo. Occupies boots.ReservedSectors x sectors.
//   +------+------+------+------+------+------+
//   | Primary FAT                             | Occupies boots.SectorsPerFAT x sectors.
//   +------+------+------+------+------+------+
//   | Backup FAT (Usually present)            | Occupies boots.SectorsPerFAT x sectors (in theory there can be more backup FATS).
//   +------+------+------+--------------------+
//   | Root dir sectors   | Only exists in FAT16. In FAT32 the root dir is stored like any other, ie. as a file.
//   +------+------+------+--------------------+------+------+------+
//   | Cluster addressable region (first cluster has address 2)     |
//   | Each cluster occupies SectorsPerCluster sectors.             |
//   |                                                              |
//   |                                                              |
//   +------+------+------+------+------+------+------+------+------+
//   | Inaccessible region (usually less than one cluster)   |
//   +------+------+------+------+------+------+------+------+

static BYTE raw_boot_sector[512];

/*.....................................................*/

#define IS_POWER_OF_2(x) (((x) & ((x)-1))==0)
// Note that this macro is unsafe (references argument twice). However, the
// macro is used very simply and makes otherwise obscure code a lot clearer.

/*.....................................................*/

static UINT
PowerOfTwo(UINT x)
{
   int y = -1;
   do {
      x >>= 1;
      y++;
   } while (x);
   return (UINT)y;
}

/*.....................................................*/

static BOOL
ReadBootSector(HVDDR hVDIsrc, FAT_BOOT_SECTOR *boots, HUGE iLBA)
{
   BOOL success = FALSE;
   BYTE *sector = raw_boot_sector;

   if (hVDIsrc->ReadSectors(hVDIsrc, sector, iLBA, 1)) {
      if (sector[510]==0x55 && sector[511]==0xAA) {
         UINT n,spc,FATSlots;
         
         if (Mem_Compare(sector+3,"MSDOS",5)!=0 && Mem_Compare(sector+3,"MSWIN",5)!=0 && Mem_Compare(sector+3,"FRDOS",5)!=0) return FALSE;
         
         boots->SectorsPerCluster = (WORD)(sector[0xD]);

         boots->ReservedSectors   = MAKEWORD(sector[0xE],sector[0xF]);

         boots->NumFATs           = sector[0x10];
         boots->RootDirEntries    = MAKEWORD(sector[0x11],sector[0x12]);
         n                        = MAKEWORD(sector[0x13],sector[0x14]);
         if (!n)                n = *((UINT*)(sector+0x20));
         boots->nSectors          = n;
         
         n = MAKEWORD(sector[0xB],sector[0xC]); // bytes per sector.

         boots->SectorsPerFAT = MAKEWORD(sector[0x16],sector[0x17]);
         if (boots->SectorsPerFAT) {
            boots->RootDirFirstCluster = 2;
            boots->FSInfoSector = 0;
         } else {
            boots->SectorsPerFAT = *((UINT*)(sector+0x24)); // FAT32 partitions have this.
            boots->RootDirFirstCluster = *((UINT*)(sector+0x2C));
            boots->FSInfoSector = *((WORD*)(sector+0x30));
         }

         /* do a number of sanity checks on the volume boot sector */
         /* I've already checked that signature(0x55AA) is present. */
         spc = boots->SectorsPerCluster;
         if (n==512 && boots->NumFATs>0 && boots->NumFATs<3 && spc>0 && spc<=64 && IS_POWER_OF_2(spc)) {
            UINT RootDirSectors = ((boots->RootDirEntries*32 + 511) >> 9);
            UINT FirstSector = boots->ReservedSectors + (boots->NumFATs*boots->SectorsPerFAT) + RootDirSectors;
            UINT DataSectors = boots->nSectors - FirstSector;
            UINT nClusters  = DataSectors/spc;

            boots->nClusters = nClusters;
            boots->RootDirSectors = RootDirSectors;
            
            // Determine FAT type, based on cluster count.
            if (nClusters<=MAX_CLUSTERS_FAT12) {
               boots->FATtype = FAT_TYPE_FAT12;
               FATSlots = (boots->SectorsPerFAT<<12)/12; // each sector holds 341.333 FAT slots.
            } else if (nClusters<=MAX_CLUSTERS_FAT16) {
               boots->FATtype = FAT_TYPE_FAT16;
               FATSlots = (boots->SectorsPerFAT<<8); // each sector holds 256 FAT slots.
            } else {
               boots->FATtype = FAT_TYPE_FAT32;
               FATSlots = (boots->SectorsPerFAT<<7); // each sector holds 128 FAT slots.
            }
            
            // check that the sectors per FAT field is consistent with the number of clusters.
            // (the OS reserves the first 2 FAT slots).
            if (FATSlots < (nClusters+2)) return FALSE;

            if (boots->FATtype==FAT_TYPE_FAT32) {
               if (Mem_Compare(sector+82,"FAT32   ",8)!=0) return FALSE;
            } else {
               PSTR psz = sector+54;
               if (boots->FATtype==FAT_TYPE_FAT16) {
                  if (Mem_Compare(psz,"FAT16   ",8)!=0 && Mem_Compare(psz,"FAT     ",8)!=0) return FALSE;
               } else {
                  if (Mem_Compare(psz,"FAT12   ",8)!=0 && Mem_Compare(psz,"FAT     ",8)!=0) return FALSE;
               }
            }

            hugeop_adduint(boots->FATBeginLBA,iLBA,boots->ReservedSectors);
            hugeop_adduint(boots->ClusterBeginLBA,iLBA,FirstSector);
            success = TRUE;
         }
      }
   }
   if (success && boots->FATtype==FAT_TYPE_FAT12) success = FALSE; // I don't support FAT12 for now.
   return success;
}

/*...............................................*/

PUBLIC BOOL
FAT_IsFATVolume(HVDDR hVDI, HUGE iLBA)
{
   FAT_BOOT_SECTOR fbs;
   return ReadBootSector(hVDI,&fbs,iLBA);
}

/*.....................................................*/

static BOOL
CreateUsedClusterBitmap(PFATVOL pFAT)
{
   BOOL success = FALSE;
   PVOID pFATmem = Mem_Alloc(0,pFAT->boots.SectorsPerFAT<<9);
   if (pFATmem) {
      UINT nBitmapDwords = ((pFAT->boots.nClusters+31)>>5)+1;  // extra dword to allow dword lookahead
      UINT *pBitmap = Mem_Alloc(MEMF_ZEROINIT,nBitmapDwords<<2);
      if (pBitmap) {
         if (pFAT->hVDIsrc->ReadSectors(pFAT->hVDIsrc, pFATmem, pFAT->boots.FATBeginLBA, pFAT->boots.SectorsPerFAT)!=VDDR_RSLT_FAIL) {
            if (pFAT->boots.FATtype==FAT_TYPE_FAT32) {
               UINT i,*FAT = ((UINT*)pFATmem)+2;
               for (i=0; i<pFAT->boots.nClusters; i++) {
                  if (FAT[i]&0x0FFFFFFF) pBitmap[i>>5] |= (1<<(i & 0x1F));
               }
            } else { /* FAT16 */
               UINT i;
               WORD *FAT = ((WORD*)pFATmem)+2;
               for (i=0; i<pFAT->boots.nClusters; i++) {
                  if (FAT[i]) pBitmap[i>>5] |= (1<<(i & 0x1F));
               }
            }
            success = TRUE;
         }
         if (success) pFAT->Bitmap = pBitmap;
         else Mem_Free(pBitmap);
      }
      if (success) pFAT->pFATmem = pFATmem;
      else Mem_Free(pFATmem);
   }
   return success;
}
                     
/*.....................................................*/

PUBLIC HFSYS
FAT_OpenVolume(HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize)
{
   PFATVOL pFAT = Mem_Alloc(MEMF_ZEROINIT, sizeof(FATVOLINF));
   if (pFAT) {
      if (ReadBootSector(hVDI,&pFAT->boots,iLBA)) {
         HUGE htemp;
         
         pFAT->Base.CloseVolume = FAT_CloseVolume;
         pFAT->Base.IsBlockUsed = FAT_IsBlockUsed;
         pFAT->hVDIsrc = hVDI;

         pFAT->SectorsPerClusterShift = PowerOfTwo(pFAT->boots.SectorsPerCluster);
         pFAT->ClusterSize = (512<<pFAT->SectorsPerClusterShift);
         
         // calculate LBA of first sector beyond bitmapped area.
         hugeop_fromuint(htemp,pFAT->boots.nClusters);
         hugeop_shl(htemp,htemp,pFAT->SectorsPerClusterShift);
         hugeop_adduint(pFAT->LastSectorLBA,htemp,pFAT->boots.ClusterBeginLBA);
         
         if (CreateUsedClusterBitmap(pFAT)) {
            return (HFSYS)pFAT;
         }
      }
      Mem_Free(pFAT);
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC HFSYS
FAT_CloseVolume(HFSYS hFAT)
{
   if (hFAT) {
      PFATVOL pFAT = (PFATVOL)hFAT;
      Mem_Free(pFAT->pFATmem);
      Mem_Free(pFAT->Bitmap);
      Mem_Free(pFAT);
   }
   return NULL;
}

/*.....................................................*/

static BOOL
SomeClustersUsed(PFATVOL pFAT, UINT i, UINT nClusters)
// Return TRUE if any cluster LCN in the range from i to i+nClusters-1
// is in use.
{
   UINT *Bitmap = pFAT->Bitmap + (i>>5);
   UINT nBits = (i & 0x1F); // count of unwanted lsbs
   UINT mask = (*Bitmap++) & (~((1<<nBits)-1)); // zero bit range from 0..[startbit-1] for first UINT only
   nClusters += nBits; // let main loop treat mask.bit0 onwards as valid
   for (; nClusters>=32; nClusters-=32) {
      if (mask) return TRUE;
      mask = (*Bitmap++);
   }
   if (nClusters) {
      if (mask & ((1<<nClusters)-1)) return TRUE;
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC int
FAT_IsBlockUsed(HFSYS hFAT, UINT iBlock, UINT SectorsPerBlockShift)
{
   if (hFAT) {
      PFATVOL pFAT = (PFATVOL)hFAT;
      UINT SectorsPerBlock = (1<<SectorsPerBlockShift);
      HUGE LBAstart, LBAend, htemp;

      LBAstart = MAKEHUGE(iBlock,0);

      hugeop_shl(LBAstart, LBAstart, SectorsPerBlockShift);
      if (hugeop_compare(LBAstart,pFAT->boots.ClusterBeginLBA)>=0) { // if LBAstart>=BootSectorLBA ...
         LBAend = LBAstart;
         hugeop_adduint(LBAend, LBAend, SectorsPerBlock);
         if (hugeop_compare(LBAend,pFAT->LastSectorLBA)<=0) { // if LBAend<LastSectorLBA ...
            UINT iFirstCluster,iLastCluster;

            // convert start/end LBA to volume relative form.
            hugeop_sub(LBAstart,LBAstart,pFAT->boots.ClusterBeginLBA);
            hugeop_sub(LBAend,LBAend,pFAT->boots.ClusterBeginLBA);
            hugeop_shr(htemp, LBAstart, pFAT->SectorsPerClusterShift);
            iFirstCluster = LO32(htemp);
            hugeop_shr(htemp, LBAend, pFAT->SectorsPerClusterShift);
            iLastCluster = LO32(htemp);

            // we need to take account of the possibility that block boundaries are not
            // aligned with clusters (the LBAstart shift is safe).
            if (LO32(LBAend) & ((1<<(pFAT->SectorsPerClusterShift))-1)) iLastCluster++;

            if (SomeClustersUsed(pFAT,iFirstCluster,iLastCluster-iFirstCluster)) return FSYS_BLOCK_USED;
            return FSYS_BLOCK_UNUSED;
         }
      }
   }
   return FSYS_BLOCK_OUTSIDE;
}

/*==============================================================================================*/
// Steps for growing a FAT partition.
//
// Note that the region of disk space mapped to cluster numbers begins after the end of the FAT,
// i.e. the size of the FAT does not affect the number of the first cluster (it's always 2). This
// is convenient, since it means that the FAT can grow without affecting what cluster numbers are
// used by an existing file. We only need to calculate what size of FAT is required to map the new
// cluster count, then expand the old FAT (and backup FAT(s)). I'm assuming that I will not attempt
// to change the cluster size used in the partition.
//
// The only complication arises when the user tries to expand a FAT16 partition beyond the number
// of clusters that can be expressed in 16bits. In that case conversion to FAT32 would be possible,
// but may be pointless if the guest OS doesn't support FAT32, e.g. if it's a DOS or Win 3.1
// partition. If we were to do a conversion then it's quite straightforward, but we must remember
// to copy the root directory to a normal directory stream, and rewrite the boot sector etc
// structures.
//
// If we were to allow changing the cluster size (say from 32K down to 4K) then that too would be
// possible, but a lot more work. Cluster chains in the FAT itself are easily corrected, but we would
// also have to recurse through every directory entry on the drive, patching start cluster numbers.
//
/*==============================================================================================*/

/*.....................................................*/

static UINT
CalcSolution(PFATVOL pFAT, UINT nSectors)
// nSectors is the size of the partition. We need to find a way to divide the
// partition into FAT sectors and cluster-mapped sectors, such that the
// FAT area is large enough to address all of the available clusters.
{
   UINT SectorsPerFAT;
   UINT nFATslots,nClusters,MaxClusters,CPFSshift;

   if (pFAT->boots.FATtype==FAT_TYPE_FAT16) {
      
      // A 16-bit FAT index can address up to 65536 FAT slots, however two slots are reserved
      // and some FAT indices are reserved for special purposes (such as the bad cluster mark 0xFFF7),
      // so in fact the maximum number of clusters on a FAT16 volume is 65524, and the FAT table itself
      // would occupy 128K (256 sectors).
      MaxClusters = MAX_CLUSTERS_FAT16;
      CPFSshift = 8; // clusters per FAT sector, expressed as a shift.

   } else {

      // Max clusters on a FAT32 volume is limited for reasons similar to the above. Note that
      // a "FAT32" FAT index is actually 28bits, not 32bits.
      MaxClusters = MAX_CLUSTERS_FAT32;
      CPFSshift = 7; // clusters per FAT sector, expressed as a shift.
   }

   // generate an initial estimate for SectorsPerFAT (this will always be a close underestimate).
   nSectors -= (pFAT->boots.ReservedSectors+pFAT->boots.RootDirSectors);
   SectorsPerFAT = nSectors / (pFAT->boots.NumFATs + (pFAT->boots.SectorsPerCluster<<CPFSshift));

   // polish the estimate.
   for (;;) {
      nFATslots = (SectorsPerFAT<<CPFSshift); // # of clusters this FAT could handle
      nClusters = (nSectors-SectorsPerFAT*pFAT->boots.NumFATs)>>pFAT->SectorsPerClusterShift; // # of clusters I'd actually have
      if (nFATslots >= (nClusters+2)) break; // stop if solution found
      SectorsPerFAT++;
   }
   if (nClusters>MaxClusters) SectorsPerFAT = 0;

   return SectorsPerFAT;
}

/*.....................................................*/

static void
MarkTrailingClusters(PFATVOL pFAT, UINT nClusters, UINT mark)
{
   UINT nFATslots,SectorsPerFAT = pFAT->boots.SectorsPerFAT;
   nClusters += 2;
   if (pFAT->boots.FATtype==FAT_TYPE_FAT16) {
      WORD *pFATmem = pFAT->pFATmem;
      nFATslots = (SectorsPerFAT<<8);
      for (; nClusters<nFATslots; nClusters++) pFATmem[nClusters] = (WORD)mark;
   } else {
      UINT *pFATmem = pFAT->pFATmem;
      nFATslots = (SectorsPerFAT<<7);
      for (; nClusters<nFATslots; nClusters++) pFATmem[nClusters] = mark;
   }
}

/*.....................................................*/

static UINT
CountFreeClusters(PFATVOL pFAT, UINT nClusters)
{
   UINT i,nFree=0;
   if (pFAT->boots.FATtype==FAT_TYPE_FAT16) {
      WORD *pFATmem = ((WORD*)(pFAT->pFATmem))+2;
      for (i=0; i<nClusters; i++) nFree += (pFATmem[i]==0);
   } else {
      UINT *pFATmem = ((UINT*)(pFAT->pFATmem))+2;
      for (i=0; i<nClusters; i++) nFree += ((pFATmem[i] & 0x0FFFFFFF)==0);
   }
   return nFree;
}

/*.....................................................*/

static void
PatchBootSector(PFATVOL pFAT, HCOW cow, UINT iLBA, UINT SectorsInVolume, UINT SectorsPerFAT, UINT cHeads)
{
   BYTE *pb = raw_boot_sector;
   *((WORD*)(pb+26)) = (WORD)cHeads;
   if (pFAT->boots.FATtype==FAT_TYPE_FAT16 && SectorsInVolume<0x10000) {
      *((WORD*)(pb+19)) = (WORD)SectorsInVolume;
      *((UINT*)(pb+32)) = 0;
   } else {
      *((WORD*)(pb+19)) = (WORD)0;
      *((UINT*)(pb+32)) = SectorsInVolume;
      if (pFAT->boots.FATtype==FAT_TYPE_FAT32) {
         *((UINT*)(pb+36)) = SectorsPerFAT;
      }
   }
   cow->WriteSectors(cow,raw_boot_sector,iLBA,1);
   if (pFAT->boots.FATtype==FAT_TYPE_FAT32) {
      cow->WriteSectors(cow,raw_boot_sector,iLBA+6,1);
   }
}

/*.....................................................*/

static void
PatchFSInfoSector(PFATVOL pFAT, HCOW cow, UINT iLBA, UINT nClusters)
{
   BYTE *pb = raw_boot_sector;
   *((UINT*)(pb+488)) = CountFreeClusters(pFAT,nClusters); // free cluster count needs to be recalculated.
   cow->WriteSectors(cow,raw_boot_sector,iLBA,1);
   if (pFAT->boots.FATtype==FAT_TYPE_FAT32) {
      cow->WriteSectors(cow,raw_boot_sector,iLBA+6,1);
   }
}

/*.....................................................*/

PUBLIC UINT
FAT_GrowPartition(HVDDR hVDI, UINT iLBA, UINT OldSectors, UINT NewSectors, UINT cHeads)
{
   HFSYS hFAT = FAT_OpenVolume(hVDI,iLBA,OldSectors,512);
   if (hFAT) {
      PFATVOL pFAT = (PFATVOL)hFAT;
      UINT newSectorsPerFAT = CalcSolution(pFAT,NewSectors);
      if (newSectorsPerFAT) {
         HCOW cow = (HCOW)hVDI;
         UINT extraFAT = ((newSectorsPerFAT - pFAT->boots.SectorsPerFAT)*pFAT->boots.NumFATs);
         UINT i,iInsert,nReserved,newClusters;

         // insert space for expanded FAT right after existing FATs
         if (extraFAT) { // number of extra sectors required
            // we create space by relocating new sectors from the end of the partition to
            // a point immediately after the existing FATs. This does not change the partition length.
            iInsert = pFAT->boots.ReservedSectors + pFAT->boots.SectorsPerFAT*pFAT->boots.NumFATs; // LSN where we need the space
            cow->MoveSectors(cow,iLBA+NewSectors-extraFAT,iLBA+iInsert,extraFAT);

            // expand memory allocation for FAT.
            pFAT->pFATmem = Mem_ReAlloc(pFAT->pFATmem, MEMF_ZEROINIT, newSectorsPerFAT<<9);
         }

         // any clusters which are beyond the end of the old partition should be marked unused.
         MarkTrailingClusters(pFAT,pFAT->boots.nClusters,0);

         // any clusters which are beyond the end of the new partition should be marked used.
         nReserved = pFAT->boots.ReservedSectors + newSectorsPerFAT*pFAT->boots.NumFATs + pFAT->boots.RootDirSectors;
         newClusters = ((NewSectors - nReserved)>>pFAT->SectorsPerClusterShift);
         MarkTrailingClusters(pFAT,newClusters,0xFFFFFF7); // magic number marks clusters as bad

         // write all FAT copies
         iInsert = pFAT->boots.ReservedSectors;
         for (i=0; i<pFAT->boots.NumFATs; i++) {
            cow->WriteSectors(cow,pFAT->pFATmem,iLBA+iInsert,newSectorsPerFAT);
            iInsert += newSectorsPerFAT;
         }

         PatchBootSector(pFAT,cow,iLBA,nReserved+(newClusters<<pFAT->SectorsPerClusterShift),newSectorsPerFAT,cHeads);
         if (pFAT->boots.FSInfoSector) {
            hVDI->ReadSectors(hVDI,raw_boot_sector,iLBA+pFAT->boots.FSInfoSector,1);
            PatchFSInfoSector(pFAT,cow,iLBA+pFAT->boots.FSInfoSector,newClusters);
         }

         OldSectors = NewSectors;
      }
      FAT_CloseVolume(hFAT);
   }
   return OldSectors;
}

/*.....................................................*/

/* end of fat.c */


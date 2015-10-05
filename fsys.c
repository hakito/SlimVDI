/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "fsys.h"
#include "djstring.h"

// one include per supported filesystem
#include "ntfs.h"
#include "extx.h"
#include "fat.h"
#include "unpart.h"

/*.....................................................*/

PUBLIC HFSYS
FSys_OpenVolume(UINT PartCode, HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize)
{
   if (PartCode==0xFFFFFFFF) { // map unpartitioned regions of source drive
      return Unpart_OpenVolume(hVDI,iLBA,cLBA,cSectorSize);
   } else if ((PartCode==7) || (PartCode==0x42) && NTFS_IsNTFSVolume(hVDI,iLBA)) {
      return NTFS_OpenVolume(hVDI,iLBA,cLBA,cSectorSize);
   } else if ((PartCode==0x83) && Extx_IsLinuxVolume(hVDI,iLBA)) {
      return Extx_OpenVolume(hVDI,iLBA,cLBA,cSectorSize);
   } else if ((PartCode<0x100) && FAT_IsFATVolume(hVDI,iLBA)) {
      return FAT_OpenVolume(hVDI,iLBA,cLBA,cSectorSize);
   };
   
   return NULL;
}

/*.....................................................*/

PUBLIC UINT
FSys_GrowPartition(UINT PartCode, HVDDR hVDI, UINT iLBA, UINT OldSectors, UINT NewSectors, UINT cHeads)
{
   if ((PartCode==7) && NTFS_IsNTFSVolume(hVDI,iLBA)) {
      return NTFS_GrowPartition(hVDI,iLBA,OldSectors,NewSectors,cHeads);
   } else if ((PartCode==0x83) && Extx_IsLinuxVolume(hVDI,iLBA)) {
      return Extx_GrowPartition(hVDI,iLBA,OldSectors,NewSectors,cHeads);
   } else if ((PartCode<0x100) && FAT_IsFATVolume(hVDI,iLBA)) {
      return FAT_GrowPartition(hVDI,iLBA,OldSectors,NewSectors,cHeads);
   };
   
   return OldSectors;
}

/*.....................................................*/

/* end of fsys.c */

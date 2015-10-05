/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "vdiw.h"
#include "vdistructs.h"
#include "mem.h"
#include "djfile.h"
#include "djstring.h"
#include "filename.h"
#include "random.h"
#include "partinfo.h"
#include "env.h"
#include "ids.h"

#define DO_MBR_FIX        1

static UINT LastError;

#define FN_MAX 2048

typedef struct {
   VDI_HEADER hdr;
   FILE f;
   UINT BlockSizeShift;
   UINT AlignLBA;
   UINT *blockmap;
   char fn[2048];
} VDIW_INFO, *PVDI;

// localization strings
static PSTR pszOK        /* = "Ok" */;
static PSTR pszUNKERROR  /* = "Unknown Error" */;
static PSTR pszVWERRPATH /* = "Destination path does not exist" */;
static PSTR pszVWEXISTS  /* = "Destination file already exists" */;
static PSTR pszVWNOSPACE /* = "Not enough free space on destination drive" */;
static PSTR pszVWWRPROT  /* = "Dest drive is write protected" */;
static PSTR pszVWACCDEN  /* = "Access to dest drive was denied" */;
static PSTR pszVWNOMEM   /* = "Not enough memory" */;
static PSTR pszVWWRERR   /* = "Drive write error" */;
static PSTR pszVWBLOCK   /* = "Attempted block write past end of virtual disk" */;
static PSTR pszVWEXISTSQ /* = "Destination already exists. Are you sure you want to overwrite it?" */;
static PSTR pszVWEXISTSC /* = "File Exists" */;

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

PUBLIC UINT
VDIW_GetLastError(void)
{
   return LastError;
}

/*.....................................................*/

PUBLIC PSTR
VDIW_GetErrorString(UINT nErr)
{
   PSTR pszErr;
   if (nErr==0xFFFFFFFF) nErr = LastError;
   switch (nErr) {
      case VDIW_ERR_NONE:
         pszErr=RSTR(OK);
         break;
      case VDIW_ERR_BADPATH:
         pszErr=RSTR(VWERRPATH);
         break;
      case VDIW_ERR_EXISTS:
         pszErr=RSTR(VWEXISTS);
         break;
      case VDIW_ERR_NOSPACE:
         pszErr=RSTR(VWNOSPACE);
         break;
      case VDIW_ERR_WPROTECT:
         pszErr=RSTR(VWWRPROT);
         break;
      case VDIW_ERR_ACCDENY:
         pszErr=RSTR(VWACCDEN);
         break;
      case VDIW_ERR_NOMEM:
         pszErr=RSTR(VWNOMEM);
         break;
      case VDIW_ERR_WRITE:
         pszErr=RSTR(VWWRERR);
         break;
      case VDIW_ERR_BLOCKNO:
         pszErr = RSTR(VWBLOCK);
         break;
      default:
         pszErr = RSTR(UNKERROR);
   }
   return pszErr;
}

/*.....................................................*/

static BOOL
AllZero(BYTE *buffer, UINT len)
{
   UINT i,*pdw,ndw;

   pdw = (UINT*)buffer;
   ndw = (len>>2);
   for (i=ndw; i; i--) {
      if (*pdw++) return FALSE;
   }
   buffer = (BYTE*)pdw;
   len &= 3;
   for (i=len; i; i--) {
      if (*buffer++) return FALSE;
   }
   return TRUE;
}

/*.....................................................*/

static BOOL
CheckDiskSpace(CPFN pfn, UINT BlockSize, UINT nBlocks)
{
   HUGE freespace;
   FNCHAR path[1024];

   Filename_SplitPath(pfn,path,NULL);
   if (path[0]=='\\' && path[1]=='\\') { // UNC names need a trailing path separator.
      int slen = Filename_Length(path);
      if (path[slen-1]!='\\') {
         path[slen] = '\\';
         path[slen+1] = (FNCHAR)0;
      }
   }

   LastError = 0;
   if (Env_GetDiskFreeSpace(path,&freespace)) {
      HUGE sizerequired;

      sizerequired = nBlocks+2; // +2 for header etc.
      hugeop_shl(sizerequired,sizerequired,PowerOfTwo(BlockSize));
      hugeop_sub(freespace, freespace, sizerequired); // this calculates free space remaining after cloning.
      if (HI32(freespace)&0x80000000) { // if freespace went negative...
         LastError = VDIW_ERR_NOSPACE;
      }
   } else {
      LastError = VDIW_ERR_BADPATH;
   }

   return (LastError==0);
}

/*.....................................................*/

static void
InitUUID(S_UUID *pUUID, BOOL bZero)
{
   if (bZero) {
       pUUID->au32[0] = 0;
       pUUID->au32[1] = 0;
       pUUID->au32[2] = 0;
       pUUID->au32[3] = 0;
   } else {
       int i;
       for (i=0; i<16; i++) pUUID->au8[i] = (BYTE)Random_Integer(256);
       pUUID->Gen.u8ClockSeqHiAndReserved = (BYTE)((pUUID->Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80);
       pUUID->Gen.u16TimeHiAndVersion     = (WORD)((pUUID->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000);
   }
}

/*.....................................................*/

static void
CalcDriveGeometry(VDIDISKGEOMETRY *geom, UINT nSectors)
{
   if (nSectors >= (1024*255*63)) {
      geom->cCylinders       = 1024;
      geom->cHeads           = 255;
   } else {
      UINT H = 16;
      UINT C = nSectors / (16*63);
      while (C>1024) {
         H += H;
         C >>= 1;
      }
      if (H>255) {
         H = 255;
         C = nSectors / (255*63);
      }
      geom->cCylinders       = C;
      geom->cHeads           = H;
   }
   geom->cSectorsPerTrack = 63;
   geom->cBytesPerSector  = 512;
}

/*.....................................................*/

static void
InitHeader(PVDI pVDI, UINT BlockSize, UINT nBlocks)
{
   pVDI->hdr.cbSize    = sizeof(VDI_HEADER);
   pVDI->hdr.vdi_type  = VDI_TYPE_DYNAMIC;
   pVDI->hdr.vdi_flags = 0;
   pVDI->hdr.vdi_comment[0] = (char)0;
   pVDI->hdr.offset_Blocks = ((sizeof(VDI_HEADER) + 511) & ~511);
   pVDI->hdr.offset_Image = (((pVDI->hdr.offset_Blocks + nBlocks*sizeof(UINT)) + 511) & ~511);
   pVDI->AlignLBA         = 63;

   pVDI->hdr.LegacyGeometry.cCylinders       = 0;
   pVDI->hdr.LegacyGeometry.cHeads           = 0;
   pVDI->hdr.LegacyGeometry.cSectorsPerTrack = 0;
   pVDI->hdr.LegacyGeometry.cBytesPerSector  = 512;
   pVDI->hdr.dwDummy = 0;
   pVDI->hdr.DiskSize    = nBlocks;
   hugeop_shl(pVDI->hdr.DiskSize, pVDI->hdr.DiskSize, pVDI->BlockSizeShift);
   pVDI->hdr.BlockSize    = BlockSize;
   pVDI->hdr.cbBlockExtra = 0;
   pVDI->hdr.nBlocks      = nBlocks;
   pVDI->hdr.nBlocksAllocated = 0;
   InitUUID(&pVDI->hdr.uuidCreate, TRUE);
   InitUUID(&pVDI->hdr.uuidModify, FALSE);
   InitUUID(&pVDI->hdr.uuidLinkage, TRUE);
   InitUUID(&pVDI->hdr.uuidParentModify, TRUE);

   CalcDriveGeometry(&pVDI->hdr.LCHSGeometry,nBlocks<<(pVDI->BlockSizeShift-9));
}

/*.....................................................*/

PUBLIC HVDIW
VDIW_Create(CPFN fn, UINT BlockSize, UINT nBlocks, UINT nBlocksUsed)
{
   if (CheckDiskSpace(fn,BlockSize,nBlocksUsed)) {
      FILE f = File_Create(fn,DJFILE_FLAG_WRITETHROUGH|DJFILE_FLAG_SEQUENTIAL);
//    FILE f = CreateFile(fn,GENERIC_WRITE,0,0,CREATE_NEW,FILE_FLAG_WRITE_THROUGH|FILE_FLAG_SEQUENTIAL_SCAN,0);
_try_again:
      LastError = 0;
      if (f == NULLFILE) {
         UINT IOR = File_IOresult();
         if (IOR == DJFILE_ERROR_ACCESS_DENIED) LastError = VDIW_ERR_ACCDENY;
         else if (IOR == DJFILE_ERROR_WRITEPROTECT) LastError = VDIW_ERR_WPROTECT;
         else {
            LastError = VDIW_ERR_EXISTS;
            if (Env_AskYN(RSTR(VWEXISTSQ), RSTR(VWEXISTSC))) {
               f = File_Create(fn,DJFILE_FLAG_WRITETHROUGH|DJFILE_FLAG_SEQUENTIAL|DJFILE_FLAG_OVERWRITE);
//             f = CreateFile(fn,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH|FILE_FLAG_SEQUENTIAL_SCAN,0);
               goto _try_again;
            }
         }
      }
      if (!LastError) {
         PVDI pVDI = Mem_Alloc(MEMF_ZEROINIT, sizeof(VDIW_INFO));
         String_Copy(pVDI->fn, fn, FN_MAX);
         pVDI->f = f;
         pVDI->BlockSizeShift = PowerOfTwo(BlockSize);
         InitHeader(pVDI,BlockSize,nBlocks);
         return (HVDIW)pVDI;
      }
   }
   return NULL;
}

/*.....................................................*/

PUBLIC BOOL
VDIW_SetDriveUUID(HANDLE hVDI, S_UUID *pUUID)
{
   LastError = VDIW_ERR_HANDLE;
   if (hVDI) {
      PVDI pVDI = (PVDI)hVDI;
      Mem_Copy(&pVDI->hdr.uuidCreate,pUUID,sizeof(S_UUID));
      LastError = 0;
   }
   return (LastError==0);
}

/*.....................................................*/

PUBLIC BOOL
VDIW_SetDriveUUIDs(HANDLE hVDI, S_UUID *uuid, S_UUID *modifyUUID)
{
   LastError = VDIW_ERR_HANDLE;
   if (hVDI) {
      PVDI pVDI = (PVDI)hVDI;
      Mem_Copy(&pVDI->hdr.uuidCreate, uuid, sizeof(S_UUID));
      if (modifyUUID)
         Mem_Copy(&pVDI->hdr.uuidModify, modifyUUID, sizeof(S_UUID));
      LastError = 0;
   }
   return (LastError==0);
}

PUBLIC BOOL
VDIW_SetParentUUIDs(HANDLE hVDI, S_UUID *parentUUID, S_UUID *parentModifyUUID)
{
   LastError = VDIW_ERR_HANDLE;
   if (hVDI) {
      PVDI pVDI = (PVDI)hVDI;
      Mem_Copy(&pVDI->hdr.uuidLinkage, parentUUID, sizeof(S_UUID));
      if (parentModifyUUID)
         Mem_Copy(&pVDI->hdr.uuidParentModify, parentModifyUUID, sizeof(S_UUID));
      LastError = 0;
   }
   return (LastError==0);
}

/*.....................................................*/

static BYTE padding[4096];

static void
WritePadding(PVDI pVDI, UINT file_offset)
{
   UINT bytes,current_pos = File_GetPos(pVDI->f, NULL);

   Mem_Zero(padding,4096);
   while (current_pos < file_offset) {
      bytes = file_offset - current_pos;
      if (bytes>4096) bytes = 4096;
      File_WrBin(pVDI->f, padding, bytes);
      current_pos += bytes;
   }
}

/*.....................................................*/

static void
WriteHeader(PVDI pVDI)
{
   VDI_PREHEADER vph;
   UINT i,LBA_boot_partition;

   // write pre-header first.
   Mem_Zero(&vph,sizeof(vph));
// String_Copy(vph.szFileInfo,pszVdiInfo,64);
   String_Copy(vph.szFileInfo,pszVdiInfoSlimVDI,64);
   vph.u32Signature = VDI_SIGNATURE;
   vph.u32Version = VDI_LATEST_VERSION;
   File_WrBin(pVDI->f, &vph, sizeof(vph));

   // then write the true header.
   if (AllZero(pVDI->hdr.uuidCreate.au8,16)) InitUUID(&pVDI->hdr.uuidCreate,FALSE);
   File_WrBin(pVDI->f, &pVDI->hdr, sizeof(VDI_HEADER));

   // alloc memory for the block map, then initialize it.
   pVDI->blockmap = Mem_Alloc(0,pVDI->hdr.nBlocks*sizeof(UINT));
   for (i=0; i<pVDI->hdr.nBlocks; i++) pVDI->blockmap[i] = VDI_PAGE_FREE;
   WritePadding(pVDI, pVDI->hdr.offset_Blocks); // pad out to blockmap offset.
   File_WrBin(pVDI->f, pVDI->blockmap, pVDI->hdr.nBlocks*sizeof(UINT));

   // VirtualBox aligns the drive image data on a sector boundary within the VDI file. I also
   // want to align it such that the start sector of the boot partition is aligned on a
   // 4K(8 sector) boundary. This should give a speed boost and only costs <4K
   // extra space in the VDI.
   LBA_boot_partition = (pVDI->hdr.offset_Image>>9)+pVDI->AlignLBA;
   while (LBA_boot_partition & 7) {
      pVDI->hdr.offset_Image += 512;
      LBA_boot_partition++;
   }

   // pad out to the image offset
   WritePadding(pVDI, pVDI->hdr.offset_Image);
}

/*.....................................................*/

static BOOL
IsMBR(BYTE *MBR)
{
   if (MBR[510]==0x55 && MBR[511]==0xAA) {
      PPART pPart = (PPART)(MBR+446);
      UINT i,LBA,LBAtest=1;
      for (i=0; i<4; i++) { // check that boot codes and LBA values seem ok.
         if (pPart->Status!=0 && pPart->Status!=0x80) break;
         LBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (LBA==0) continue;
         if (LBA<LBAtest) break; // if this is really an MBR then the partitions overlap!
         LBAtest = LBA + MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
         pPart++;
      }
      if (i==4) return TRUE;
   }
   return FALSE;
}

/*....................................................*/
#if DO_MBR_FIX
static void
LBA2CHS(PVDI pVDI, BYTE *pC, BYTE *pH, BYTE *pS, UINT LBA)
{
   UINT C,H,S,temp;
   if (LBA>=(1023*254*63)) {
      C = 1023; H=254; S=63;
   } else {
      C = LBA / (pVDI->hdr.LCHSGeometry.cHeads*63);
      temp = LBA - (C*pVDI->hdr.LCHSGeometry.cHeads*63);
      H = temp / 63;
      S = (temp - H*63)+1;
   }
   *pC = (BYTE)(C & 0xFF);
   *pH = (BYTE)H;
   *pS = (BYTE)(((C>>2)&0xC0) + S);
}
#endif
/*....................................................*/

#if DO_MBR_FIX
static BOOL
PatchBootSector(BYTE *bs, UINT cHeads)
// Patch "Number of heads" BPB field in NTFS/DOS volume boot sector.
{
   BOOL bIsFAT32;
   CHAR sz[8];
   Mem_Copy(sz,bs+3,5);
   sz[5] = (CHAR)0;
   bIsFAT32 = (String_Compare(sz,"MSWIN")==0); // not strictly correct, but close enough.
   if (bIsFAT32 || String_Compare(sz,"NTFS ")==0 ||
       String_Compare(sz,"MSDOS")==0 || String_Compare(sz,"FRDOS")==0) {
      bs[26] = (BYTE)cHeads;
      bs[27] = (BYTE)0;
   }
   return bIsFAT32;
}
#endif

/*....................................................*/

static PPART
FindLargestPartition(BYTE *MBR)
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
            iLargest = i;
            Largest = LBAsize;
         }
         LBAtest = LBA + LBAsize; // where next partition should start
         pPart++;
      }
      if (i==4) return (PPART)(MBR+446+(iLargest<<4));
   }
   return NULL;
}

/*.....................................................*/

static void
SetAlignment(PVDI pVDI, BYTE *MBR)
{
   if (IsMBR(MBR)) {
      PPART pPart = FindLargestPartition(MBR);
      pVDI->AlignLBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA); // start LBA of first partition.
   }
}

/*.....................................................*/

PUBLIC BOOL
VDIW_FixMBR(HVDIW hVDI, BYTE *MBR)
{
#if DO_MBR_FIX
   LastError = VDIW_ERR_HANDLE;
   if (hVDI) {
      PVDI pVDI = (PVDI)hVDI;
      if (IsMBR(MBR)) {
         PPART pPart = (PPART)(MBR+446);
         UINT i,PartSize,LBA;
         for (i=0; i<4; i++) { // check that boot codes and LBA values seem ok.
            LBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
            PartSize = MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
            if (LBA==0 || PartSize==0) continue;
            LBA2CHS(pVDI,&pPart->pstart_chs_cyl,&pPart->pstart_chs_head,&pPart->pstart_chs_sect,LBA);
            LBA2CHS(pVDI,&pPart->pend_chs_cyl,&pPart->pend_chs_head,&pPart->pend_chs_sect,LBA+PartSize-1);
            pPart++;
         }

         // also patch "number of heads for BIOS calls" word at offset 26 in FAT or NTFS volume boot sector.
         pPart = (PPART)(MBR+446);
         LBA = MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (LBA < 2048) {
            if (PatchBootSector(MBR+(LBA<<9), pVDI->hdr.LCHSGeometry.cHeads)) {
               LBA += 6; // ditto for backup boot sector (exists on FAT32 only).
               if (LBA<2048) PatchBootSector(MBR+(LBA<<9), pVDI->hdr.LCHSGeometry.cHeads);
            }
         }
      }
      LastError = 0;
   }
   return (LastError==0);
#else
   return FALSE;
#endif
}

/*.....................................................*/

PUBLIC BOOL
VDIW_WritePage(HVDIW hVDI, void *buffer, UINT iPage, BOOL bAllZero)
{
   LastError = VDIW_ERR_HANDLE;
   if (hVDI) {
      PVDI pVDI = (PVDI)hVDI;
      LastError = VDIW_ERR_BLOCKNO;
      if (iPage<pVDI->hdr.nBlocks) {
         LastError = 0;
         if (!pVDI->blockmap) {
            if (iPage==0) SetAlignment(pVDI,buffer); // align boot partition on 4K VDI file boundary.
            WriteHeader(pVDI);
         }
         if (LastError==0) {
            if (!buffer) {
               LastError = 0;
            } else if (!VDI_BLOCK_ALLOCATED(pVDI->blockmap[iPage])) {
               if (bAllZero || AllZero(buffer,pVDI->hdr.BlockSize)) {
                  pVDI->blockmap[iPage] = VDI_PAGE_ZERO;
               } else {
                  // we need to allocate and write a new block.
                  pVDI->blockmap[iPage] = pVDI->hdr.nBlocksAllocated;
                  pVDI->hdr.nBlocksAllocated++;
                  LastError = VDIW_ERR_WRITE;
                  if (File_WrBin(pVDI->f,buffer,pVDI->hdr.BlockSize)==pVDI->hdr.BlockSize) {
                     LastError = 0;
                  }
               }
            } else {
               // an already allocated block is being rewritten.
               HUGE seekpos;
               if (bAllZero) Mem_Zero(buffer,pVDI->hdr.BlockSize);
               seekpos = (((HUGE)iPage)<<pVDI->BlockSizeShift);
               seekpos += pVDI->hdr.offset_Image;
               File_Seek(pVDI->f,seekpos);
               File_Size(pVDI->f,&seekpos);
               File_WrBin(pVDI->f,buffer,pVDI->hdr.BlockSize);
               File_Seek(pVDI->f,seekpos);
            }
         }
      }
   }
   return (LastError==0);
}

/*.....................................................*/

PUBLIC HVDIW
VDIW_Close(HVDIW hVDI)
{
   LastError = 0;
   if (hVDI) { // we silently handle closing of an already closed file.
      // write final header and block map.
      PVDI pVDI = (PVDI)hVDI;
      if (!pVDI->blockmap) {
         if (!pVDI->blockmap) WriteHeader(pVDI); // source VDI was empty.
      } else {
         File_Seek(pVDI->f, sizeof(VDI_PREHEADER));
         File_WrBin(pVDI->f, &pVDI->hdr, sizeof(VDI_HEADER));
         File_Seek(pVDI->f, pVDI->hdr.offset_Blocks);
         File_WrBin(pVDI->f, pVDI->blockmap, pVDI->hdr.nBlocks*sizeof(UINT));
      }
      File_Close(pVDI->f);
      Mem_Free(pVDI->blockmap);
      Mem_Free(pVDI);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC HVDIW
VDIW_Discard(HVDIW hVDI)
{
   LastError = 0;
   if (hVDI) { // we silently handle closing of an already closed file.
      PVDI pVDI = (PVDI)hVDI;
      File_Close(pVDI->f);
      File_Erase(pVDI->fn);
      Mem_Free(pVDI->blockmap);
      Mem_Free(pVDI);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC BOOL
VDIW_SetFileSize(HVDIW hVDI, UINT nBlocks)
{
   LastError = VDIW_ERR_HANDLE;
   if (hVDI) {
      /*
      PVDI pVDI = (PVDI)hVDI;
      HUGE pos,old_pos;

      File_GetPos(pVDI->f, &old_pos);

      pos = nBlocks;
      hugeop_shl(pos,pos,pVDI->BlockSizeShift);
      hugeop_adduint(pos,pos,pVDI->hdr.offset_Image);
      File_Seek(pVDI->f, pos);
      SetEndOfFile(pVDI->f);
      File_Seek(pVDI->f, old_pos);
      */
      LastError = 0;
   }
   return (LastError==0);
}

/*.....................................................*/

/* end of module vdiw.c */


/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "hddr.h"
#include "parallels.h"
#include "mem.h"
#include "djfile.h"
#include "env.h"
#include "ids.h"
#include "djstring.h"

// error codes
#define HDDR_ERR_NONE         0
#define HDDR_ERR_FILENOTFOUND ((VDD_TYPE_PARALLELS<<16)+1)
#define HDDR_ERR_READ         ((VDD_TYPE_PARALLELS<<16)+2)
#define HDDR_ERR_NOTHDD       ((VDD_TYPE_PARALLELS<<16)+3)
#define HDDR_ERR_INVHANDLE    ((VDD_TYPE_PARALLELS<<16)+4)
#define HDDR_ERR_OUTOFMEM     ((VDD_TYPE_PARALLELS<<16)+5)
#define HDDR_ERR_INVBLOCK     ((VDD_TYPE_PARALLELS<<16)+6)
#define HDDR_ERR_BADFORMAT    ((VDD_TYPE_PARALLELS<<16)+7)
#define HDDR_ERR_SEEK         ((VDD_TYPE_PARALLELS<<16)+8)
#define HDDR_ERR_BLOCKMAP     ((VDD_TYPE_PARALLELS<<16)+9)
#define HDDR_ERR_SHARE        ((VDD_TYPE_PARALLELS<<16)+10)

PUBLIC PSTR HEADER_MAGIC    = "WithoutFreeSpace";

static UINT OSLastError;

typedef struct {
   CLASS(VDDR) Base;
   HDD_HEADER hdr;
   FILE f;
   UINT *blockmap;
} HDD_INFO, *PHDD;

static PSTR pszOK          /* = "Ok" */;
static PSTR pszUNKERROR    /* = "Unknown Error" */;
static PSTR pszVDNOEXIST   /* = "The source file does not exist" */;
static PSTR pszVDERRSHARE  /* = "Source file already in use (is VirtualBox running?)" */;
static PSTR pszVDERRREAD   /* = "Got OS error %lu when reading from source file" */;
static PSTR pszVDNOTHDD    /* = "Source file is not a recognized HDD file format" */;
static PSTR pszVDINVHANDLE /* = "Invalid handle passed to VDx source object" */;
static PSTR pszVDNOMEM     /* = "Not enough memory to map source file" */;
static PSTR pszVDSEEKRANGE /* = "App attempted to seek beyond end of drive!" */;
static PSTR pszVDERRFMT    /* = "Source has strange format which is incompatible with this tool" */;
static PSTR pszVDERRSEEK   /* = "Got OS error %lu when seeking inside source file" */;
static PSTR pszVDBLKMAP    /* = "Source file corrupt - block map contains errors" */;

/*.....................................................*/

static UINT
PowerOfTwo(UINT x)
// Returns the base 2 log of x, same as the index of the most significant 1 bit in x.
// Returns 0xFFFFFFFF if x was 0 on entry.
{
   int y;
   for (y=(-1); x>255; x>>=9) y+=9;
   while (x) {
      x >>= 1;
      y++;
   }
   return (UINT)y;
}

/*.....................................................*/

PUBLIC UINT
HDDR_GetLastError(void)
{
   return VDDR_LastError;
}

/*.....................................................*/

PUBLIC PSTR
HDDR_GetErrorString(UINT nErr)
{
   PSTR pszErr;
   static char sz[256];
   if (nErr==0xFFFFFFFF) nErr = VDDR_LastError;
   switch (nErr) {
      case HDDR_ERR_NONE:
         pszErr=RSTR(OK);
         break;
      case HDDR_ERR_FILENOTFOUND:
         pszErr=RSTR(VDNOEXIST);
         break;
      case HDDR_ERR_SHARE:
         pszErr=RSTR(VDERRSHARE);
         break;
      case HDDR_ERR_READ:
         Env_sprintf(sz,RSTR(VDERRREAD),OSLastError);
         pszErr=sz;
         break;
      case HDDR_ERR_NOTHDD:
         pszErr=RSTR(VDNOTHDD);
         break;
      case HDDR_ERR_INVHANDLE:
         pszErr=RSTR(VDINVHANDLE);
         break;
      case HDDR_ERR_OUTOFMEM:
         pszErr=RSTR(VDNOMEM);
         break;
      case HDDR_ERR_INVBLOCK:
         pszErr=RSTR(VDSEEKRANGE);
         break;
      case HDDR_ERR_BADFORMAT:
         pszErr=RSTR(VDERRFMT);
         break;
      case HDDR_ERR_SEEK:
         Env_sprintf(sz,RSTR(VDERRSEEK),OSLastError);
         pszErr=sz;
         break;
      case HDDR_ERR_BLOCKMAP:
         pszErr = RSTR(VDBLKMAP);
         break;
      default:
         pszErr = RSTR(UNKERROR);
   }
   return pszErr;
}

/*.....................................................*/

static UINT
DoSeek(FILE f, HUGE pos)
{
   HUGE currpos;
   UINT err = 0;
   if (File_GetPos(f,&currpos)==0xFFFFFFFF) err = File_IOresult();
   else if (pos!=currpos) {
      File_Seek(f,pos);
      err = File_IOresult();
   }
   return err;
}

/*.....................................................*/

static BOOL
ReadHeader(FILE f, HDD_HEADER *hdr)
{
   VDDR_LastError = HDDR_ERR_READ;
   if (File_RdBin(f, hdr, sizeof(HDD_HEADER)) == sizeof(HDD_HEADER)) {
      VDDR_LastError = 0;
   }
   return (VDDR_LastError == 0);
}

/*.....................................................*/

static BOOL
ValidateMap(PHDD pHDD)
{
   UINT i,nAlloc,nFree;
   UINT sid;
   HUGE FileSectors;

   File_Size(pHDD->f, &FileSectors);
   FileSectors >>= 9;

   nAlloc = nFree = 0;
   for (i=0; i<pHDD->hdr.nBlocks; i++) {
      sid = pHDD->blockmap[i];
      if (sid == HDD_PAGE_FREE) nFree++;
      else if (sid<FileSectors) nAlloc++;
      else return FALSE; // SID out of range.
   }
   return TRUE;
}

/*.....................................................*/

PUBLIC HVDDR
HDDR_Open(CPFN fn, UINT iChain)
{
   PHDD pHDD = NULL;
   FILE f = File_OpenRead(fn);
   if (f==NULLFILE) {
      if (File_IOresult()==DJFILE_ERROR_SHARE) VDDR_LastError = HDDR_ERR_SHARE;
      else VDDR_LastError = HDDR_ERR_FILENOTFOUND;
   } else {
      HDD_HEADER hdr;
      if (ReadHeader(f,&hdr)) {
         VDDR_LastError = HDDR_ERR_NOTHDD;
         if (Mem_Compare(hdr.szSig,HEADER_MAGIC,16)==0 && hdr.u32Version==HEADER_VERSION) {
            UINT mapsize;
            
            pHDD = Mem_Alloc(MEMF_ZEROINIT,sizeof(HDD_INFO));
            pHDD->f = f;
            Mem_Copy(&pHDD->hdr, &hdr, sizeof(hdr));

            pHDD->Base.GetDriveType       = HDDR_GetDriveType;
            pHDD->Base.GetDriveSize       = HDDR_GetDriveSize;
            pHDD->Base.GetDriveBlockCount = HDDR_GetDriveBlockCount;
            pHDD->Base.BlockStatus        = HDDR_BlockStatus;
            pHDD->Base.GetDriveUUID       = HDDR_GetDriveUUID;
            pHDD->Base.IsSnapshot         = HDDR_IsSnapshot;
            pHDD->Base.ReadPage           = HDDR_ReadPage;
            pHDD->Base.ReadSectors        = HDDR_ReadSectors;
            pHDD->Base.Close              = HDDR_Close;

            // read the blocks map into a buffer.
            mapsize = pHDD->hdr.nBlocks*sizeof(UINT);
            pHDD->blockmap = Mem_Alloc(0, mapsize);
            VDDR_LastError = HDDR_ERR_OUTOFMEM;
            if (pHDD->blockmap) {
               VDDR_LastError = HDDR_ERR_READ;
               if (File_RdBin(pHDD->f, pHDD->blockmap, mapsize)==mapsize) {
                  VDDR_LastError = HDDR_ERR_BLOCKMAP;
                  if (ValidateMap(pHDD)) VDDR_LastError = 0;
               } else {
                  OSLastError = File_IOresult();
               }
            }

            if (VDDR_LastError) pHDD = Mem_Free(pHDD);
         }
      }
      if (!pHDD) File_Close(f);
   }
   return (HVDDR)pHDD;
}

/*.....................................................*/

PUBLIC BOOL
HDDR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid)
{
   if (pThis) {
      Mem_Zero(drvuuid, sizeof(S_UUID));
      VDDR_LastError = 0;
      return TRUE;
   }
   VDDR_LastError = HDDR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

PUBLIC BOOL
HDDR_GetHeader(HVDDR pThis, HDD_HEADER *hddh)
{
   if (pThis) {
      PHDD pHDD = (PHDD)pThis;
      VDDR_LastError = 0;
      if (hddh) Mem_Copy(hddh, &pHDD->hdr, sizeof(HDD_HEADER));
      return TRUE;
   }
   VDDR_LastError = HDDR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC int
RawReadSectors(PHDD pHDD, void *buffer, UINT iBlock, UINT offset_sector, UINT nSectors)
// Primitive of ReadSectors() used when I know I'm reading from within a single block.
{
   VDDR_LastError = HDDR_ERR_INVBLOCK;
   if (iBlock<pHDD->hdr.nBlocks) {
      UINT SID  = pHDD->blockmap[iBlock];
      VDDR_LastError = 0;
      if (SID == HDD_PAGE_FREE) {
         return VDDR_RSLT_NOTALLOC;
      } else {
         HUGE seekpos;

         seekpos = SID+offset_sector;
         seekpos <<= 9;

         VDDR_LastError = HDDR_ERR_SEEK;
         OSLastError = DoSeek(pHDD->f,seekpos);
         if (OSLastError==0) {
            UINT bytes = nSectors<<9;
            VDDR_LastError = HDDR_ERR_READ;
            if (File_RdBin(pHDD->f, buffer, bytes)==bytes) {
               VDDR_LastError = 0;
               return VDDR_RSLT_NORMAL;
            }
            OSLastError = File_IOresult();
         }
      }
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
HDDR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = HDDR_ERR_INVHANDLE;
   if (pThis) {
      BYTE *pDest=buffer;
      PHDD pHDD = (PHDD)pThis;
      UINT SectorsToCopy,SectorsLeft,iBlock;
      int  rslt,PageReadResult;

      if (nSectors==0) return VDDR_RSLT_NORMAL;

      // find which block the first LBA falls into.
      iBlock = (UINT)(LBA/pHDD->hdr.u32BlockSize);
      if (iBlock>=pHDD->hdr.nBlocks) {
         VDDR_LastError = HDDR_ERR_INVBLOCK;
         return VDDR_RSLT_FAIL;
      }

      rslt   = VDDR_RSLT_NOTALLOC;

      SectorsToCopy = 0;
      LBA         = LBA-(iBlock*pHDD->hdr.u32BlockSize);
      SectorsLeft = pHDD->hdr.u32BlockSize - ((UINT)LBA); // sectors remaining in this block

      while (nSectors) {
         SectorsToCopy = (SectorsLeft<=nSectors ? SectorsLeft : nSectors);
         nSectors -= SectorsToCopy;
         PageReadResult = RawReadSectors(pHDD, pDest, iBlock, (UINT)LBA, SectorsToCopy);
         if (PageReadResult == VDDR_RSLT_FAIL) return VDDR_RSLT_FAIL;
         if (PageReadResult != VDDR_RSLT_NORMAL) Mem_Zero(pDest,SectorsToCopy<<9);
         if (rslt>PageReadResult) rslt = PageReadResult;
         pDest += (SectorsToCopy<<9);
         LBA = 0;
         iBlock++;
         if (iBlock==pHDD->hdr.nBlocks) return rslt; // return with partial result when we reach EOF.
         SectorsLeft = pHDD->hdr.u32BlockSize;
      }

      return rslt;
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
HDDR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift)
{
   HUGE LBA;
   hugeop_fromuint(LBA,iPage);
   hugeop_shl(LBA,LBA,SPBshift);
   return HDDR_ReadSectors(pThis, buffer, LBA, 1<<SPBshift);
}

/*.....................................................*/

PUBLIC HVDDR
HDDR_Close(HVDDR pThis)
{
   VDDR_LastError = 0;
   if (pThis) { // we silently handle closing of an already closed file.
      PHDD pHDD = (PHDD)pThis;
      File_Close(pHDD->f);
      Mem_Free(pHDD->blockmap);
      Mem_Free(pHDD);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC UINT
HDDR_GetDriveType(HVDDR pThis)
{
   return VDD_TYPE_PARALLELS;
}

/*.....................................................*/

PUBLIC BOOL
HDDR_IsSnapshot(HVDDR pThis)
{
   return FALSE;
}

/*....................................................*/

PUBLIC BOOL
HDDR_GetDriveSize(HVDDR pThis, HUGE *drive_size)
{
   if (pThis) {
      PHDD pHDD = (PHDD)pThis;
      *drive_size = pHDD->hdr.DriveSize;
      *drive_size <<= 9;
      VDDR_LastError = 0;
      return TRUE;
   }
   VDDR_LastError = HDDR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC UINT
HDDR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift)
{
   if (pThis) {
      PHDD pHDD = (PHDD)pThis;
      UINT DriveSize = pHDD->hdr.DriveSize;
      DriveSize += ((1<<SPBshift)-1);
      VDDR_LastError = 0;
      return (DriveSize>>SPBshift);
   }
   VDDR_LastError = HDDR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

static UINT
BlockStatus(PHDD pHDD, HUGE LBA_start, HUGE LBA_end)
{
   UINT SID,LastSID,blks,rslt=VDDR_RSLT_NOTALLOC;
   SID = LO32(LBA_start/pHDD->hdr.u32BlockSize);
   LastSID = LO32(LBA_end/pHDD->hdr.u32BlockSize);
   for (; SID<=LastSID; SID++) {
      if (SID>=pHDD->hdr.nBlocks) break;
      blks = pHDD->blockmap[SID];
      if (blks != HDD_PAGE_FREE) return VDDR_RSLT_NORMAL;
   }
   return rslt;
}

/*.....................................................*/

PUBLIC UINT
HDDR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end)
{
   UINT rslt = VDDR_RSLT_NOTALLOC;
   VDDR_LastError = HDDR_ERR_INVHANDLE;
   if (pThis) {
      PHDD pHDD = (PHDD)pThis;
      VDDR_LastError = 0;
      rslt = BlockStatus(pHDD,LBA_start,LBA_end);
   }
   return rslt;
}

/*....................................................*/

/* end of module hddr.c */


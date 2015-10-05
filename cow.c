/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "cow.h"
#include "vddr.h"
#include "mem.h"
#include "djfile.h"
#include "env.h"
#include "djstring.h"

// error codes
#define COW_ERR_NONE        0
#define COW_ERR_INVHANDLE   ((VDD_TYPE_COW<<16)+1)
#define COW_ERR_OUTOFMEM    ((VDD_TYPE_COW<<16)+2)
#define COW_ERR_INVBLOCK    ((VDD_TYPE_COW<<16)+3)
#define COW_ERR_SEEK        ((VDD_TYPE_COW<<16)+4)
#define COW_ERR_READ        ((VDD_TYPE_COW<<16)+5)
#define COW_ERR_INVALIDMOVE ((VDD_TYPE_COW<<16)+6)

#define COW_SIGNATURE       0x434F5753 /* COWS */

#define COW_BLOCK_SIZE      1048576
#define COW_BLOCK_SECTORS   2048
#define COW_SPB_SHIFT       11

// extent source types
typedef enum {
   EXTENT_SRC_NONE, // extent represents unallocated data
   EXTENT_SRC_DISK, // data source is the underlying virtual disk
   EXTENT_SRC_COW   // data source is the cow image
} EXTENT_TYPES;

#define MAX_EXTENTS         4096

typedef UINT EXTREF;

// I take 16 bytes per extent, compared to 4 bytes per block in a VDI image. This
// is ok since I don't expect there to be too many extents. I'll start off with 1-3,
// then I add 1MB extents as sectors are modified.
//
typedef struct t_COW_EXTENT {
	EXTREF NextExtent;
	UINT ExtentBase;   // sector offset into source object where this extent is stored. Note NOT an LBA into the COW drive.
	UINT nSectors;     // extent size in sectors.
	EXTENT_TYPES type; // see extent types.
} COW_EXTENT, *PEXTENT;

typedef struct {
   CLASS(COWOVL) Base;
   UINT cowsig;
   HVDDR SourceDisk; // object instance of underlying virtual disk.
   HUGE  DiskSize;    // total size of virtual disk (sum of all extent sizes, in sectors).

   // virtual image extents info
   UINT    nExtents,MaxExtents;
   EXTREF  ExtentList;        // list of extents, in LBA order.
   EXTREF  ExtentDeletedList; // if extents are deleted they get moved to a free list.
   PEXTENT pExtentCache;      // memory allocation for extents (variable length array).

   // info about COW image file (where modified sectors are stored).
   char fnCow[1024];
   FILE fCOW;
   BOOL bCowCacheDirty;
   HUGE LBA_cache;
   HUGE cow_seekpos;
   UINT nCowBlocks;       // count of modified blocks, hence this is also the size of the COW file.
   BYTE buff[COW_BLOCK_SIZE];
} COW_INFO, *PCOW;

static UINT OSLastError;

/*...................................................................*/

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

static EXTREF
CreateExtent(PCOW pCOW, UINT LBA, UINT nSectors, EXTENT_TYPES source)
{
   EXTREF iExtent;
   PEXTENT pe;

   if (pCOW->ExtentDeletedList) {
      iExtent = pCOW->ExtentDeletedList;
      pCOW->ExtentDeletedList = pCOW->pExtentCache[iExtent].NextExtent;
   } else {
      if (pCOW->nExtents >= pCOW->MaxExtents) {
         pCOW->MaxExtents += MAX_EXTENTS;
         pCOW->pExtentCache = Mem_ReAlloc(pCOW->pExtentCache,0,sizeof(COW_EXTENT)*pCOW->MaxExtents);
      }
      iExtent = pCOW->nExtents;
      pCOW->nExtents++;
   }
   pe = pCOW->pExtentCache + iExtent;
   pe->NextExtent = 0;
   pe->ExtentBase = LBA;
   pe->nSectors = nSectors;
   pe->type = source;
   return iExtent;
}

/*...................................................................*/

static EXTREF
FindExtent(PCOW pCOW, HUGE LBA, HUGE *LBA_out)
// Find the extent which the given LBA falls into. Optionally, an output
// LBA can be returned which is the input LBA made relative to the start
// of the selected extent.
{
   EXTREF i = pCOW->ExtentList;
   while (i) {
      PEXTENT pe = pCOW->pExtentCache+i;
      if (LBA<pe->nSectors) {
         if (LBA_out) *LBA_out = LBA;
         return i;
      }
      LBA -= pe->nSectors;
      i = pe->NextExtent;
   }
   if (LBA_out) *LBA_out = LBA;
   return 0;
}

/*...................................................................*/

PUBLIC UINT
COW_GetLastError(void)
{
   return VDDR_LastError;
}

/*...................................................................*/

PUBLIC PSTR
COW_GetErrorString(UINT nErr)
{
   PSTR pszErr;
   static char sz[256];
   if (nErr==0xFFFFFFFF) nErr = VDDR_LastError;
   switch (nErr) {
      case COW_ERR_NONE:
         pszErr="Ok";
         break;
      case COW_ERR_INVHANDLE:
         pszErr="Invalid handle passed to COW module";
         break;
      case COW_ERR_OUTOFMEM:
         pszErr="Memory allocation failed - not enough memory";
         break;
      case COW_ERR_INVBLOCK:
         pszErr="App attempted to seek beyond end of drive!";
         break;
      case COW_ERR_SEEK:
         Env_sprintf(sz,"Got OS error %lu when seeking inside source disk",OSLastError);
         pszErr=sz;
         break;
      case COW_ERR_READ:
         Env_sprintf(sz,"Got OS error %lu when reading source disk",OSLastError);
         pszErr=sz;
         break;
      case COW_ERR_INVALIDMOVE:
         pszErr="Invalid COW_MoveSectors operation";
         break;
      default:
         pszErr = "Unknown error";
   }
   return pszErr;
}

/*...................................................................*/

PUBLIC UINT
COW_GetDriveType(HVDDR pThis)
{
   return VDD_TYPE_COW;
}

/*...................................................................*/

PUBLIC BOOL
COW_GetDriveSize(HVDDR pThis, HUGE *drive_size)
{
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      *drive_size = pCOW->DiskSize;
      *drive_size <<= 9;
      VDDR_LastError = 0;
      return TRUE;
   }
   VDDR_LastError = COW_ERR_INVHANDLE;
   return FALSE;
}

/*...................................................................*/

PUBLIC UINT
COW_GetDriveBlockCount(HVDDR pThis, UINT SPBshift)
{
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      UINT DiskSize = (UINT)(pCOW->DiskSize>>SPBshift);
      if ((DiskSize<<SPBshift)<pCOW->DiskSize) DiskSize++;
      VDDR_LastError = 0;
      return DiskSize;
   }
   VDDR_LastError = COW_ERR_INVHANDLE;
   return 0;
}

/*...................................................................*/

static UINT
BlockStatus(PCOW pCOW, HUGE LBA_start, HUGE LBA_end)
// Checks allocation status of one block. See comment for COW_AllocatedBlocks().
{
   UINT rslt = VDDR_RSLT_NOTALLOC;
   HUGE LBA;
   UINT iExtent;

   // Find which extent the start LBA falls inside.
   iExtent = FindExtent(pCOW,LBA_start,&LBA); // returned LBA is relative to beginning of the extent.
   if (iExtent) {
      PEXTENT pe;
      UINT nSectors,SectorsLeft,SectorsToCheck;
      pe   = pCOW->pExtentCache + iExtent;
      SectorsLeft = pe->nSectors - ((UINT)LBA); // sectors remaining in the first extent
      nSectors = ((UINT)(LBA_end-LBA_start))+1;
      LBA = LBA+pe->ExtentBase;
      while (nSectors) {
         SectorsToCheck = (SectorsLeft<=nSectors ? SectorsLeft : nSectors);
         nSectors -= SectorsToCheck;
         if (pe->type==EXTENT_SRC_COW) return VDDR_RSLT_NORMAL;
         if (pe->type==EXTENT_SRC_DISK) {
            if (pCOW->SourceDisk->BlockStatus(pCOW->SourceDisk, LBA, LBA+(SectorsToCheck-1))==VDDR_RSLT_NORMAL) return VDDR_RSLT_NORMAL;
         }
         iExtent = pe->NextExtent;
         if (!iExtent) break; // return with partial result when we reach EOF.
         pe = pCOW->pExtentCache + iExtent;
         SectorsLeft = pe->nSectors;
         LBA = pe->ExtentBase;
      }
   }
   return rslt;
}

/*.....................................................*/

PUBLIC UINT
COW_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      VDDR_LastError = 0;
      return BlockStatus((PCOW)pThis,LBA_start,LBA_end);
   }
   return FALSE;
}

/*...................................................................*/

PUBLIC BOOL
COW_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      VDDR_LastError = 0;
      return pCOW->SourceDisk->GetDriveUUID(pCOW->SourceDisk,drvuuid);
   }
   return FALSE;
}

/*...................................................................*/

PUBLIC BOOL
COW_IsSnapshot(HVDDR pThis)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      VDDR_LastError = 0;
      return pCOW->SourceDisk->IsSnapshot(pCOW->SourceDisk);
   }
   return FALSE;
}

/*...................................................................*/

static int
RawReadSectors(PCOW pCOW, PEXTENT pe, BYTE *buffer, HUGE LBA, UINT offset_sector, UINT nSectors)
// Primitive of "ReadSectors" used when it is known that all sectors to be read
// come from the same extent.
{
   switch (pe->type) {
      case EXTENT_SRC_NONE: // read from unallocated portion of disk
         return VDDR_RSLT_NOTALLOC;
      case EXTENT_SRC_DISK: // read from underlying disk
         return pCOW->SourceDisk->ReadSectors(pCOW->SourceDisk,buffer,pe->ExtentBase+offset_sector,nSectors);
      case EXTENT_SRC_COW: { // read from COW layer (modified blocks)
         HUGE pos = pe->ExtentBase;
         if ((LBA-offset_sector)==pCOW->LBA_cache) {
            Mem_Copy(buffer,pCOW->buff+(offset_sector<<9),nSectors<<9);
            VDDR_LastError = 0;
            return VDDR_RSLT_NORMAL;
         } else {
            pos += offset_sector;
            pos <<= 9;
            VDDR_LastError = COW_ERR_SEEK;
            if (DoSeek(pCOW->fCOW,pos)==0) {
               File_RdBin(pCOW->fCOW,buffer,nSectors<<9);
               VDDR_LastError = COW_ERR_READ;
               if (File_IOresult()==0) {
                  VDDR_LastError = 0;
                  return VDDR_RSLT_NORMAL;
               }
            }
         }
         return VDDR_RSLT_FAIL;
      }
      default:
         ;
   }
   return VDDR_RSLT_FAIL;
}

/*...................................................................*/

PUBLIC int
COW_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      BYTE *pDest=buffer;
      PCOW pCOW = (PCOW)pThis;
      PEXTENT pe;
      UINT SectorsToCopy,SectorsLeft;
      HUGE offset_LBA;
      int  rslt,PageReadResult,iExtent;

      if (nSectors==0) return VDDR_RSLT_NORMAL;

      // find the extent the first LBA falls into.
      iExtent = FindExtent(pCOW,LBA,&offset_LBA);
      if (!iExtent) {
         VDDR_LastError = COW_ERR_INVBLOCK;
         return VDDR_RSLT_FAIL;
      }

      rslt = VDDR_RSLT_NOTALLOC;
      pe   = pCOW->pExtentCache + iExtent;
      SectorsLeft = pe->nSectors - ((UINT)offset_LBA); // sectors remaining in this extent

      while (nSectors) {
         SectorsToCopy = (SectorsLeft<=nSectors ? SectorsLeft : nSectors);
         nSectors -= SectorsToCopy;
         PageReadResult = RawReadSectors(pCOW, pe, pDest, LBA, (UINT)offset_LBA, SectorsToCopy);
         if (PageReadResult == VDDR_RSLT_FAIL) return VDDR_RSLT_FAIL;
         if (PageReadResult != VDDR_RSLT_NORMAL) Mem_Zero(pDest,SectorsToCopy<<9);
         if (rslt>PageReadResult) rslt = PageReadResult;
         LBA += SectorsToCopy;
         pDest += (SectorsToCopy<<9);
         offset_LBA = 0;
         iExtent = pe->NextExtent;
         if (!iExtent) break; // return with partial result when we reach EOF.
         pe = pCOW->pExtentCache + iExtent;
         SectorsLeft = pe->nSectors;
      }

      return rslt;
   }
   return VDDR_RSLT_FAIL;
}

/*...................................................................*/

PUBLIC int
COW_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift)
{
   HUGE LBA = iPage;
   LBA <<= SPBshift;
   return COW_ReadSectors(pThis,buffer,LBA,1<<SPBshift);
}

/*...................................................................*/

static EXTREF
SplitExtent(PCOW pCOW, HUGE LBA)
// Ensure that an extent begins at the given LBA, then return a reference
// to that extent.
{
   EXTREF iExtent = FindExtent(pCOW,LBA,&LBA);
   VDDR_LastError = 0;
   if (iExtent==0) VDDR_LastError = COW_ERR_INVBLOCK;
   else if (LBA>0) { // else if an extent does not already begin at the chosen LBA.
      PEXTENT pe = pCOW->pExtentCache + iExtent;
      EXTREF iExtent2 = CreateExtent(pCOW,pe->ExtentBase+(UINT)LBA,pe->nSectors-(UINT)LBA,pe->type);
      PEXTENT pe2 = pCOW->pExtentCache + iExtent2;
      pe = pCOW->pExtentCache + iExtent; // adding an extent might have invalidated the old pe ptr.
      pe2->NextExtent = pe->NextExtent;
      pe->NextExtent = iExtent2;
      pe->nSectors = (UINT)LBA;
      iExtent = iExtent2;
   }
   return iExtent;
}

/*...................................................................*/

static BOOL
FlushCowCache(PCOW pCOW)
{
   if (pCOW->bCowCacheDirty) {
      if (!pCOW->fCOW) {
         Env_GetTempFileName(NULL, "COW", 0, pCOW->fnCow);
         pCOW->fCOW = File_Create(pCOW->fnCow,DJFILE_FLAG_OVERWRITE|DJFILE_FLAG_READWRITE);
      }
      DoSeek(pCOW->fCOW,pCOW->cow_seekpos);
      File_WrBin(pCOW->fCOW,pCOW->buff,COW_BLOCK_SIZE);
      pCOW->bCowCacheDirty = FALSE;
   }
   return TRUE;
}

/*...................................................................*/

static EXTREF
CopyToCOW(PCOW pCOW, HUGE LBA, UINT nSectors)
// Copy-on-write feature: make sure this LBA is mapped to a copied block (a COW extent).
// Sectors <LBA or >= (LBA+nSectors) need to be copied from the source disk
// (intermediate sectors are about to be overwritten).
//
// This function also leaves the required COW block cached in RAM.
{
   HUGE LBA_cowstart = ((LBA>>COW_SPB_SHIFT)<<COW_SPB_SHIFT);
   HUGE LBA_cowend;
   UINT iExtent,iExtent2;
   PEXTENT pe;

   iExtent = FindExtent(pCOW,LBA,NULL);
   if (pCOW->LBA_cache==LBA_cowstart) { // already a COW block, and already resident.
      return iExtent;
   }

   // check whether the given LBA falls inside a nonresident COW block.
   if (pCOW->pExtentCache[iExtent].type==EXTENT_SRC_COW) return iExtent;
   
   // ok, from now on we know that this sector doesn't already fall inside a COW block.

   FlushCowCache(pCOW);
   pCOW->LBA_cache = (HUGE)(-1);
   if (LBA>LBA_cowstart) {
      if (COW_ReadSectors((HVDDR)pCOW, pCOW->buff, LBA_cowstart, (UINT)(LBA-LBA_cowstart))==VDDR_RSLT_FAIL) return 0;
   }
   LBA_cowend   = LBA_cowstart + COW_BLOCK_SECTORS;
   LBA += nSectors;
   if (LBA<LBA_cowend) {
      UINT trailing_sectors = (UINT)(LBA_cowend-LBA);
      if (COW_ReadSectors((HVDDR)pCOW, pCOW->buff+((COW_BLOCK_SECTORS-trailing_sectors)<<9), LBA, trailing_sectors)==VDDR_RSLT_FAIL) return 0;
   }
   pCOW->LBA_cache = LBA_cowstart;

   // ok, now that the data is safely resident we can start juggling extents
   iExtent  = SplitExtent(pCOW, LBA_cowstart);
   iExtent2 = SplitExtent(pCOW, LBA_cowend); // note that this might return NULL, indicating that COW block end coincides with end of last extent

   // The sector range from LBA_cowstart..LBA_cowend may cover more than one
   // extent. We concatenate them into a single extent if so.
   pe = pCOW->pExtentCache + iExtent;
   for (;;) {
      EXTREF NextExtent = pe->NextExtent;
      if (NextExtent==iExtent2) break;

      pe->NextExtent = pCOW->pExtentCache[NextExtent].NextExtent; // link this extent to next but one extent ptr (which may be NULL).

      // move the (former) next extent to the deleted extents list.
      pCOW->pExtentCache[NextExtent].NextExtent = pCOW->ExtentDeletedList;
      pCOW->ExtentDeletedList = NextExtent;
   }

   // So, the LBA range is now covered by a single extent. Modify this extent
   // to reference the COW file rather than the source disk.
   pe->ExtentBase = (pCOW->nCowBlocks<<COW_SPB_SHIFT);
   pe->nSectors   = COW_BLOCK_SECTORS;
   pe->type       = EXTENT_SRC_COW;
   pCOW->cow_seekpos = pe->ExtentBase;
   pCOW->cow_seekpos <<= 9;
   pCOW->nCowBlocks++;
   
   VDDR_LastError = 0;
   return iExtent;
}

/*...................................................................*/

PUBLIC int
COW_WriteSectors(HCOW pThis, void *buffer, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      HUGE LBA_cowstart;
      BYTE *pSrc=buffer;
      PCOW pCOW = (PCOW)pThis;
      UINT SectorsToCopy,SectorsLeft,offset;
      int  iExtent;

      if (pCOW->cowsig != COW_SIGNATURE) return VDDR_RSLT_FAIL;

      VDDR_LastError = 0;
      if (nSectors==0) return VDDR_RSLT_NORMAL;

      // find and map the extent the first LBA falls into.
      iExtent = CopyToCOW(pCOW,LBA,nSectors);
      if (!iExtent) return VDDR_RSLT_FAIL;

      LBA_cowstart = ((LBA>>COW_SPB_SHIFT)<<COW_SPB_SHIFT);
      SectorsLeft  = (UINT)((LBA_cowstart+COW_BLOCK_SECTORS)-LBA);
      offset       = ((COW_BLOCK_SECTORS - SectorsLeft)<<9);

      while (nSectors) {
         SectorsToCopy = (SectorsLeft<=nSectors ? SectorsLeft : nSectors);
         nSectors -= SectorsToCopy;
         if (pCOW->LBA_cache==LBA_cowstart) {
            // write to cached COW block
            Mem_Copy(pCOW->buff+offset, pSrc, SectorsToCopy<<9);
            pCOW->bCowCacheDirty = TRUE;
         } else {
            // write directly to COW block in the COW file
            PEXTENT pe = pCOW->pExtentCache + iExtent;
            HUGE seekpos = pe->ExtentBase;
            seekpos <<= 9;
            seekpos += offset;
            DoSeek(pCOW->fCOW,seekpos);
            File_WrBin(pCOW->fCOW,pSrc,SectorsToCopy<<9);
         }
         if (nSectors) {
            // find and map the next extent.
            pSrc += (SectorsToCopy<<9);
            LBA_cowstart += COW_BLOCK_SECTORS;
            offset = 0;
            iExtent = CopyToCOW(pCOW,LBA_cowstart,nSectors);
            if (!iExtent) return VDDR_RSLT_FAIL;
            SectorsLeft = COW_BLOCK_SECTORS;
         }
      }

      return VDDR_RSLT_NORMAL;
   }
   return VDDR_RSLT_FAIL;
}

/*...................................................................*/

PUBLIC int
COW_InsertSectors(HCOW pThis, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      if (pCOW->cowsig == COW_SIGNATURE) {
         EXTREF iNewExtent = CreateExtent(pCOW,0,nSectors,EXTENT_SRC_NONE); // create a new extent
         EXTREF iNextExtent = SplitExtent(pCOW,LBA); // note that this can return NULL if the split point is at EOF.
         EXTREF iExtent;

         pCOW->pExtentCache[iNewExtent].NextExtent = iNextExtent;

         // insert new extent into list.
         iExtent = pCOW->ExtentList;
         while (iExtent) {
            PEXTENT pe = pCOW->pExtentCache+iExtent;
            if (pe->NextExtent == iNextExtent) {
               pe->NextExtent = iNewExtent;
               break;
            }
            iExtent = pe->NextExtent;
         }

         pCOW->DiskSize += nSectors;
         VDDR_LastError = 0;
      }
   }
   return VDDR_LastError;
}

/*...................................................................*/

PUBLIC int
COW_MoveSectors(HCOW pThis, HUGE LBA_from, HUGE LBA_to, UINT nSectors)
{
   VDDR_LastError = COW_ERR_INVHANDLE;
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      if (pCOW->cowsig == COW_SIGNATURE) {
         // Remember that the LBA range may possibly encompass multiple extents.
         EXTREF iLeft,iRight,iInsert,iExtent;

         if (LBA_to>=LBA_from && LBA_to<=(LBA_from+nSectors)) {
            // this operation will not work if the sectors are pasted
            // into the middle of themselves!
            VDDR_LastError = COW_ERR_INVALIDMOVE;
            return COW_ERR_INVALIDMOVE;
         }
         
         iLeft   = SplitExtent(pCOW, LBA_from);
         iRight  = SplitExtent(pCOW, LBA_from+nSectors);
         iInsert = SplitExtent(pCOW, LBA_to);

         // unlink the "from" range of extents from the extent list.
         iExtent = pCOW->ExtentList;
         while (iExtent) {
            PEXTENT pe = pCOW->pExtentCache+iExtent;
            if (pe->NextExtent == iLeft) {
               pe->NextExtent = iRight;
               break;
            }
            iExtent = pe->NextExtent;
         }

         // relink the same run of extents back into the extent list in the new position.
         iExtent = iLeft;
         while (iExtent) {
            PEXTENT pe = pCOW->pExtentCache+iExtent;
            if (pe->NextExtent == iRight) {
               pe->NextExtent = iInsert;
               break;
            }
            iExtent = pe->NextExtent;
         }

         iExtent = pCOW->ExtentList;
         while (iExtent) {
            PEXTENT pe = pCOW->pExtentCache+iExtent;
            if (pe->NextExtent == iInsert) {
               pe->NextExtent = iLeft;
               break;
            }
            iExtent = pe->NextExtent;
         }

         VDDR_LastError = 0;
      }
   }
   return VDDR_LastError;
}

/*...................................................................*/

PUBLIC HVDDR
COW_Close(HVDDR pThis)
{
   VDDR_LastError = 0;
   if (pThis) {
      PCOW pCOW = (PCOW)pThis;
      if (pCOW->fCOW) {
         File_Close(pCOW->fCOW);
         File_Erase(pCOW->fnCow);
      }
      Mem_Free(pCOW->pExtentCache);
      pCOW->SourceDisk->Close(pCOW->SourceDisk);
      Mem_Free(pCOW);
   }
   return NULL;
}

/*...................................................................*/

PUBLIC HVDDR
COW_CreateCowOverlay(HVDDR SourceDisk)
{
   if (SourceDisk && SourceDisk->GetDriveType(SourceDisk)!=VDD_TYPE_COW) {
      PCOW pCOW = Mem_Alloc(MEMF_ZEROINIT, sizeof(COW_INFO));
      HUGE SourceDiskSize;

      pCOW->Base.vddr.GetDriveType       = COW_GetDriveType;
      pCOW->Base.vddr.GetDriveSize       = COW_GetDriveSize;
      pCOW->Base.vddr.GetDriveBlockCount = COW_GetDriveBlockCount;
      pCOW->Base.vddr.BlockStatus        = COW_BlockStatus;
      pCOW->Base.vddr.GetDriveUUID       = COW_GetDriveUUID;
      pCOW->Base.vddr.IsSnapshot         = COW_IsSnapshot;
      pCOW->Base.vddr.ReadPage           = COW_ReadPage;
      pCOW->Base.vddr.ReadSectors        = COW_ReadSectors;
      pCOW->Base.vddr.Close              = COW_Close;

      pCOW->Base.InsertSectors = COW_InsertSectors;
      pCOW->Base.MoveSectors   = COW_MoveSectors;
      pCOW->Base.WriteSectors  = COW_WriteSectors;
         
      pCOW->cowsig     = COW_SIGNATURE;
      pCOW->SourceDisk = SourceDisk;
      pCOW->nExtents   = 1; // make sure that extent[0] is never used, because we treat index 0 as NULL.
      pCOW->LBA_cache  = (HUGE)(-1);

      SourceDisk->GetDriveSize(SourceDisk, &SourceDiskSize);
      SourceDiskSize >>= 9; // convert bytes to sectors
      pCOW->ExtentList = CreateExtent(pCOW,0,(UINT)SourceDiskSize,EXTENT_SRC_DISK);
      pCOW->DiskSize = SourceDiskSize;

      return (HVDDR)pCOW;
   }
   return NULL;
}

/*...................................................................*/

/* end of cow.c */

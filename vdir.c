/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "vdir.h"
#include "vdistructs.h"
#include "mem.h"
#include "djfile.h"
#include "env.h"
#include "ids.h"
#include "djstring.h"

PUBLIC PSTR pszVdiInfo         = "<<< Sun VirtualBox Disk Image >>>\n";
PUBLIC PSTR pszVdiInfoAlt      = "<<< Sun xVM VirtualBox Disk Image >>>\n";
PUBLIC PSTR pszVdiInfoInno     = "<<< innotek VirtualBox Disk Image >>>\n";
PUBLIC PSTR pszVdiInfoSlimVDI = "<<< SlimVDI VirtualBox Disk Image >>>\n";
PUBLIC PSTR pszQemuVdiInfo     = "<<< QEMU VM Virtual Disk Image >>>\n";
PUBLIC PSTR pszVdiInfoOracle   = "<<< Oracle VM VirtualBox Disk Image >>>\n";

static UINT OSLastError;

typedef struct {
   CLASS(VDDR) Base;
   VDI_PREHEADER phdr;
   VDI_HEADER hdr;
   HVDDR hVDIparent;
   FILE f;
   UINT BlockShift;
   UINT SectorsPerBlock,SPBshift;
   int  PageReadResult;
   UINT *blockmap;
} VDI_INFO, *PVDI;

static PSTR pszOK          /* = "Ok" */;
static PSTR pszUNKERROR    /* = "Unknown Error" */;
static PSTR pszVDNOEXIST   /* = "The source file does not exist" */;
static PSTR pszVDERRSHARE  /* = "Source file already in use (is VirtualBox running?)" */;
static PSTR pszVDERRREAD   /* = "Got OS error %lu when reading from source file" */;
static PSTR pszVDNOTVDI    /* = "Source file is not a recognized VDI file format" */;
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
VDIR_GetLastError(void)
{
   return VDDR_LastError;
}

/*.....................................................*/

PUBLIC PSTR
VDIR_GetErrorString(UINT nErr)
{
   PSTR pszErr;
   static char sz[256];
   if (nErr==0xFFFFFFFF) nErr = VDDR_LastError;
   switch (nErr) {
      case VDIR_ERR_NONE:
         pszErr=RSTR(OK);
         break;
      case VDIR_ERR_FILENOTFOUND:
         pszErr=RSTR(VDNOEXIST);
         break;
      case VDIR_ERR_SHARE:
         pszErr=RSTR(VDERRSHARE);
         break;
      case VDIR_ERR_READ:
         Env_sprintf(sz,RSTR(VDERRREAD),OSLastError);
         pszErr=sz;
         break;
      case VDIR_ERR_NOTVDI:
         pszErr=RSTR(VDNOTVDI);
         break;
      case VDIR_ERR_INVHANDLE:
         pszErr=RSTR(VDINVHANDLE);
         break;
      case VDIR_ERR_OUTOFMEM:
         pszErr=RSTR(VDNOMEM);
         break;
      case VDIR_ERR_INVBLOCK:
         pszErr=RSTR(VDSEEKRANGE);
         break;
      case VDIR_ERR_BADFORMAT:
         pszErr=RSTR(VDERRFMT);
         break;
      case VDIR_ERR_SEEK:
         Env_sprintf(sz,RSTR(VDERRSEEK),OSLastError);
         pszErr=sz;
         break;
      case VDIR_ERR_BLOCKMAP:
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
ReadOldHeader(PVDI pVDI)
{
   VDI_OLD_HEADER hdr;
   VDDR_LastError = VDIR_ERR_READ;
   if (File_RdBin(pVDI->f, &hdr, sizeof(hdr)) == sizeof(hdr)) {
      VDDR_LastError = 0;
      pVDI->hdr.vdi_type    = hdr.vdi_type;
      pVDI->hdr.vdi_flags   = hdr.vdi_flags;
      Mem_Copy(pVDI->hdr.vdi_comment,hdr.vdi_comment,256);
      Mem_Copy(&pVDI->hdr.LegacyGeometry, &hdr.LegacyGeometry, sizeof(VDIDISKGEOMETRY));
      pVDI->hdr.DiskSize    = hdr.DiskSize;
      pVDI->hdr.BlockSize   = hdr.BlockSize;
      pVDI->hdr.nBlocks     = hdr.nBlocks;
      pVDI->hdr.nBlocksAllocated = hdr.nBlocksAllocated;
      Mem_Copy(&pVDI->hdr.uuidCreate, &hdr.uuidCreate, 16);
      Mem_Copy(&pVDI->hdr.uuidModify, &hdr.uuidModify, 16);
      Mem_Copy(&pVDI->hdr.uuidLinkage, &hdr.uuidLinkage, 16);

      Mem_Copy(&pVDI->hdr.uuidParentModify, &hdr.uuidLinkage, 16);
       // not sure if this is a reasonable way to supply the missing uuidParentModify field. But since this only
       // affects pre-v1.6 VDIs which used snapshots, it probably won't ever matter.

      pVDI->hdr.offset_Blocks = sizeof(VDI_PREHEADER)+sizeof(hdr);    // not present in old header.
      pVDI->hdr.offset_Image  = pVDI->hdr.offset_Blocks + hdr.nBlocks * sizeof(UINT);
   }
   return (VDDR_LastError == 0);
}

/*.....................................................*/

static BOOL
ReadNewHeader(PVDI pVDI)
{
   UINT cbSize = pVDI->hdr.cbSize;
   if (cbSize>sizeof(VDI_HEADER)) cbSize = sizeof(VDI_HEADER);
   VDDR_LastError = VDIR_ERR_READ;
   cbSize -= sizeof(UINT); // cbSize field itself has already been read.
   if (File_RdBin(pVDI->f, &pVDI->hdr.vdi_type, cbSize) == cbSize) {
      VDDR_LastError = 0;
   }
   return (VDDR_LastError == 0);
}

/*.....................................................*/

static BOOL
PreHeaderOK(VDI_PREHEADER *vph)
{
   if ((String_Compare(vph->szFileInfo,pszVdiInfo)==0 || String_Compare(vph->szFileInfo,pszVdiInfoAlt)==0 ||
        String_Compare(vph->szFileInfo,pszVdiInfoInno)==0 || String_Compare(vph->szFileInfo,pszVdiInfoSlimVDI)==0 ||
        String_Compare(vph->szFileInfo,pszQemuVdiInfo)==0 || String_Compare(vph->szFileInfo,pszVdiInfoOracle)==0 ) &&
       vph->u32Signature==VDI_SIGNATURE && (vph->u32Version==VDI_VERSION_1_0 || vph->u32Version==VDI_VERSION_1_1)) return TRUE;
   VDDR_LastError = VDIR_ERR_NOTVDI;
   return FALSE;
}

/*.....................................................*/

static BOOL
ValidateMap(PVDI pVDI)
{
   UINT i,nAlloc,nFree,nZero;
   UINT sid;

   nAlloc = nFree = nZero = 0;
   for (i=0; i<pVDI->hdr.nBlocks; i++) {
      sid = pVDI->blockmap[i];
      if (sid == VDI_PAGE_FREE) nFree++;
      else if (sid == VDI_PAGE_ZERO) nZero++;
      else if (sid<pVDI->hdr.nBlocksAllocated) nAlloc++;
      else return FALSE; // SID out of range.
   }
   i = nAlloc+nFree+nZero;
   return (nAlloc==pVDI->hdr.nBlocksAllocated && i==pVDI->hdr.nBlocks);
}

/*.....................................................*/

PUBLIC HVDDR
VDIR_Open(CPFN fn, UINT iChain)
{
   PVDI pVDI = NULL;
   FILE f = File_OpenRead(fn);
   if (f==NULLFILE) {
      if (File_IOresult()==DJFILE_ERROR_SHARE) VDDR_LastError = VDIR_ERR_SHARE;
      else VDDR_LastError = VDIR_ERR_FILENOTFOUND;
   } else {
      VDI_PREHEADER vph;
      VDDR_LastError = VDIR_ERR_READ;
      if (File_RdBin(f,&vph,sizeof(vph))==sizeof(vph)) {
         if (PreHeaderOK(&vph)) {
            UINT cbSize;
            BOOL bOK = TRUE;
            VDDR_LastError = VDIR_ERR_READ;
            if (vph.u32Version==VDI_VERSION_1_0) cbSize = sizeof(VDI_OLD_HEADER);
            else bOK = (File_RdBin(f,&cbSize,4)==4);
            if (bOK) {
               VDDR_LastError = VDIR_ERR_NOTVDI;
               if (vph.u32Version!=VDI_VERSION_1_0) bOK = (cbSize >= sizeof(VDI_HEADER) || (cbSize==sizeof(VDI_HEADER)-sizeof(VDIDISKGEOMETRY)));
               if (bOK) {
                  pVDI = Mem_Alloc(MEMF_ZEROINIT,sizeof(VDI_INFO));
                  pVDI->f = f;
                  pVDI->hdr.cbSize = cbSize;

                  pVDI->Base.GetDriveType       = VDIR_GetDriveType;
                  pVDI->Base.GetDriveSize       = VDIR_GetDriveSize;
                  pVDI->Base.GetDriveBlockCount = VDIR_GetDriveBlockCount;
                  pVDI->Base.BlockStatus        = VDIR_BlockStatus;
                  pVDI->Base.GetDriveUUID       = VDIR_GetDriveUUID;
                  pVDI->Base.GetDriveUUIDs      = VDIR_GetDriveUUIDs;
                  pVDI->Base.GetParentUUIDs     = VDIR_GetParentUUIDs;
                  pVDI->Base.IsSnapshot         = VDIR_IsSnapshot;
                  pVDI->Base.ReadPage           = VDIR_ReadPage;
                  pVDI->Base.ReadSectors        = VDIR_ReadSectors;
                  pVDI->Base.Close              = VDIR_Close;
                  pVDI->Base.IsInheritedPage    = VDIR_IsInheritedPage;

                  Mem_Copy(&pVDI->phdr, &vph, sizeof(vph));

                  // The ReadHeader routines also set VDDR_LastError.
                  if (vph.u32Version==VDI_VERSION_1_0) ReadOldHeader(pVDI);
                  else ReadNewHeader(pVDI);

                  if (VDDR_LastError==0) {
                     pVDI->BlockShift = PowerOfTwo(pVDI->hdr.BlockSize); // BlockShift will be 0xFFFFFFFF if BlockSize==0
                     pVDI->SectorsPerBlock = (pVDI->hdr.BlockSize>>9);   // This will be 0 if BlockSize<512, but ...
                     pVDI->SPBshift = PowerOfTwo(pVDI->SectorsPerBlock); // ... this returns 0xFFFFFFFF if SectorsPerBlock==0
                     if (pVDI->BlockShift==0xFFFFFFFF || pVDI->SPBshift==0xFFFFFFFF) {
                        VDDR_LastError = VDIR_ERR_BADFORMAT;
                     }
                  }

                  if (VDDR_LastError==0 && VDIR_IsSnapshot((HVDDR)pVDI)) {
                     pVDI->hVDIparent = VDDR_OpenByUUID(&pVDI->hdr.uuidLinkage,iChain+1); // may set VDDR_LastError
                     if (iChain==0) VDDR_LastError = 0; // we let the user open the first in a broken snapshot chain for diagnostic purposes, though we don't let him clone it.
                  }

                  if (!VDDR_LastError) {
                     // read the blocks map into a buffer. Assuming 1MB blocks there are 1024 blocks per GB,
                     // 1048576 blocks per TB. Therefore the complete map for a 1TB drive would require only
                     // 4MB for the map. It seems reasonable therefore to read the whole thing at once.
                     //
                     UINT mapsize = pVDI->hdr.nBlocks*sizeof(UINT);
                     pVDI->blockmap = Mem_Alloc(0, mapsize);
                     VDDR_LastError = VDIR_ERR_OUTOFMEM;
                     if (pVDI->blockmap) {
                        VDDR_LastError = VDIR_ERR_SEEK;
                        File_Seek(pVDI->f, pVDI->hdr.offset_Blocks);
                        OSLastError = File_IOresult();
                        if (OSLastError==0) {
                           VDDR_LastError = VDIR_ERR_READ;
                           if (File_RdBin(pVDI->f, pVDI->blockmap, mapsize)==mapsize) {
                              VDDR_LastError = VDIR_ERR_BLOCKMAP;
                              if (ValidateMap(pVDI)) VDDR_LastError = 0;
                           } else {
                              OSLastError = File_IOresult();
                           }
                        }
                     }
                  }
                  if (VDDR_LastError) pVDI = Mem_Free(pVDI);
               }
            }
         }
      }
      if (!pVDI) File_Close(f);
   }
   return (HVDDR)pVDI;
}

/*.....................................................*/

PUBLIC BOOL
VDIR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      if (pVDI->hVDIparent) return pVDI->hVDIparent->GetDriveUUID(pVDI->hVDIparent, drvuuid);
      VDDR_LastError = 0;
      Mem_Copy(drvuuid, &pVDI->hdr.uuidCreate, sizeof(S_UUID));
      return TRUE;
   }
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

PUBLIC BOOL
VDIR_GetDriveUUIDs(HVDDR pThis, S_UUID *uuid, S_UUID *modifyUUID)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      VDDR_LastError = 0;
      Mem_Copy(uuid, &pVDI->hdr.uuidCreate, sizeof(S_UUID));
      Mem_Copy(modifyUUID, &pVDI->hdr.uuidModify, sizeof(S_UUID));
      return TRUE;
   }
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   return 0;
}

PUBLIC BOOL
VDIR_GetParentUUIDs(HVDDR pThis, S_UUID *parentUUID, S_UUID *parentModifyUUID)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      VDDR_LastError = 0;
      Mem_Copy(parentUUID, &pVDI->hdr.uuidLinkage, sizeof(S_UUID));
      Mem_Copy(parentModifyUUID, &pVDI->hdr.uuidParentModify, sizeof(S_UUID));
      return TRUE;
   }
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

PUBLIC BOOL
VDIR_GetHeader(HVDDR pThis, VDI_PREHEADER *vdiph, VDI_HEADER *vdih)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      VDDR_LastError = 0;
      if (vdiph) Mem_Copy(vdiph, &pVDI->phdr, sizeof(VDI_PREHEADER));
      if (vdih) Mem_Copy(vdih, &pVDI->hdr, sizeof(VDI_HEADER));
      return TRUE;
   }
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC int
RawReadPage(PVDI pVDI, void *buffer, UINT iPage, UINT SectorOffset, UINT length)
// Read a block/page using the native block size. This is an internal
// function, so there's very little parameter checking. The calling
// app must ensure that length<(native blocksize) and SectorOffset+length
// does not go beyond the length of a block. Length is a size in bytes,
// but is expected to be a multiple of the sector size.
{
   VDDR_LastError = VDIR_ERR_INVBLOCK;
   if (iPage<pVDI->hdr.nBlocks) {
      UINT SID  = pVDI->blockmap[iPage];
      VDDR_LastError = 0;
      if (SID == VDI_PAGE_FREE) {
         if (pVDI->hVDIparent) { // reading from unallocated page in snapshot child: pass read request to parent VDI.
            HUGE LBA = iPage;
            return pVDI->hVDIparent->ReadSectors(pVDI->hVDIparent,buffer,(LBA<<pVDI->SPBshift)+SectorOffset,length>>9);
         }
         return VDDR_RSLT_NOTALLOC;
      } else if (SID == VDI_PAGE_ZERO) {
         return VDDR_RSLT_BLANKPAGE;
      } else if (SID < pVDI->hdr.nBlocksAllocated) {
         HUGE seekpos;

         seekpos = (((HUGE)SID)<<pVDI->BlockShift);
         seekpos += pVDI->hdr.offset_Image; // ... and add image base address.
         seekpos += (SectorOffset<<9);

         VDDR_LastError = VDIR_ERR_SEEK;
         OSLastError = DoSeek(pVDI->f,seekpos);
         if (OSLastError==0) {
            VDDR_LastError = VDIR_ERR_READ;
            if (File_RdBin(pVDI->f, buffer, length)==length) {
               VDDR_LastError = 0;
               return VDDR_RSLT_NORMAL;
            }
            OSLastError = File_IOresult();
         }
      } else {
         VDDR_LastError = VDIR_ERR_BLOCKMAP;
      }
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
VDIR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   if (pThis) {
      BYTE *pDest=buffer;
      PVDI pVDI = (PVDI)pThis;
      UINT SectorsToCopy,SectorsLeft,SectorOffset,BytesToCopy,iPage;
      HUGE sid;
      int  rslt;

      if (nSectors==0) return VDDR_RSLT_NORMAL;

      hugeop_shr(sid,LBA,pVDI->SPBshift); // LBA/SectorsPerBlock leaves iPage number in sid.

      iPage  = LO32(sid);
      SectorOffset = (UINT)(LBA & (pVDI->SectorsPerBlock-1));
      rslt   = VDDR_RSLT_NOTALLOC;

      SectorsLeft = pVDI->SectorsPerBlock - SectorOffset;

      while (nSectors) {
         SectorsToCopy = (SectorsLeft<=nSectors ? SectorsLeft : nSectors);
         nSectors -= SectorsToCopy;
         BytesToCopy = (SectorsToCopy<<9);
         pVDI->PageReadResult = RawReadPage(pVDI, pDest, iPage, SectorOffset, BytesToCopy);
         if (pVDI->PageReadResult == VDDR_RSLT_FAIL) return VDDR_RSLT_FAIL;
         if (pVDI->PageReadResult != VDDR_RSLT_NORMAL) Mem_Zero(pDest,BytesToCopy);
         if (rslt>pVDI->PageReadResult) rslt = pVDI->PageReadResult;
         pDest += BytesToCopy;
         SectorOffset = 0;
         iPage++;
         if (iPage==pVDI->hdr.nBlocks) return rslt; // return with partial result when we reach EOF.
         SectorsLeft = pVDI->SectorsPerBlock;
      }
      return rslt;
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
VDIR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift)
{
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      if (SPBshift==pVDI->SPBshift) return RawReadPage(pVDI,buffer,iPage, 0, pVDI->hdr.BlockSize);
      else {
         // Convert the page number into a disk LBA (not the same as a file LBA because dynamic VHD has an extra sector per block).
         HUGE LBA;
         hugeop_fromuint(LBA,iPage);
         hugeop_shl(LBA,LBA,SPBshift);
         return VDIR_ReadSectors(pThis, buffer, LBA, 1<<SPBshift);
      }
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC HVDDR
VDIR_Close(HVDDR pThis)
{
   VDDR_LastError = 0;
   if (pThis) { // we silently handle closing of an already closed file.
      PVDI pVDI = (PVDI)pThis;
      File_Close(pVDI->f);
      Mem_Free(pVDI->blockmap);
      Mem_Free(pVDI);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC UINT
VDIR_GetDriveType(HVDDR pThis)
{
   return VDD_TYPE_VDI;
}

/*.....................................................*/

static BOOL
IsNullUUID(S_UUID *uuid)
{
   int i;
   for (i=0; i<4; i++) {
      if (uuid->au32[i]) return FALSE;
   }
   return TRUE;
}

/*.....................................................*/

PUBLIC BOOL
VDIR_IsSnapshot(HVDDR pThis)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      VDDR_LastError = 0;
      if (pVDI->hVDIparent) return FALSE; // if we've already resolved the snapshot then don't alarm the app...
      if (IsNullUUID(&pVDI->hdr.uuidParentModify)) return FALSE;
      if (IsNullUUID(&pVDI->hdr.uuidLinkage)) return FALSE;
   }
   return TRUE;
}

/*....................................................*/

PUBLIC BOOL
VDIR_GetDriveSize(HVDDR pThis, HUGE *drive_size)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      *drive_size = pVDI->hdr.DiskSize;
      VDDR_LastError = 0;
      return TRUE;
   }
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC UINT
VDIR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift)
{
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      HUGE nBlocks = pVDI->hdr.nBlocks;
      if (SPBshift != pVDI->SPBshift) {
         nBlocks <<= pVDI->SPBshift;
         nBlocks += ((1<<SPBshift)-1);
         nBlocks >>= SPBshift;
      }
      VDDR_LastError = 0;
      return LO32(nBlocks);
   }
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

static UINT
BlockStatus(PVDI pVDI, HUGE LBA_start, HUGE LBA_end)
{
   UINT SID,LastSID,blks,rslt=VDDR_RSLT_NOTALLOC;
   SID = LO32(LBA_start>>pVDI->SPBshift);   // convert start LBA into block number.
   LastSID = LO32(LBA_end>>pVDI->SPBshift); // convert end LBA into block number.
   for (; SID<=LastSID; SID++) {
      if (SID>=pVDI->hdr.nBlocks) break;
      blks = pVDI->blockmap[SID];
      if (blks==VDI_PAGE_FREE && pVDI->hVDIparent) {
         blks = pVDI->hVDIparent->BlockStatus(pVDI->hVDIparent,LBA_start,LBA_end);
         if (blks==VDDR_RSLT_NORMAL) return blks;
         if (blks==VDDR_RSLT_BLANKPAGE) rslt = blks;
      } else {
         if (VDI_BLOCK_ALLOCATED(blks)) return VDDR_RSLT_NORMAL;
         if (blks==VDI_PAGE_ZERO) rslt = VDDR_RSLT_BLANKPAGE;
      }
   }
   return rslt;
}

/*.....................................................*/

PUBLIC UINT
VDIR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end)
{
   UINT rslt = VDDR_RSLT_NOTALLOC;
   VDDR_LastError = VDIR_ERR_INVHANDLE;
   if (pThis) {
      PVDI pVDI = (PVDI)pThis;
      VDDR_LastError = 0;
      rslt = BlockStatus(pVDI,LBA_start,LBA_end);
   }
   return rslt;
}

/*....................................................*/

PUBLIC BOOL
VDIR_QuickGetUUID(CPFN fn, S_UUID *UUID)
{
   FILE f = File_OpenRead(fn);
   if (f==NULLFILE) {
      if (File_IOresult()==DJFILE_ERROR_SHARE) VDDR_LastError = VDIR_ERR_SHARE;
      else VDDR_LastError = VDIR_ERR_FILENOTFOUND;
   } else {
      VDI_PREHEADER vph;
      VDDR_LastError = VDIR_ERR_READ;
      if (File_RdBin(f,&vph,sizeof(vph))==sizeof(vph)) {
         if (PreHeaderOK(&vph)) {
            UINT cbSize;
            BOOL bOK = TRUE;
            VDDR_LastError = VDIR_ERR_READ;
            if (vph.u32Version==VDI_VERSION_1_0) cbSize = sizeof(VDI_OLD_HEADER);
            else bOK = (File_RdBin(f,&cbSize,4)==4);
            if (bOK) {
               VDDR_LastError = VDIR_ERR_NOTVDI;
               if (vph.u32Version!=VDI_VERSION_1_0) bOK = (cbSize >= sizeof(VDI_HEADER) || (cbSize==sizeof(VDI_HEADER)-sizeof(VDIDISKGEOMETRY)));
               if (bOK) {
                  if (vph.u32Version==VDI_VERSION_1_0) {
                     VDI_OLD_HEADER hdr;
                     VDDR_LastError = VDIR_ERR_READ;
                     if (File_RdBin(f, &hdr, sizeof(hdr)) == sizeof(hdr)) {
                        VDDR_LastError = 0;
                        Mem_Copy(UUID, &hdr.uuidCreate, 16);
                     }
                  } else {
                     VDI_HEADER hdr;
                     if (cbSize>sizeof(VDI_HEADER)) cbSize = sizeof(VDI_HEADER);
                     VDDR_LastError = VDIR_ERR_READ;
                     cbSize -= sizeof(UINT); // cbSize field itself has already been read.
                     if (File_RdBin(f, &hdr.vdi_type, cbSize) == cbSize) {
                        Mem_Copy(UUID, &hdr.uuidCreate, 16);
                        VDDR_LastError = 0;
                     }
                  }
               }
            }
         }
      }
      File_Close(f);
   }
   return (VDDR_LastError==0);
}

/*....................................................*/

PUBLIC BOOL
VDIR_IsInheritedPage(HVDDR pThis, UINT iPage)
{
   PVDI pVDI = (PVDI)pThis;
   return (pVDI->hVDIparent && pVDI->blockmap[iPage] == VDI_PAGE_FREE);
}

/*....................................................*/

/* end of module vdir.c */


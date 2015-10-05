/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "vhdr.h"
#include "vhdstructs.h"
#include "mem.h"
#include "djstring.h"
#include "djfile.h"
#include "env.h"
#include "ids.h"

// error codes
#define VHDR_ERR_NONE         0
#define VHDR_ERR_FILENOTFOUND ((VDD_TYPE_VHD<<16)+1)
#define VHDR_ERR_READ         ((VDD_TYPE_VHD<<16)+2)
#define VHDR_ERR_NOTVHD       ((VDD_TYPE_VHD<<16)+3)
#define VHDR_ERR_INVHANDLE    ((VDD_TYPE_VHD<<16)+4)
#define VHDR_ERR_OUTOFMEM     ((VDD_TYPE_VHD<<16)+5)
#define VHDR_ERR_INVBLOCK     ((VDD_TYPE_VHD<<16)+6)
#define VHDR_ERR_BADFORMAT    ((VDD_TYPE_VHD<<16)+7)
#define VHDR_ERR_SEEK         ((VDD_TYPE_VHD<<16)+8)
#define VHDR_ERR_BLOCKMAP     ((VDD_TYPE_VHD<<16)+9)
#define VHDR_ERR_SHARE        ((VDD_TYPE_VHD<<16)+10)

static UINT OSLastError;

typedef struct {
   CLASS(VDDR) Base;
   VHD_FOOTER ftr;
   VHD_DYN_HEADER hdr;
   HVDDR hVDIparent;
   FILE f;
   UINT nBlocks;
   UINT nBlocksAllocated;
   UINT BlockSize;
   UINT BlockShift;
   UINT SectorsPerBlock,SPBshift;
   UINT SectorsPerBitmap;
   int  PageReadResult;
   UINT *blockmap;
} VHD_INFO, *PVHD;

// localization strings
static PSTR pszOK          /* = "Ok" */;
static PSTR pszUNKERROR    /* = "Unknown Error" */;
static PSTR pszVDNOEXIST   /* = "The source file does not exist" */;
static PSTR pszVDERRSHARE  /* = "Source file already in use (is VirtualBox running?)" */;
static PSTR pszVDERRREAD   /* = "Got OS error %lu when reading from source file" */;
static PSTR pszVDNOTVHD    /* = "Source file is not a recognized VHD file format" */;
static PSTR pszVDINVHANDLE /* = "Invalid handle passed to VDx source object" */;
static PSTR pszVDNOMEM     /* = "Not enough memory to map source file" */;
static PSTR pszVDSEEKRANGE /* = "App attempted to seek beyond end of drive!" */;
static PSTR pszVDERRFMT    /* = "Source has strange format which is incompatible with this tool" */;
static PSTR pszVDERRSEEK   /* = "Got OS error %lu when seeking inside source file" */;
static PSTR pszVDBLKMAP    /* = "Source file corrupt - block map contains errors" */;

/*.....................................................*/

static _inline WORD
wSwap(WORD x)
{
   return (WORD)((x>>8)|(x<<8));
}

/*.....................................................*/

static _inline UINT
dwSwap(UINT x)
{
   return ((UINT)wSwap((WORD)(x>>16))) + (((UINT)wSwap((WORD)x))<<16);
}

/*.....................................................*/

static HUGE
hSwap(HUGE x)
{
   UINT LowPart,HighPart;
   LowPart  = dwSwap(HI32(x));
   HighPart = dwSwap(LO32(x));
   return MAKEHUGE(LowPart,HighPart);
}

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
VHDR_GetLastError(void)
{
   return VDDR_LastError;
}

/*.....................................................*/

PUBLIC PSTR
VHDR_GetErrorString(UINT nErr)
{
   PSTR pszErr;
   static char sz[256];
   if (nErr==0xFFFFFFFF) nErr = VDDR_LastError;
   if (nErr==VHDR_ERR_NONE) pszErr=RSTR(OK);
   else {
      switch (nErr) {
         case VHDR_ERR_FILENOTFOUND:
            pszErr=RSTR(VDNOEXIST);
            break;
         case VHDR_ERR_SHARE:
            pszErr=RSTR(VDERRSHARE);
            break;
         case VHDR_ERR_READ:
            Env_sprintf(sz,RSTR(VDERRREAD),OSLastError);
            pszErr=sz;
            break;
         case VHDR_ERR_NOTVHD:
            pszErr=RSTR(VDNOTVHD);
            break;
         case VHDR_ERR_INVHANDLE:
            pszErr=RSTR(VDINVHANDLE);
            break;
         case VHDR_ERR_OUTOFMEM:
            pszErr=RSTR(VDNOMEM);
            break;
         case VHDR_ERR_INVBLOCK:
            pszErr=RSTR(VDSEEKRANGE);
            break;
         case VHDR_ERR_BADFORMAT:
            pszErr=RSTR(VDERRFMT);
            break;
         case VHDR_ERR_SEEK:
            Env_sprintf(sz,RSTR(VDERRSEEK),OSLastError);
            pszErr=sz;
            break;
         case VHDR_ERR_BLOCKMAP:
            pszErr = RSTR(VDBLKMAP);
            break;
         default:
            pszErr = RSTR(UNKERROR);
      }
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

static UINT
Checksum(BYTE *pb, UINT nb)
{
   UINT cs = 0;
   for (; nb; nb--) cs += *pb++;
   return (~cs);
}

/*.....................................................*/

static BOOL
HasFooterSig(VHD_FOOTER *ftr)
{
   char sz[16];
   Mem_Copy(sz,ftr->cookie,8);
   sz[8] = (char)0;
   return (String_Compare(sz,"conectix")==0);
}

/*.....................................................*/

static BOOL
ReadFooter(FILE f, VHD_FOOTER *ftr)
// NOTE: In fixed size VHDs the footer really is a footer which occurs at the end of the file. In
// fact in a fixed VHD this is the only structure apart from the image data itself, which is
// a raw image (VHD size is exactly the virtual image size plus the footer size).
{
   BOOL bMustBeFixed = FALSE;
   UINT u32Checksum;

   VDDR_LastError = VHDR_ERR_READ;
   // try reading the "footer" from the beginning of the file first.
   if (File_RdBin(f, ftr, sizeof(VHD_FOOTER)) != sizeof(VHD_FOOTER)) return FALSE;
   if (!HasFooterSig(ftr)) {
      /* ok, not at the beginning. Try the last sector */
      HUGE fsize;
      bMustBeFixed = TRUE;
      File_Size(f,&fsize);
      VDDR_LastError = VHDR_ERR_SEEK;
      File_Seek(f, fsize-512);
      if (File_IOresult()) return FALSE;
      VDDR_LastError = VHDR_ERR_READ;
      if (File_RdBin(f, ftr, sizeof(VHD_FOOTER)) != sizeof(VHD_FOOTER)) return FALSE;
      if (!HasFooterSig(ftr)) {
         /* ok, not the last sector either. Really old VHDs had 511 byte footers - so try that instead. */
         Mem_Move(ftr, ((PSTR)ftr)+1, sizeof(VHD_FOOTER)-1);
         if (!HasFooterSig(ftr)) {
            /* nope, I don't recognize this alleged VHD */
            VDDR_LastError = VHDR_ERR_NOTVHD;
            return FALSE;
         }
      }
   }
   
   u32Checksum = dwSwap(ftr->u32Checksum);
   ftr->u32Checksum     = 0;
   VDDR_LastError = VHDR_ERR_NOTVHD;
   if (Checksum((BYTE*)ftr, sizeof(VHD_FOOTER))==u32Checksum) {
      ftr->u32Checksum     = u32Checksum;
      ftr->u32Features     = dwSwap(ftr->u32Features);
      ftr->u32FmtVer       = dwSwap(ftr->u32FmtVer);
      ftr->u64DataOffset   = hSwap(ftr->u64DataOffset);
      ftr->u32TimeStamp    = dwSwap(ftr->u32TimeStamp);
      ftr->u32CreatorVer   = dwSwap(ftr->u32CreatorVer);
      ftr->u64OriginalSize = hSwap(ftr->u64OriginalSize);
      ftr->u64CurrentSize  = hSwap(ftr->u64CurrentSize);
      ftr->geometry.u16Cylinders = wSwap(ftr->geometry.u16Cylinders);
      ftr->u32DiskType     = dwSwap(ftr->u32DiskType);
//    ftr->uuidCreate      = ?; // not sure yet if I need to byte swap the uuid.
      VDDR_LastError = 0;
      if ((bMustBeFixed && ftr->u32DiskType!=VHD_TYPE_FIXED) ||
          (!bMustBeFixed && (ftr->u32DiskType!=VHD_TYPE_DYNAMIC && ftr->u32DiskType!=VHD_TYPE_DIFFERENCING) ) ||
          ((ftr->u32FmtVer>>16)!=VHD_FORMAT_MAJOR_VER)) {
             VDDR_LastError = VHDR_ERR_BADFORMAT;
      }
   }

   return (VDDR_LastError == 0);
}

/*.....................................................*/

static BOOL
ReadDynHeader(PVHD pVHD)
{
   VDDR_LastError = VHDR_ERR_READ;
   if (File_RdBin(pVHD->f, &pVHD->hdr, sizeof(VHD_DYN_HEADER)) == sizeof(VHD_DYN_HEADER)) {
      char sz[16];
      Mem_Copy(sz,pVHD->hdr.cookie,8);
      sz[8] = (char)0;
      VDDR_LastError = VHDR_ERR_NOTVHD;
      if (String_Compare(sz,"cxsparse")==0) {
         UINT u32Checksum = dwSwap(pVHD->hdr.u32Checksum);
         pVHD->hdr.u32Checksum     = 0;
         if (Checksum((BYTE*)&pVHD->hdr, sizeof(VHD_DYN_HEADER))==u32Checksum) {
            pVHD->hdr.u32Checksum     = u32Checksum;
            pVHD->hdr.u64DataOffset   = hSwap(pVHD->hdr.u64DataOffset);
            pVHD->hdr.u64TableOffset  = hSwap(pVHD->hdr.u64TableOffset);
            pVHD->hdr.u32HeaderVer    = dwSwap(pVHD->hdr.u32HeaderVer);
            pVHD->hdr.u32BlockCount   = dwSwap(pVHD->hdr.u32BlockCount);
            pVHD->hdr.u32BlockSize    = dwSwap(pVHD->hdr.u32BlockSize);
            pVHD->hdr.u32ParentTimeStamp = dwSwap(pVHD->hdr.u32ParentTimeStamp);
            pVHD->hdr.u32Reserved     = dwSwap(pVHD->hdr.u32Reserved);
            VDDR_LastError = VHDR_ERR_BADFORMAT;
            if ((pVHD->hdr.u32HeaderVer>>16)==VHD_FORMAT_MAJOR_VER) VDDR_LastError = 0;
         }
      }
   }
   return (VDDR_LastError == 0);
}

/*.....................................................*/

static BOOL
ValidateMap(PVHD pVHD)
{
   UINT i,nAlloc,nFree,sid,nSectors;
   HUGE fsize;

   File_Size(pVHD->f,&fsize);
   fsize>>=9;
   nSectors = LO32(fsize);

   nAlloc = nFree = 0;
   for (i=0; i<pVHD->hdr.u32BlockCount; i++) {
      sid = dwSwap(pVHD->blockmap[i]);
      pVHD->blockmap[i] = sid;
      if (sid == VHD_PAGE_FREE) nFree++;
      else if (sid<nSectors) nAlloc++;
      else return FALSE; // SID out of range.
   }
   pVHD->nBlocksAllocated = nAlloc;
   return ((nAlloc+nFree) == pVHD->hdr.u32BlockCount); // this doesn't really do anything useful. More to come.
}

/*.....................................................*/

PUBLIC HVDDR
VHDR_Open(CPFN fn, UINT iChain)
{
   PVHD pVHD = NULL;
   FILE f = File_OpenRead(fn);
   if (f==NULLFILE) {
      if (File_IOresult()==DJFILE_ERROR_SHARE) VDDR_LastError = VHDR_ERR_SHARE;
      else VDDR_LastError = VHDR_ERR_FILENOTFOUND;
   } else {
      VHD_FOOTER ftr;
      if (ReadFooter(f,&ftr)) {
         VDDR_LastError = VHDR_ERR_NOTVHD;
         if (ftr.u32DiskType==VHD_TYPE_FIXED || ftr.u32DiskType==VHD_TYPE_DYNAMIC || ftr.u32DiskType==VHD_TYPE_DIFFERENCING) {
            pVHD = Mem_Alloc(MEMF_ZEROINIT,sizeof(VHD_INFO));
            pVHD->f = f;
            Mem_Copy(&pVHD->ftr, &ftr, sizeof(ftr));

            pVHD->Base.GetDriveType       = VHDR_GetDriveType;
            pVHD->Base.GetDriveSize       = VHDR_GetDriveSize;
            pVHD->Base.GetDriveBlockCount = VHDR_GetDriveBlockCount;
            pVHD->Base.BlockStatus        = VHDR_BlockStatus;
            pVHD->Base.GetDriveUUID       = VHDR_GetDriveUUID;
            pVHD->Base.IsSnapshot         = VHDR_IsSnapshot;
            pVHD->Base.ReadPage           = VHDR_ReadPage;
            pVHD->Base.ReadSectors        = VHDR_ReadSectors;
            pVHD->Base.Close              = VHDR_Close;

            if (pVHD->ftr.u32DiskType == VHD_TYPE_FIXED) {
               pVHD->BlockSize = 0;
               pVHD->BlockShift = 0;
               pVHD->nBlocks = 1;
               pVHD->nBlocksAllocated = 1;
               pVHD->SectorsPerBlock = (UINT)(pVHD->ftr.u64CurrentSize>>9);
               pVHD->SPBshift = 0;
               VDDR_LastError = 0;
            } else if (ReadDynHeader(pVHD)) { // this sets LastError.
               pVHD->nBlocks   = pVHD->hdr.u32BlockCount;
               pVHD->BlockSize = pVHD->hdr.u32BlockSize;
               pVHD->BlockShift = PowerOfTwo(pVHD->BlockSize); // BlockShift will be 0xFFFFFFFF if BlockSize==0
               pVHD->SectorsPerBlock = (pVHD->BlockSize>>9);   // This will be 0 if BlockSize<512, but ...
               pVHD->SPBshift = PowerOfTwo(pVHD->SectorsPerBlock); // ... this returns 0xFFFFFFFF if SectorsPerBlock==0
               if (pVHD->BlockShift==0xFFFFFFFF || pVHD->SPBshift==0xFFFFFFFF) {
                  VDDR_LastError = VHDR_ERR_BADFORMAT;
               }
            }
            if (VDDR_LastError==0 && (pVHD->ftr.u32DiskType==VHD_TYPE_DYNAMIC||pVHD->ftr.u32DiskType==VHD_TYPE_DIFFERENCING)) {
               UINT mapsize = pVHD->hdr.u32BlockCount*sizeof(UINT);
               pVHD->blockmap = Mem_Alloc(0, mapsize);
               VDDR_LastError = VHDR_ERR_OUTOFMEM;
               if (pVHD->blockmap) {
                  VDDR_LastError = VHDR_ERR_SEEK;
                  File_Seek(pVHD->f, pVHD->hdr.u64TableOffset);
                  OSLastError = File_IOresult();
                  if (OSLastError==0) {
                     VDDR_LastError = VHDR_ERR_READ;
                     if (File_RdBin(pVHD->f, pVHD->blockmap, mapsize)==mapsize) {
                        pVHD->SectorsPerBitmap = ((pVHD->SectorsPerBlock+4095)>>12);
                        VDDR_LastError = VHDR_ERR_BLOCKMAP;
                        if (ValidateMap(pVHD)) VDDR_LastError = 0;
                        pVHD->hdr.u32Reserved = pVHD->nBlocksAllocated; // put here so ShowHeader can access it.
                     } else {
                        OSLastError = File_IOresult();
                     }
                  }
               }
            } else if (VDDR_LastError==0) {
               pVHD->hdr.u32BlockCount = pVHD->nBlocks; // put here so ShowHeader can access it.
               pVHD->hdr.u32Reserved   = pVHD->nBlocks; // put here so ShowHeader can access it.
            }

            if (VDDR_LastError==0 && VHDR_IsSnapshot((HVDDR)pVHD)) {
               pVHD->hVDIparent = VDDR_OpenByUUID(&pVHD->hdr.uuidParent,iChain+1); // may set VDDR_LastError
               if (iChain==0) VDDR_LastError = 0; // we let the user open the first in a broken snapshot chain for diagnostic purposes, though we don't let him clone it.
            }

            if (VDDR_LastError) {
               Mem_Free(pVHD->blockmap);
               pVHD = Mem_Free(pVHD);
            }
         }
      }
      if (!pVHD) File_Close(f);
   }
   return (HVDDR)pVHD;
}

/*.....................................................*/

PUBLIC BOOL
VHDR_GetDriveSize(HVDDR pThis, HUGE *drive_size)
{
   if (pThis) {
      PVHD pVHD = (PVHD)pThis;
      *drive_size = pVHD->ftr.u64CurrentSize;
      VDDR_LastError = 0;
      return TRUE;
   }
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC BOOL
VHDR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid)
{
   if (pThis) {
      PVHD pVHD = (PVHD)pThis;
      if (pVHD->hVDIparent) return pVHD->hVDIparent->GetDriveUUID(pVHD->hVDIparent, drvuuid);
      VDDR_LastError = 0;
      Mem_Copy(drvuuid, &pVHD->ftr.uuidCreate, sizeof(S_UUID));
      return TRUE;
   }
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

PUBLIC BOOL
VHDR_GetHeader(HVDDR pThis, VHD_FOOTER *vhdf, VHD_DYN_HEADER *vhdh)
{
   if (pThis) {
      PVHD pVHD = (PVHD)pThis;
      VDDR_LastError = 0;
      if (vhdf) Mem_Copy(vhdf, &pVHD->ftr, sizeof(VHD_FOOTER));
      if (vhdh) Mem_Copy(vhdh, &pVHD->hdr, sizeof(VHD_DYN_HEADER));
      return TRUE;
   }
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

static int
RawReadPage(PVHD pVHD, void *buffer, UINT iPage, UINT SectorOffset, UINT length)
// Read a block/page using the native block size. This is an internal
// function, so there's very little parameter checking. The calling
// app must ensure that length<(native blocksize) and SectorOffset+length
// does not go beyond the length of a block. Note that "blocks" don't
// really apply to fixed VHDs, but we imagine they exist anyway...
{
   VDDR_LastError = VHDR_ERR_INVBLOCK;
   if (iPage<pVHD->nBlocks) {
      UINT SID;

      VDDR_LastError = 0;

      if (pVHD->ftr.u32DiskType==VHD_TYPE_DYNAMIC || pVHD->ftr.u32DiskType==VHD_TYPE_DIFFERENCING) {
         SID = pVHD->blockmap[iPage];
         if (SID==VHD_PAGE_FREE) return VDDR_RSLT_NOTALLOC;
         SID += pVHD->SectorsPerBitmap;
      } else {
         SID = 0;
      }

      VDDR_LastError = VHDR_ERR_SEEK;
      OSLastError = DoSeek(pVHD->f, (((HUGE)SID)+SectorOffset)<<9);
      if (OSLastError==0) {
         VDDR_LastError = VHDR_ERR_READ;
         if (File_RdBin(pVHD->f, buffer, length)==length) {
            VDDR_LastError = 0;
            return VDDR_RSLT_NORMAL;
         }
         OSLastError = File_IOresult();
      }
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
VHDR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   if (pThis) {
      BYTE *pDest=buffer;
      PVHD pVHD = (PVHD)pThis;
      UINT SectorsToCopy,SectorsLeft,BytesToCopy,SectorOffset,iPage;
      HUGE sid;
      int  rslt;

      if (nSectors==0) return VDDR_RSLT_NORMAL;

      rslt = VDDR_RSLT_NOTALLOC;

      if (pVHD->ftr.u32DiskType==VHD_TYPE_DYNAMIC || pVHD->ftr.u32DiskType==VHD_TYPE_DIFFERENCING) {
         hugeop_shr(sid,LBA,pVHD->SPBshift); // LBA/SectorsPerBlock leaves iPage number in sid.
         iPage  = LO32(sid);
         SectorOffset = (UINT)(LBA & (pVHD->SectorsPerBlock-1));
         SectorsLeft = (pVHD->BlockSize>>9) - SectorOffset;
      } else {
         iPage = 0;
         SectorOffset = (UINT)LBA;
         SectorsLeft = (UINT)(pVHD->SectorsPerBlock - LBA);
      }

      while (nSectors) {
         SectorsToCopy = (SectorsLeft<=nSectors ? SectorsLeft : nSectors);
         nSectors -= SectorsToCopy;
         BytesToCopy = (SectorsToCopy<<9);

         pVHD->PageReadResult = RawReadPage(pVHD, pDest, iPage, SectorOffset, BytesToCopy);
         if (pVHD->PageReadResult==VDDR_RSLT_NOTALLOC && pVHD->hVDIparent) {
            // reading from unallocated page in snapshot child: pass read request to parent.
            pVHD->PageReadResult = pVHD->hVDIparent->ReadSectors(pVHD->hVDIparent,pDest,LBA,SectorsToCopy);
         }
         if (pVHD->PageReadResult == VDDR_RSLT_FAIL) return VDDR_RSLT_FAIL;
         if (pVHD->PageReadResult != VDDR_RSLT_NORMAL) Mem_Zero(pDest,BytesToCopy);
         if (rslt>pVHD->PageReadResult) rslt = pVHD->PageReadResult;
         pDest += BytesToCopy;
         LBA += SectorsToCopy;
         SectorOffset = 0;
         iPage++;
         if (iPage==pVHD->nBlocks) return rslt; // return with partial result when we reach EOF.
         SectorsLeft = pVHD->SectorsPerBlock;
      }
      return rslt;
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
VHDR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift)
{
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   if (pThis) {
      HUGE LBA;
      hugeop_fromuint(LBA,iPage);
      hugeop_shl(LBA,LBA,SPBshift);
      return VHDR_ReadSectors(pThis, buffer, LBA, 1<<SPBshift);
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC UINT
VHDR_GetDriveType(HVDDR pThis)
{
   return VDD_TYPE_VHD;
}

/*.....................................................*/

PUBLIC BOOL
VHDR_IsSnapshot(HVDDR pThis)
{
   if (pThis) {
      PVHD pVHD = (PVHD)pThis;
      VDDR_LastError = 0;
      if (pVHD->hVDIparent) return FALSE; // if we've already resolved the snapshot then don't alarm the app...
      return (pVHD->ftr.u32DiskType==VHD_TYPE_DIFFERENCING);
   }
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC UINT
VHDR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift)
{
   if (pThis) {
      PVHD pVHD = (PVHD)pThis;
      HUGE nBlocks = ((pVHD->ftr.u64CurrentSize>>9) + ((1<<SPBshift)-1))>>SPBshift;
      VDDR_LastError = 0;
      return LO32(nBlocks);
   }
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

static UINT
BlockUsed(PVHD pVHD, HUGE LBA_start, HUGE LBA_end)
{
   if (pVHD->ftr.u32DiskType==VHD_TYPE_DYNAMIC || pVHD->ftr.u32DiskType==VHD_TYPE_DIFFERENCING) {
      UINT SID,LastSID, SPB;
      SID = LO32(LBA_start>>pVHD->SPBshift);   // convert first sector LBA into VHD block number.
      LastSID = LO32(LBA_end>>pVHD->SPBshift); // convert last sector LBA into a VHD block number.
      SPB = 1<<pVHD->SPBshift;
      for (; SID<=LastSID; SID++) {            // we now have a range of VHD blocks to test.
         if (SID>=pVHD->nBlocks) break;
         if (pVHD->blockmap[SID]!=VHD_PAGE_FREE) return 1;
         else if (pVHD->hVDIparent) {
            UINT blks = pVHD->hVDIparent->BlockStatus(pVHD->hVDIparent,LBA_start,LBA_start+(SPB-1));
            if (blks==VDDR_RSLT_NORMAL) return blks;
         }
         LBA_start += SPB;
      }
   } else if (LBA_start<pVHD->SectorsPerBlock) { // flat file
      return 1;
   }
   return 0;
}

/*.....................................................*/

PUBLIC HVDDR
VHDR_Close(HVDDR pThis)
{
   VDDR_LastError = 0;
   if (pThis) { // we silently handle closing of an already closed file.
      PVHD pVHD = (PVHD)pThis;
      File_Close(pVHD->f);
      Mem_Free(pVHD->blockmap);
      Mem_Free(pVHD);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC UINT
VHDR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end)
{
   VDDR_LastError = VHDR_ERR_INVHANDLE;
   if (pThis) {
      VDDR_LastError = 0;
      if (BlockUsed((PVHD)pThis,LBA_start,LBA_end)) return VDDR_RSLT_NORMAL;
   }
   return VDDR_RSLT_NOTALLOC;
}

/*....................................................*/

PUBLIC BOOL
VHDR_QuickGetUUID(CPFN fn, S_UUID *UUID)
{
   FILE f = File_OpenRead(fn);
   if (f==NULLFILE) {
      VDDR_LastError = VHDR_ERR_FILENOTFOUND;
      if (File_IOresult()==DJFILE_ERROR_SHARE) VDDR_LastError = VHDR_ERR_SHARE;
   } else {
      VHD_FOOTER ftr;
      if (ReadFooter(f,&ftr)) {
         VDDR_LastError = 0;
         Mem_Copy(UUID, &ftr.uuidCreate, sizeof (S_UUID));
      }
      File_Close(f);
   }
   return (VDDR_LastError==0);
}

/*....................................................*/

/* end of module vhdr.c */

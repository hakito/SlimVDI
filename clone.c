/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* This module does the actual work of cloning, using the services of other modules. */
#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <mmsystem.h>
#include "clone.h"
#include "vddr.h"
#include "vdiw.h"
#include "thermo.h"
#include "djfile.h"
#include "mem.h"
#include "progress.h"
#include "fsys.h"
#include "partinfo.h"
#include "filename.h"
#include "djtypes.h"
#include "env.h"
#include "ids.h"
#include "enlarge.h"

#define BURST_BLOCKS 16

#define BLOCK_SIZE             1048576 /* must be a power of 2, and at least 512 */
#define SECTORS_PER_BLOCK      2048
#define SECTORS_PER_BURST      (2048*BURST_BLOCKS)
#define SPB_SHIFT              11      /* sectors per block again, but expressed as a shift */
#define MAX_MAPPED_PARTITIONS  8

static FILE          stderr;

static char          szfnSrc[4096];
static char          szfnDest[4096];
static HVDDR         SourceDisk;
static HVDIW         hVDIdst;
static HFSYS         pFSys[MAX_MAPPED_PARTITIONS];
static ProgInfo      prog;

// localized strings
static PSTR pszERROR           /* = "Error" */ ;
static PSTR pszLOMEM           /* = "Low memory! Could not allocate copy buffer!" */ ;
static PSTR pszFINDINGUSEDBLKS /* = "Finding used blocks in partitions - please wait..." */ ;
static PSTR pszCLONINGVHD      /* = "Cloning Virtual Hard Disk..." */ ;
static PSTR pszCLONEWAIT       /* = "Cloning virtual disk - please wait..." */ ;
static PSTR pszUSERABORT       /* = "Aborted by user" */ ;
static PSTR pszORIGINAL        /* = "Original " */ ;
static PSTR pszSNAPSHOT        /* = "Source is a difference image. Sorry, this tool cannot clone these (for now)" */ ;
static PSTR pszSIZEERR2        /* = "The new drive size must be a number followed by 'GB' or 'MB' to indicate the size range" */ ;
static PSTR pszSIZEERR3        /* = "Bad number format, or number too large, in new drive size field" */ ;
static PSTR pszSIZEERR4        /* = "The new drive size must be at least as large as the old drive size" */ ;
static PSTR pszSIZEERR5        /* = "This utility cannot create virtual drives larger than 2047.00 GB" */ ;
static PSTR pszBADNUM          /* = "Bad number in new drive size field" */ ;
static PSTR pszERRRENAME       /* = "Could not rename old VDI - a backup file of the intended name already exists! Therefore the clone will keep its temp filename of ""%s""" */ ;

/*.....................................................*/

static BOOL
Error(PSTR sz)
{
   if (stderr) {
      File_WrBin(stderr,sz,lstrlen(sz));
      File_WrBin(stderr,"\r\n",2);
   } else {
      MessageBox(GetFocus(),sz,RSTR(ERROR),MB_ICONSTOP|MB_OK);
   }
   prog.bUserCancel = TRUE;
   return FALSE;
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

static UINT
MapPartitions(HFSYS pFSys[MAX_MAPPED_PARTITIONS], s_CLONEPARMS *parm)
// This function checks each partition in turn, and if it uses a supported guest filesystem
// then I create a filesystem object which maps that partition into used/unused clusters. The
// maps are used during cloning to detect unused blocks.
//
// TODO: I assume in this function (and in others) that you can't have more than four
// partitions. That's true provided you only count the MBR. I'll have to change this
// if I ever support extended partitions or GPT partitions.
//
// MOD: In addition to the four real partitions, I also now create a "partition agent" to
// handle unpartitioned regions of the drive, ensuring that unpartitioned blocks are always
// considered to be unused.
//
// NOTE TO SELF: this function assumes that parm->MBR (MBR sector read when source disk was opened)
// is still valid. In future versions if I repartition before cloning, or fix the MBR in other
// ways then I need to make sure the updated MBR arrives here.
{
   int i,j=0;
   for (i=0; i<MAX_MAPPED_PARTITIONS; i++) pFSys[i] = NULL;
   if (parm->flags & PARM_FLAG_COMPACT) { // if we don't need to detect unused blocks then we don't need to know the filesystems.
      if (parm->MBR[510]==0x55 && parm->MBR[511]==0xAA) {
         HUGE startLBA,cLBA;
         HFSYS h;
         PPART pPart = (PPART)(parm->MBR+446);

         for (i=0; i<4; i++) {
            startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
            cLBA = (HUGE)MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
            h = FSys_OpenVolume(pPart->PartType,SourceDisk,startLBA,cLBA,512); // a non-NULL result indicates a supported guest filesystem.
            if (h) pFSys[j++] = h;
            if (j==MAX_MAPPED_PARTITIONS) break;
            pPart++;
         }
         if (j<MAX_MAPPED_PARTITIONS) {
            SourceDisk->GetDriveSize(SourceDisk,&cLBA);
            cLBA >>= 9;
            h = FSys_OpenVolume(0xFFFFFFFF,SourceDisk,0,cLBA,512); // map unpartitioned free space on drive too.
            if (h) pFSys[j++] = h;
         }
      }
   }
   return j;
}

/*.....................................................*/

static BOOL
IsBlockUsed(UINT iPage, UINT nMappedParts)
{
   if (nMappedParts) { // if we don't need to detect unused blocks then we don't need to know the filesystems.
      UINT j;
      for (j=0; j<nMappedParts; j++) {
         UINT BlockUsedCode = pFSys[j]->IsBlockUsed(pFSys[j],iPage,SPB_SHIFT);
         if (BlockUsedCode!=FSYS_BLOCK_OUTSIDE) {   // if block falls inside this partition
            if (BlockUsedCode==FSYS_BLOCK_UNUSED) { // ... and blocks are unused by guest filesystem.
               return FALSE;                        // then reduce the count of blocks we need to copy.
            }
            break;
         }
      }
   }
   return TRUE;
}

/*.....................................................*/

static UINT
CountUsedBlocks(UINT nMappedParts, UINT dst_nBlocks, BOOL nomerge)
{
   UINT iPage,nUsedBlocks=0;
   UINT blkstat;
   HUGE LBA = 0;

   for (iPage=0; iPage<dst_nBlocks; iPage++) {
      blkstat = (nomerge && SourceDisk->IsInheritedPage(SourceDisk, iPage) ? VDDR_RSLT_NOTALLOC
                 : SourceDisk->BlockStatus(SourceDisk,LBA,LBA+(SECTORS_PER_BLOCK-1)));
      LBA += (SECTORS_PER_BLOCK);
      if (blkstat==VDDR_RSLT_NORMAL) {
         if (IsBlockUsed(iPage,nMappedParts)) nUsedBlocks++;
      }
   }
   return nUsedBlocks;
}

/*.....................................................*/

static BOOL
DoClone(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm)
// This is the actual cloning function. Quite simple, it just reads a bunch
// of blocks from the source drive, (optionally) checks if they are used, and
// and writes them to the dest drive if so.
{
   UINT  i,iBurst,iPage,nBlocksWritten,DestPage,blkstat,nPages=parm->dst_nBlocks;
   BYTE  *buffer,*block;
   int   BlockStatus[BURST_BLOCKS];
   HUGE  LBA = 0;

   // to reduce seek overheads I read a "burst" of blocks then write
   // them as a burst too.
   buffer = Mem_Alloc(0,BLOCK_SIZE*BURST_BLOCKS);
   if (!buffer) {
      Error(RSTR(LOMEM));
      return FALSE;
   }

   // init progress stats and show progress window.
   FillMemory(&prog, sizeof(prog), 0);
   prog.pszFn = szfnSrc;
   prog.pszMsg = RSTR(CLONEWAIT);
   prog.pszCaption = RSTR(CLONINGVHD);
   prog.BytesTotal = parm->dst_nBlocksAllocated*(1.0*BLOCK_SIZE);
   Progress.Begin(hInstRes, hWndParent, &prog);
   Progress.UpdateStats(&prog);

   // loop over all copy blocks
   nBlocksWritten = 0;
   block = buffer;
   DestPage = 0;
   LBA = 0;

   for (iPage=iBurst=0; iPage<nPages; iPage++) { // iPage a.k.a. block number.

      blkstat = VDDR_RSLT_NOTALLOC;
      if (!((parm->flags & PARM_FLAG_NOMERGE) && SourceDisk->IsInheritedPage(SourceDisk, iPage)) && IsBlockUsed(iPage, parm->nMappedParts))
         blkstat = SourceDisk->ReadPage(SourceDisk,block,iPage,SPB_SHIFT);

      BlockStatus[iBurst] = blkstat;
      if (blkstat==VDDR_RSLT_FAIL) {
         // We could have some kind of error recovery in here: abort, or "recover and continue". The
         // dialog should also have a "don't show this again" checkbox.
         Error(VDDR_GetErrorString(0xFFFFFFFF));
         break;
      }
      block += BLOCK_SIZE;
      iBurst++;

      if (iBurst==BURST_BLOCKS || (iPage+1)==nPages) {
         if ((parm->flags & PARM_FLAG_FIXMBR) && DestPage==0) { // if fixmbr needed, and this is the first block...
            VDIW_FixMBR(hVDIdst,buffer);
         }
         block = buffer;
         for (i=0; i<iBurst; i++,DestPage++) {

            if (BlockStatus[i] != VDDR_RSLT_NOTALLOC) {
               if (!VDIW_WritePage(hVDIdst,block,DestPage,BlockStatus[i]==VDDR_RSLT_BLANKPAGE)) {
                  Error(VDIW_GetErrorString(0xFFFFFFFF));
                  goto _error_out;
               }

               // update the progress when we process a normal block of data. Note that the condition below
               // must test BlockStatus[i], not blkstat, otherwise the progress bar may not reach 100%.
               if (BlockStatus[i]==VDDR_RSLT_NORMAL) {
                  nBlocksWritten++;
                  prog.BytesDone = nBlocksWritten * (1.0*BLOCK_SIZE);
                  Progress.UpdateStats(&prog);
                  if (prog.bUserCancel) {
                     Error(RSTR(USERABORT));
                     goto _error_out;
                  }
               }
            }
            block += BLOCK_SIZE;
         }
         block = buffer;
         iBurst = 0;
      }
   }

_error_out:

   Mem_Free(buffer);
   return !(prog.bUserCancel);
}

/*.....................................................*/

static void
GenerateName(PFN dstfn, CPFN srcfn, PSTR pszPrefix)
{
   static FNCHAR path[4096];
   static FNCHAR tail[4096];
   lstrcpy(tail,pszPrefix);
   Filename_SplitPath(srcfn,path,tail+lstrlen(pszPrefix));
   Filename_MakePath(dstfn,path,tail);
}

/*....................................................*/

static BOOL
IsSnapshot(HVDDR SourceDisk)
{
   if (SourceDisk->IsSnapshot(SourceDisk)) {
      Error(RSTR(SNAPSHOT));
      return TRUE;
   }
   return FALSE;
}

/*....................................................*/

static BOOL
TooSmall(HVDDR SourceDisk, s_CLONEPARMS *parm)
// The user has selected "enlarge" and entered a new drive size.
// Return TRUE if this is too small to keep the existing data.
{
   HUGE DriveSize;
   SourceDisk->GetDriveSize(SourceDisk,&DriveSize);
   DriveSize >>= 9; // convert to sectors.

   // when enlarging drive <8GB I'll need to correct drive geometry assumptions in MBR.
   if (DriveSize < (1023*255*63)) parm->flags |= PARM_FLAG_FIXMBR;

   return (parm->DestSectors<DriveSize);
}

/*....................................................*/

static int
ParseDestSize(HVDDR SourceDisk, s_CLONEPARMS *parm)
{
   UINT i,c;
   PSTR szSize = parm->szDestSize;
   double dDrvSize,mantissa;

   // parse the integer part of the new size;
   i = 0;
   dDrvSize = 0;
   c = szSize[i++];
   while (c>='0' && c<='9') {
      if (c<'0' || c>'9') break;
      dDrvSize = dDrvSize*10 + (c-'0');
      c = szSize[i++];
   }

   if (c=='.') {
      c = szSize[i++];

      // parse the mantissa part of the new size
      mantissa = 0.1;
      while (c>='0' && c<='9') {
         if (c<'0' || c>'9') break;
         dDrvSize = dDrvSize + (c-'0')*mantissa;
         mantissa *= 0.1;
         c = szSize[i++];
      }
   }

   while (c==' ' || c==9) {
      c = szSize[i++];
   }

   if (c=='M' || c=='m') {
      if (dDrvSize>(2047*1024)) return 5;
      parm->DestSectors = (UINT)(dDrvSize*2048+0.5);
   } else if (c=='G' || c=='g') {
      if (dDrvSize>2047) return 5;
      parm->DestSectors = (UINT)(dDrvSize*1024.0*2048.0+0.5);
   } else {
      return 2;
   }
   if (TooSmall(SourceDisk, parm)) return 4;

   c = szSize[i++];
   if (c!='B' && c!='b') return 2;

   do {
      c = szSize[i++];
   } while (c==' ' || c==9);

   if (c!=0) return 3;

   return 0;
}

/*....................................................*/

static BOOL
DestSizeOK(HVDDR SourceDisk, s_CLONEPARMS *parm)
{
   if (parm->flags & PARM_FLAG_ENLARGE) {
      int err = ParseDestSize(SourceDisk,parm);
      switch (err) {
         case 0:
            break;
         case 2:
            return Error(RSTR(SIZEERR2));
         case 1:
         case 3:
            return Error(RSTR(SIZEERR3));
         case 4:
            return Error(RSTR(SIZEERR4));
         case 5:
            return Error(RSTR(SIZEERR5));
         default:
            return Error(RSTR(BADNUM));
      }
   }
   return TRUE;
}

/*....................................................*/

PUBLIC BOOL
Clone_Proceed(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm)
// Open the source file, create the dest file, calculate how much work we have to
// do (so we can animate the progress meter), then call the DoClone() function. We also
// clean up appropriately when the work is done.
{
   BOOL bNameMatch,bSuccess;
   UINT i,dst_nBlocks,dst_nBlocksAllocated,dst_MaxBlocks,ParmFlags,nMappedParts;
// HVDDR cow;

   ParmFlags = parm->flags;
   if (ParmFlags & PARM_FLAG_CLIMODE) {
      stderr = (FILE)GetStdHandle(STD_ERROR_HANDLE);
      if (!stderr || stderr==NULLFILE) ParmFlags &= (~PARM_FLAG_CLIMODE); // happens if app is not run from a console window.
   }
   if (!(ParmFlags & PARM_FLAG_CLIMODE)) stderr = 0;
   
   lstrcpy(szfnSrc, parm->srcfn);
   if (Filename_Compare(parm->srcfn,parm->dstfn)==0) {
      // If source and destination names are the same then generate a temp name
      // for the clone.
      FNCHAR path[256];
      bNameMatch = TRUE;
      Filename_SplitPath(parm->dstfn,path,NULL);
      GetTempFileName(path,"Clo",0,szfnDest);
      File_Erase(szfnDest);
      Filename_ChangeExtension(szfnDest,"vdi");
   } else {
      bNameMatch = FALSE;
      lstrcpy(szfnDest, parm->dstfn);
   }

   SourceDisk = VDDR_Open(szfnSrc,0);
   if (!SourceDisk) return Error(VDDR_GetErrorString(0xFFFFFFFF));
   if (IsSnapshot(SourceDisk) || !DestSizeOK(SourceDisk,parm)) {
      SourceDisk->Close(SourceDisk);
      return FALSE;
   }
   if (SourceDisk->GetDriveType(SourceDisk)==VDD_TYPE_PART_RAW) parm->flags |= PARM_FLAG_FIXMBR;

   if (parm->flags & PARM_FLAG_ENLARGE) {
      dst_MaxBlocks = (parm->DestSectors>>SPB_SHIFT);
      if (parm->DestSectors & (SECTORS_PER_BLOCK-1)) dst_MaxBlocks++; // note that adding (SECTORS_PER_BLOCK-1) before the shift might cause overflow of UINT.
      if (parm->flags & PARM_FLAG_REPART) { // if we are expanding the partition as well...
         SourceDisk = Enlarge_Drive(SourceDisk, dst_MaxBlocks);
         parm->flags &= (~PARM_FLAG_FIXMBR); // in this case the Enlarge_Drive() function has already fixed the MBR.
      }
      dst_nBlocks = SourceDisk->GetDriveBlockCount(SourceDisk,SPB_SHIFT);
   } else {
      dst_nBlocks = SourceDisk->GetDriveBlockCount(SourceDisk,SPB_SHIFT);
      dst_MaxBlocks = dst_nBlocks;
   }

   // Calculate number of allocated blocks on source drive, express in units of dest blocks.
   // This is needed to calculate disk space requirements, and for the progress meter.
   // get used/unused cluster maps for partitions on source drive.
   SourceDisk->ReadSectors(SourceDisk, parm->MBR, 0, 1); // read MBR sector.
   nMappedParts = MapPartitions(pFSys,parm);
   dst_nBlocksAllocated = CountUsedBlocks(nMappedParts, dst_nBlocks, parm->flags & PARM_FLAG_NOMERGE);

   // Create the dest VDI.
   hVDIdst = VDIW_Create(szfnDest,BLOCK_SIZE,dst_MaxBlocks,dst_nBlocksAllocated);
   if (!hVDIdst) {
      if (VDIW_GetLastError()==VDIW_ERR_EXISTS) bSuccess = FALSE; // user already got an error message in this case.
      else bSuccess = Error(VDIW_GetErrorString(0xFFFFFFFF));
      SourceDisk->Close(SourceDisk);
   } else {
      if (parm->flags & PARM_FLAG_KEEPUUID) {
         S_UUID uuidCreate;
         SourceDisk->GetDriveUUID(SourceDisk,&uuidCreate);
         VDIW_SetDriveUUID(hVDIdst,&uuidCreate);
      }

      if ((parm->flags & PARM_FLAG_NOMERGE) && SourceDisk->GetDriveType(SourceDisk) == VDD_TYPE_VDI) {
         S_UUID uuidCreate, uuidModify;

         if (parm->flags & PARM_FLAG_KEEPUUID) { // adopt the UUIDs from the source VDI
            SourceDisk->GetDriveUUIDs(SourceDisk, &uuidCreate, &uuidModify);
            VDIW_SetDriveUUIDs(hVDIdst, &uuidCreate, &uuidModify);
         }

         // adopt the parent UUIDs from the source VDI
         SourceDisk->GetParentUUIDs(SourceDisk, &uuidCreate, &uuidModify);
         VDIW_SetParentUUIDs(hVDIdst, &uuidCreate, &uuidModify);
      }

      parm->dst_nBlocks = dst_nBlocks;
      parm->dst_nBlocksAllocated = dst_nBlocksAllocated;
      parm->nMappedParts = nMappedParts;

      // start cloning!
      bSuccess = DoClone(hInstRes, hWndParent, parm);

      // destroy the partition usage map objects then close the source disk.
      for (i=0; i<nMappedParts; i++) pFSys[i]->CloseVolume(pFSys[i]);
      SourceDisk->Close(SourceDisk);

      if (!bSuccess) VDIW_Discard(hVDIdst);
      else {
         VDIW_Close(hVDIdst);
         // at this point we know the clone task was successful, the clone file now exists.
         if (bNameMatch) {
            // if source and dest had conflicting filenames then we will have written the clone to a temp file. Now
            // that cloning has succeeded we want to juggle the filenames to get it all as the user wants.
            GenerateName(szfnSrc, parm->srcfn, RSTR(ORIGINAL)); // rename the original to "Original <oldname>".
            if (File_Rename(parm->srcfn, szfnSrc)) {        // that may fail in the rare case that "Original <oldname>" already exists.
               File_Rename(szfnDest, parm->srcfn);          // ... if it worked then we rename the temp file to match <oldname>.
            } else {                                        // ... if it didn't work then we give up and tell the user the bad news.
               char szMsg[512];
               char tail[256];
               Filename_SplitTail(szfnDest,tail);
               wsprintf(szMsg,RSTR(ERRRENAME),tail);
               Error(szMsg);
               bSuccess = FALSE;
            }
         }
      }
   }

   if (bSuccess) {
      if (!(parm->flags & PARM_FLAG_CLIMODE)) PlaySound("notify.wav", NULL, SND_FILENAME);
   }
   Progress.End(&prog);
   return bSuccess;
}

/*.....................................................*/

#if 0

// This is debug code I wrote to compare disk images before and after
// cloning, listing all changed sectors (so I can see any unintended
// changes). Not used normally, preserved in case I ever need it again.

/*.....................................................*/

static BOOL
SectorsDifferent(BYTE *pbsect1, BYTE *pbsect2)
{
   UINT i,*sect1,*sect2;
   sect1 = (UINT*)pbsect1;
   sect2 = (UINT*)pbsect2;
   for (i=(512/sizeof(UINT)); i; i--) {
      if (*sect1++ != *sect2++) return TRUE;
   }
   return FALSE;
}

/*.....................................................*/

#define N_SECTORS 31455206

PUBLIC BOOL
Clone_CompareImages(HINSTANCE hInstRes, HWND hWndParent, s_CLONEPARMS *parm)
{
   HVDDR Disk1,Disk2;
   UINT ns,nDiffSectors,nBlock,nBlocks,LBA,nSectors;
   BYTE szOut[256],*sect1,*sect2;
   FILE fOut;

   Disk1 = VDDR_Open("D:\\VDI\\Clone of xp.vdi",0);
   if (!Disk1) return FALSE;
   Disk2 = VDDR_Open("C:\\DJ2\\Copy of Clone of xp.vdi",0);
   if (!Disk2) return FALSE;

   // init progress stats and show progress window.
   FillMemory(&prog, sizeof(prog), 0);
   prog.pszFn = "D:\\VDI\\Clone of xp.vdi";
   prog.pszMsg = "Comparing images - please wait";
   prog.pszCaption = "Comparing Images";
   prog.BytesTotal = N_SECTORS*512.0;
   Progress.Begin(hInstRes, hWndParent, &prog);
   Progress.UpdateStats(&prog);

   sect1 = Mem_Alloc(0,BLOCK_SIZE);
   sect2 = Mem_Alloc(0,BLOCK_SIZE);
   nBlocks = ((N_SECTORS+(SECTORS_PER_BLOCK-1)) >> SPB_SHIFT);

   fOut = File_Create("C:\\CompareSectors.txt",DJFILE_FLAG_OVERWRITE);

   nDiffSectors = 0;
   for (nBlock=0; nBlock<nBlocks; nBlock++) {
      LBA = nBlock<<SPB_SHIFT;
      nSectors = ((LBA+SECTORS_PER_BLOCK)<=N_SECTORS ? SECTORS_PER_BLOCK : N_SECTORS-LBA);
      Disk1->ReadSectors(Disk1,sect1,LBA,nSectors);
      Disk2->ReadSectors(Disk2,sect2,LBA,nSectors);
      for (ns=0; ns<nSectors; ns++,LBA++) {
         if (SectorsDifferent(sect1+(ns<<9),sect2+(ns<<9))) nDiffSectors++;
         else if (nDiffSectors) {
            if (nDiffSectors==1) wsprintf(szOut,"%lu\r\n",LBA-1);
            else wsprintf(szOut,"%lu-%lu\r\n",LBA-nDiffSectors,LBA-1);
            File_WrBin(fOut,szOut,lstrlen(szOut));
            nDiffSectors=0;
         }
      }
      prog.BytesDone = (nBlock+1) * (1.0*BLOCK_SIZE);
      Progress.UpdateStats(&prog);
      if (prog.bUserCancel) {
         break;
      }
   }
   Progress.End(&prog);

   if (nDiffSectors) {
      if (nDiffSectors==1) wsprintf(szOut,"%lu\r\n",N_SECTORS-1);
      else wsprintf(szOut,"%lu-%lu\r\n",N_SECTORS-nDiffSectors,N_SECTORS-1);
      File_WrBin(fOut,szOut,lstrlen(szOut));
   }

   Disk1->Close(Disk1);
   Disk2->Close(Disk2);
   File_Close(fOut);

   Mem_Free(sect1);
   Mem_Free(sect2);

   return TRUE;
}

/*.....................................................*/

#endif

/*.....................................................*/

/* end of clone.c */


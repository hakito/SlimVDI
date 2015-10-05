/*================================================================================*/
/* Copyright (C) 2011, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include <stdarg.h>
#include <windows.h>
#include "mediareg.h"
#include "djtypes.h"
#include "filename.h"
#include "djstring.h"
#include "vddr.h"
#include "memfile.h"
#include "mem.h"

// I use the string/data cache idea a lot. It is a data structure which expands
// dynamically as required, it stores variable length data items efficiently, and
// it allows me to reference stored items using a pointer, with no danger that
// the pointer might be invalidated by a GlobalResize() of the container.
//
#define FN_CACHE_INCR 4096
typedef struct t_FN_CACHE {
   struct t_FN_CACHE *pNextCache;
   UINT BytesUsed;
   BYTE buff[FN_CACHE_INCR];
} FN_CACHE;
static FN_CACHE *pFnCache;

typedef struct {
   S_UUID   uuid;
   FILETIME ftLastModify;
   PFN      filename;
} MED_REG_ENTRY, *PMEDREG;

#define MRE_STATIC_HDR_SIZE (sizeof(MED_REG_ENTRY)-sizeof(FNCHAR)*256)

static HMEMFILE hmRegData;
static UINT nRegEntries;

/*.....................................................*/

PUBLIC void
MediaReg_Discard(void)
{
   if (hmRegData) {
      MemFile.Close(hmRegData);

      hmRegData = NULL;
      nRegEntries = 0;

      while (pFnCache) {
         PVOID pNext = pFnCache->pNextCache;
         Mem_Free(pFnCache);
         pFnCache = pNext;
      }
   }
}

/*.....................................................*/

static PFN
StoreFilename(CPFN pfn)
{
   UINT len,alloc;
   PFN  rslt;

   len = (Filename_Length(pfn)+1)*sizeof(FNCHAR);
   alloc = (len+7) & ~7; // round up to qword boundary.
   if (pFnCache==NULL || (pFnCache->BytesUsed+alloc)>FN_CACHE_INCR) {
      FN_CACHE *p = Mem_Alloc(MEMF_ZEROINIT,sizeof(FN_CACHE));
      p->pNextCache = pFnCache;
      pFnCache = p;
   }
   rslt = pFnCache->buff + pFnCache->BytesUsed;
   CopyMemory(rslt, pfn, len);
   pFnCache->BytesUsed += alloc;
   return rslt;
}

/*.....................................................*/

static BOOL
UUIDmatch(S_UUID *pUUID1, S_UUID *pUUID2)
{
   UINT i;
   for (i=0; i<4; i++) {
      if (pUUID1->au32[i] != pUUID2->au32[i]) return FALSE;
   }
   return TRUE;
}
         
/*.....................................................*/

static PMEDREG
FindVirtualDisk(S_UUID *pUUID)
{
   if (hmRegData && nRegEntries) {
      PMEDREG pmr = (PMEDREG)MemFile.GetPtr(hmRegData,0);
      UINT i=0;
      do {
         if (UUIDmatch(pUUID,&pmr->uuid)) return pmr;
         pmr++;
      } while ((++i)<nRegEntries);
   }
   return NULL;
}

/*.....................................................*/

static BOOL
AddToRegistry(CPFN fn, S_UUID *pUUID, FILETIME *ftLastModify)
{
   PMEDREG pmr = FindVirtualDisk(pUUID);
   if (pmr) { // entry with this UUID exists already.
      if (CompareFileTime(ftLastModify,&pmr->ftLastModify)<0) {
         // in the case of a UUID clash we prefer the older file because the
         // newer one is quite possibly a clone.
         CopyMemory(&pmr->ftLastModify,ftLastModify,sizeof(FILETIME));
         pmr->filename = StoreFilename(fn);
      }
   } else {
      MED_REG_ENTRY mre;

      CopyMemory(&mre.uuid,pUUID,16);
      CopyMemory(&mre.ftLastModify,ftLastModify,sizeof(FILETIME));
      mre.filename = StoreFilename(fn);
     
      if (!hmRegData) hmRegData = MemFile.Create(0);
      MemFile.WrBin(hmRegData,&mre,sizeof(mre));
      nRegEntries++;
      return TRUE;
   }
   return FALSE;
}

/*.....................................................*/

static BOOL
AddMediaFile(CPFN fn, FILETIME *ftLastModify)
// At the time of the call we don't actually know that this IS a
// media file (virtual disk). So first we check that it is,
// then we attempt to extract a UUID, finally we attempt to
// store the UUID+filename pair in our media registry.
{
   S_UUID uuid;
   BOOL bOK = VDDR_QuickGetUUID(fn,&uuid);
   if (bOK) return AddToRegistry(fn,&uuid,ftLastModify);
   return FALSE;
}

/*.....................................................*/

static int
ScanFolder(FNCHAR *fnPath)
{
   HANDLE hFind;
   WIN32_FIND_DATA wfd;
   int pathlen,len,nFilesAdded=0;
   
   pathlen = len = Filename_Length(fnPath);
   fnPath[len++] = '\\';
   fnPath[len++] = '*';
   fnPath[len]   = (FNCHAR)0;
   
   hFind = FindFirstFile(fnPath,&wfd);
   if (!hFind || (hFind == INVALID_HANDLE_VALUE)) return 0;
   do {
      if ((wfd.dwFileAttributes & (FILE_ATTRIBUTE_COMPRESSED|FILE_ATTRIBUTE_DIRECTORY))==0) {
         Filename_Copy(fnPath+pathlen+1, wfd.cFileName, 1024-pathlen-1);
         nFilesAdded += AddMediaFile(fnPath,&wfd.ftLastWriteTime);
      }
   } while (FindNextFile(hFind, &wfd));
   FindClose(hFind);
   fnPath[pathlen] = (FNCHAR)0;
   return nFilesAdded;
}

/*.....................................................*/

PUBLIC BOOL
MediaReg_ScanFolder(CPFN pfn)
{
   FNCHAR fnPath[1024],fnTail[512];
   int nFilesAdded;

   if (pfn[0]=='\\' && pfn[1]=='\\' && pfn[2]=='.') return FALSE; // Windows UNC name, e.g. "\\.\PhysicalDrive". Not a folder.
      
   // The argument identifies a file in a folder. We want to search the folder. So to get
   // the search name we discard the tailname and substitute a wildcard.
   Filename_SplitPath(pfn, fnPath, NULL);

   nFilesAdded = ScanFolder(fnPath);

   // if the folder being searched is called "Snapshots" then include
   // the contents of the parent folder too.
   Filename_SplitPath(fnPath,fnPath,fnTail);
   if (Filename_Compare(fnTail,"Snapshots")==0) {
      nFilesAdded += ScanFolder(fnPath);
   }

   return (nFilesAdded>0);
}

/*.....................................................*/

PUBLIC PFN
MediaReg_FilenameFromUUID(S_UUID *pUUID)
{
   PMEDREG pmr = FindVirtualDisk(pUUID);
   if (pmr) return pmr->filename;
   return FALSE;
}

/*.....................................................*/

/* end of mediareg.c */

/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include "memfile.h"
#include "mem.h"

#define ALLOC_SIZE_INCR 4096
#define Alloc(size) Mem_Alloc(MEMF_ZEROINIT, size)

typedef enum{MEMFREAD, MEMFWRITE, MEMFREADWRITE} MEMFIOMODES;

typedef struct {
   MEMFIOMODES iomode;
   BOOL bOwnsMemory;
   int  rwptr;     /* current read/write pointer */
   int  allocsize; /* size of memory block pointed to by pMem field */
   int  len;       /* high water mark for writes. */
   PSTR pMem;
} MEMFCB, *PMEMFCB;

/*....................................................*/

static HMEMFILE
MemFile_Create(UINT initialAllocBytes)
{
   PMEMFCB pFCB    = Alloc(sizeof(MEMFCB));
   pFCB->iomode    = MEMFWRITE;
   pFCB->allocsize = (initialAllocBytes ? initialAllocBytes : ALLOC_SIZE_INCR);
   pFCB->pMem      = Alloc(pFCB->allocsize);
   pFCB->bOwnsMemory = TRUE;
   return (HMEMFILE)pFCB;
}

/*....................................................*/

static HMEMFILE
MemFile_Open(HANDLE hMemObj, UINT objLenBytes)
{
   if (hMemObj) {
      PMEMFCB pFCB    = Alloc(sizeof(MEMFCB));
      PSTR pSrc;
      pFCB->iomode    = MEMFREADWRITE;
      pFCB->allocsize = objLenBytes;
      pFCB->len       = objLenBytes;
      pFCB->pMem      = Alloc(objLenBytes);
      pFCB->bOwnsMemory = TRUE;
      pSrc = GlobalLock(hMemObj);
      CopyMemory(pFCB->pMem, pSrc, objLenBytes);
      GlobalUnlock(hMemObj);
      return (HMEMFILE)pFCB;
   }
   return NULL;
}

/*....................................................*/

static HMEMFILE
MemFile_OpenRead(HANDLE hMemObj, UINT objLenBytes, BOOL bAssignOwnership)
{
   if (hMemObj) {
      PMEMFCB pFCB    = Alloc(sizeof(MEMFCB));
      pFCB->iomode    = MEMFREAD;
      pFCB->allocsize = objLenBytes;
      pFCB->len       = objLenBytes;
      pFCB->pMem      = GlobalLock(hMemObj);
      pFCB->bOwnsMemory = bAssignOwnership;
      return (HMEMFILE)pFCB;
   }
   return NULL;
}

/*....................................................*/

static int
MemFile_RdBin(HMEMFILE h, void *pData, int len)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   int newpos = pFCB->rwptr + len;
   if (newpos>pFCB->len) {
      newpos = pFCB->len;
      len = newpos - pFCB->rwptr;
   }
   CopyMemory(pData, pFCB->pMem+pFCB->rwptr, len);
   pFCB->rwptr += len;
   return len;
}

/*....................................................*/

static int
MemFile_WrBin(HMEMFILE h, const void *pData, int len)
{
   if (len) {
      PMEMFCB pFCB = (PMEMFCB)h;
      int newpos = pFCB->rwptr + len;
      int allocsize = pFCB->allocsize;

      if (pFCB->iomode == MEMFREAD) return 0; // can't write to read-only file.

      if (newpos>allocsize) {
         PSTR pMem;
         while (newpos>allocsize) {
            allocsize += ALLOC_SIZE_INCR;
         }
         pMem = Mem_ReAlloc(pFCB->pMem,MEMF_ZEROINIT,allocsize);
         if (!pMem) return 0;
         pFCB->allocsize = allocsize;
         pFCB->pMem = pMem;
      }

      CopyMemory(pFCB->pMem+pFCB->rwptr, pData, len);
      pFCB->rwptr += len;
      if (pFCB->rwptr > pFCB->len) pFCB->len = pFCB->rwptr;
   }
   return len;
}

/*....................................................*/

static int
MemFile_DwordAlign(HMEMFILE h)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   pFCB->rwptr = ((pFCB->rwptr + 3) & ~3);
   return pFCB->rwptr;
}

/*....................................................*/

static int
MemFile_Seek(HMEMFILE h, int pos)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   pFCB->rwptr = pos;
   return pFCB->rwptr;
}

/*....................................................*/

static int
MemFile_GetPos(HMEMFILE h)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   return pFCB->rwptr;
}

/*....................................................*/

static PVOID
MemFile_GetPtr(HMEMFILE h, int pos)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   return pFCB->pMem + pos;
}

/*....................................................*/

static int
MemFile_Size(HMEMFILE h)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   return pFCB->len;
}

/*....................................................*/

static BOOL
MemFile_EOF(HMEMFILE h)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   return (pFCB->rwptr >= pFCB->len);
}

/*....................................................*/

static PVOID
MemFile_Extract(HMEMFILE h, int *lenBytes)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   pFCB->iomode = MEMFREAD;
   pFCB->bOwnsMemory = FALSE;
   if (lenBytes) *lenBytes = pFCB->len;
   return (pFCB->pMem);
}

/*....................................................*/

static HANDLE
MemFile_Copy(HMEMFILE h, UINT gMemFlags)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   HANDLE hMem = GlobalAlloc(gMemFlags, pFCB->len);
   PSTR pDest = GlobalLock(hMem);
   CopyMemory(pDest, pFCB->pMem, pFCB->len);
   GlobalUnlock(hMem);
   return hMem;
}

/*....................................................*/

static void
MemFile_Close(HMEMFILE h)
{
   PMEMFCB pFCB = (PMEMFCB)h;
   if (pFCB->bOwnsMemory) {
      /* discard the io buffer owned by the memfile */
      Mem_Free(pFCB->pMem);
   }
   Mem_Free(pFCB);
}

/*....................................................*/

static char outbuf[4096];

void MemFile_mprintf(HMEMFILE f, PSTR s, ...)
{
   if (f) {
      wsprintf(outbuf, s, ((PSTR)&s)+4);
      MemFile.WrBin(f, outbuf, lstrlen(outbuf));
   }
}

/*......................................................*/

MemFile_DEF MemFile = {
   MemFile_Create,
   MemFile_Open,
   MemFile_OpenRead,
   MemFile_RdBin,
   MemFile_WrBin,
   MemFile_DwordAlign,
   MemFile_Seek,
   MemFile_GetPos,
   MemFile_GetPtr,
   MemFile_Size,
   MemFile_EOF,
   MemFile_Extract,
   MemFile_Copy,
   MemFile_Close,
   MemFile_mprintf
};

/*....................................................*/

/* end of module memfile.c */



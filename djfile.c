/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* implementation of module DJFile */
#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include "djfile.h"

#define PUBLIC

static UINT IOR;

/*.....................................................*/

PUBLIC void
File_SetIOR(UINT err)
{
   IOR = err;
}

/*.....................................................*/

PUBLIC FILE
File_Open(CPFN fn)
{
   FILE h = CreateFile(fn,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,0);
   if (h!=NULLFILE) IOR=0;
   else IOR = GetLastError();
   return h;
}

/*.....................................................*/

PUBLIC FILE
File_Create(CPFN fn, UINT flags)
{
   DWORD dwOpenMode=CREATE_NEW;
   DWORD dwAccess=GENERIC_WRITE;
   DWORD dwflags=0;
   FILE h;
   
   if (flags & DJFILE_FLAG_OVERWRITE) dwOpenMode = CREATE_ALWAYS;
   if (flags & DJFILE_FLAG_DELETEONCLOSE) dwflags |= FILE_FLAG_DELETE_ON_CLOSE;
   if (flags & DJFILE_FLAG_WRITETHROUGH) dwflags |= FILE_FLAG_WRITE_THROUGH;
   if (flags & DJFILE_FLAG_SEQUENTIAL) dwflags |= FILE_FLAG_SEQUENTIAL_SCAN;
   if (flags & DJFILE_FLAG_READWRITE) dwAccess |= GENERIC_READ;
   h = CreateFile(fn,dwAccess,0,0,dwOpenMode,dwflags,0);
   if (h!=NULLFILE) IOR=0;
   else IOR = GetLastError();
   return h;
}

/*.....................................................*/

PUBLIC FILE
File_OpenRead(CPFN fn)
{
   HANDLE h = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,0);
   if (h!=NULLFILE) IOR=0;
   else IOR = GetLastError();
   return (FILE)h;
}

/*.....................................................*/

PUBLIC FILE
File_OpenReadShared(CPFN fn)
{
   HANDLE h = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_FLAG_RANDOM_ACCESS,0);
   if (h!=NULLFILE) IOR=0;
   else IOR = GetLastError();
   return (FILE)h;
}

/*.....................................................*/

PUBLIC void
File_Close(FILE f)
{
   if (CloseHandle(f)) IOR = 0;
   else IOR = GetLastError();
}

/*.....................................................*/

PUBLIC UINT
File_RdBin(FILE f, PVOID buf, UINT Size)
{
   DWORD BytesRead;
   if (ReadFile(f,buf,Size,&BytesRead,0)) IOR=0;
   else IOR=GetLastError();
   return BytesRead;
}

/*.....................................................*/

PUBLIC UINT
File_WrBin(FILE f, PVOID buf, UINT Size)
{
   DWORD BytesWritten;
   if (WriteFile(f,buf,Size,&BytesWritten,0)) IOR=0;
   else IOR = GetLastError();
   return BytesWritten;
}

/*.....................................................*/

PUBLIC void
File_Seek(FILE f, HUGE pos)
{
   UINT newposhi = HI32(pos);
   UINT newposlo = SetFilePointer(f,LO32(pos),(long*)&newposhi,FILE_BEGIN);
   IOR = 0;
   if (newposlo == 0xFFFFFFFF) IOR = GetLastError();
}

/*.....................................................*/

PUBLIC UINT
File_GetPos(FILE f, HUGE *pos)
{
   UINT newposhi = 0;
   UINT newposlo = SetFilePointer(f,0,(long*)&newposhi,FILE_CURRENT);
   if (newposlo == 0xFFFFFFFF) IOR = GetLastError();
   if (pos) *pos = MAKEHUGE(newposlo,newposhi);
   return newposlo;
}

/*.....................................................*/

PUBLIC UINT
File_Size(FILE f, HUGE *size)
{
   UINT size_hi;
   UINT size_lo = GetFileSize(f,(DWORD*)&size_hi);
   IOR = 0;
   if (size_lo==0xFFFFFFFF) IOR=GetLastError();
   if (size) *size = MAKEHUGE(size_lo,size_hi);
   return size_lo;
}

/*.....................................................*/

PUBLIC void
File_Erase(CPFN fn)
{
   if (DeleteFile(fn)) IOR = 0;
   else IOR = GetLastError();
}

/*.....................................................*/

PUBLIC BOOL
File_Rename(CPFN oldfn, CPFN fn)
{
   if (MoveFile(oldfn, fn)) IOR = 0;
   else IOR = GetLastError();
   return (IOR==0);
}

/*.....................................................*/

PUBLIC UINT
File_GetDate(FILE f)
{
   FILETIME ft;
   IOR = 0;
   if (GetFileTime(f,0,0,&ft)) {
      WORD date,time;
      FileTimeToDosDateTime(&ft,&date,&time);
      return MAKELONG(time,date);
   }
   IOR = GetLastError();
   return 0;
}

/*.....................................................*/

PUBLIC void
File_SetDate(FILE f, UINT d)
{
   FILETIME ft;
   IOR = 0;
   if (DosDateTimeToFileTime(HIWORD(d),LOWORD(d),&ft)) {
      if (SetFileTime(f,0,&ft,&ft)) return;
   }
   IOR = GetLastError();
}

/*.....................................................*/

PUBLIC UINT
File_IOresult(void)
{
   return IOR;
}

/*.....................................................*/

PUBLIC BOOL
File_Exists(CPFN path)
{
   FILE f = File_OpenRead(path);
   File_Close(f);
   return (f!=NULLFILE);
}

/*.....................................................*/

/* end of module djfile.c */





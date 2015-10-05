/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/*======================================================================*/
/* Miscellaneous interactions with the OS environment                   */
/*======================================================================*/

#include "djwarning.h"
#define NOTOOLBAR
#include <stdarg.h>
#include <windows.h>
#include <winioctl.h>
#include <commdlg.h>
#include "env.h"
#include "filename.h"
#include "mem.h"
#include "ids.h"

#define ARGBUFSIZ  16384
#define N_MAX_ARGS 128
#define STR_CACHE_INCR 4096

typedef struct t_STRING_CACHE {
   struct t_STRING_CACHE *pNextCache;
   UINT BytesUsed;
   CHAR buff[STR_CACHE_INCR];
} STRING_CACHE;

static UI32   argc;
static PSTR   argv[N_MAX_ARGS];
static CHAR   argbuff[ARGBUFSIZ];
static HINSTANCE hInstRes,hInstLang;
static UINT   CloneVDI_Language_Code;
static LANGID idDefaultLanguage;
static STRING_CACHE *pStrCache;
static BOOL   bCOMInitDone;

static PSTR pszSELECT_SOURCE /* = "Select source file..." */ ;
static PSTR pszSRCFILEFILTER /* = "Virtual drive files\0*.vdi;*.vhd;*.vmdk;*.raw;*.img\0\0" */ ;
static PSTR pszSELECT_DEST   /* = "Select destination filename..." */ ;
static PSTR pszDSTFILEFILTER /* = "Virtual drive files\0*.vdi\0\0" */ ;

/*.....................................................*/

PUBLIC UI32
Env_ProcessCmdLine(HANDLE hInstApp, PSTR rawarg)
/* This function scans the raw argument string, copying it to a destination
 * buffer which the app is allowed to modify. Every time this function detects
 * the start of a new command line argument it records the start position
 * pointer in the argv[] array and increments the argc count. During the
 * copy we replace whitespace with NULLs so that the argv strings are
 * properly terminated.
 */
{
   UI32 i,sptr,dptr,origdptr;
   CHAR c;

   hInstRes = hInstApp;
   
   dptr = GetModuleFileNameA(hInstApp,argbuff,MAX_PATH)+1;
   argv[0] = argbuff;

   argc=sptr=0;
   c = rawarg[sptr++];
   while (c) {
      origdptr = dptr;

      /* skip past leading spaces and tabs */
      while (c==' ' || c==((char)9)) c = rawarg[sptr++];

      /* copy this argument to the dest buffer */
      if (c=='"') {

         c = rawarg[sptr++];
         while (c) {
            if (c=='"') break;
            else argbuff[dptr++]=c;
            c = rawarg[sptr++];
         }
         if (c=='"') c=rawarg[sptr++];

      } else {

         while (c) {
            if (c==' ' || c==((char)9)) break;
            else argbuff[dptr++]=c;
            c = rawarg[sptr++];
         }
         
      }

      if (dptr>origdptr) {
         argv[++argc] = argbuff + origdptr;
         argbuff[dptr++]  = (char)0;
      }

      if (c) c = rawarg[sptr++];
   }
   for (i=argc+1; i<=N_MAX_ARGS; i++) argv[i]=NULL;
   return argc;
}

/*.....................................................*/

PUBLIC void
Env_GetProgramPath(PFN path)
{
   Filename_SplitPath(argbuff,path,NULL);
}

/*.............................................*/

PUBLIC UI32
Env_ParamCount(void)
{
   return argc;
}

/*.....................................................*/

PUBLIC PSTR
Env_ParamStr(UI32 n)
{
   if (n<=argc) return argv[n];
   return NULL;
}

/*.....................................................*/

PUBLIC int
Env_sprintf(PSTR szDest, CPSTR s, ...)
{
   int rslt;
   va_list argList;
   va_start(argList, s);
   rslt = wvsprintf(szDest, s, argList);
   va_end(argList);
   return rslt;
}

/*......................................................*/

PUBLIC BOOL
Env_GetDiskFreeSpace(CPFN szPath, HUGE *freebytes)
{
   ULARGE_INTEGER UserFreeBytes,BytesUsed,TotalFreeBytes;
   if (GetDiskFreeSpaceEx(szPath,&UserFreeBytes,&BytesUsed,&TotalFreeBytes)) {
      *freebytes = UserFreeBytes.QuadPart;
      return TRUE;
   }
   return FALSE; // most likely an invalid path
}

/*......................................................*/

PUBLIC BOOL
Env_GetDriveSectors(HANDLE hDrive, UINT *DriveSectors)
{
   DISK_GEOMETRY geom;
   DWORD bytes;
   BOOL bSuccess = DeviceIoControl(hDrive,IOCTL_DISK_GET_DRIVE_GEOMETRY,NULL,0,&geom,sizeof(geom),&bytes,NULL);
   if (bSuccess) {
      *DriveSectors = (geom.Cylinders.u.LowPart * geom.TracksPerCylinder * geom.SectorsPerTrack);
   }
   return bSuccess;
}

/*......................................................*/

PUBLIC UI32
Env_AskYN(PSTR szMsg, PSTR szCaption)
{
   UI32 rslt = MessageBox(GetFocus(),szMsg,szCaption,MB_ICONSTOP|MB_YESNO);
   if (rslt==IDYES) return 1;
   return 0;
}

/*......................................................*/

PUBLIC UI32
Env_AskYNC(PSTR szMsg, PSTR szCaption)
{
   UI32 rslt = MessageBox(GetFocus(),szMsg,szCaption,MB_ICONSTOP|MB_YESNOCANCEL);
   if (rslt==IDYES) return 1;
   if (rslt==IDCANCEL) return 2;
   return 0;
}

/*......................................................*/

static PSTR pszCLONEOF;

PUBLIC void
Env_GenerateCloneName(PFN pfnClone, CPFN pfnSource)
{
   static FNCHAR path[4096];
   static FNCHAR tail[4096];
   lstrcpy(tail,RSTR(CLONEOF)); // rstr() also inits pszCLONEOF
   Filename_SplitPath(pfnSource,path,tail+lstrlen(pszCLONEOF));
   if (Filename_IsExtension(tail,"vhd") || Filename_IsExtension(tail,"vmdk") ||
       Filename_IsExtension(tail,"raw") || Filename_IsExtension(tail,"img")  ||
       Filename_IsExtension(tail,"hdd")) {
      Filename_ChangeExtension(tail,"vdi");
   }
   Filename_MakePath(pfnClone,path,tail);
}

/*......................................................*/

PUBLIC PVOID
Env_LoadBinData(PSTR szResource)
{
   HANDLE hRes = FindResource(hInstRes, szResource, RT_RCDATA);
   if (hRes) { // am I supposed to free the hRes handle?
      HGLOBAL hData = LoadResource(hInstRes,hRes);
      if (hData) {
         return LockResource(hData);
      }
   }
   return NULL;
}

/*......................................................*/

PUBLIC void
Env_SetLanguage(HINSTANCE hInstLangDLL, UINT idLanguage)
{
   hInstLang = hInstLangDLL;
   if (idLanguage==1)      idDefaultLanguage = MAKELANGID(LANG_DUTCH,SUBLANG_DUTCH);
   else if (idLanguage==2) idDefaultLanguage = MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN);
   else if (idLanguage==3) idDefaultLanguage = MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH);
   else {
      idLanguage = 0;
      idDefaultLanguage = MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_UK);
   }
   CloneVDI_Language_Code = idLanguage;
}

/*.....................................................*/

PUBLIC UINT
Env_GetLanguage(void)
{
   return CloneVDI_Language_Code;
}

/*.....................................................*/

static LPCWSTR
FindStringResourceEx(HINSTANCE hinst, UINT uId, UINT langId)
{
   HRSRC hrsrc = FindResourceEx(hinst, RT_STRING, MAKEINTRESOURCE((uId>>4)+1), (WORD)langId);
   if (hrsrc) {
      HGLOBAL hglob = LoadResource(hinst, hrsrc);
      if (hglob) {
         LPCWSTR pwsz = (LPCWSTR)LockResource(hglob);
         if (pwsz) {
            // okay, now walk the string table
            uId &= 0xF;
            for (; uId; uId--) pwsz += (1 + (*pwsz));
            return pwsz;
         }
      }
   }
   return NULL;
}

/*......................................................*/

PUBLIC PSTR
Env_LoadString(PSTR *psz, UINT ids)
// Note I assume that string table entries are no longer than STR_CACHE_INCR chars in length, including terminating NUL).
{
   if (*psz == NULL) {
      LPCWSTR pwsz = (ids==0 ? NULL : FindStringResourceEx(hInstLang, ids, idDefaultLanguage));
      if (pwsz==NULL || pwsz[0]==0) *psz = "";
      else {
         UINT i,len,alloc;
         len = pwsz[0];
         alloc = (len+4) & ~3; // add space for NUL byte then round up to dword boundary.
         if (pStrCache==NULL || (pStrCache->BytesUsed+alloc)>STR_CACHE_INCR) {
            STRING_CACHE *p = Mem_Alloc(MEMF_ZEROINIT,sizeof(STRING_CACHE));
            p->pNextCache = pStrCache;
            pStrCache = p;
         }
         for (i=0; i<len; i++) pStrCache->buff[pStrCache->BytesUsed+i] = (CHAR)(pwsz[i+1]); // copy wide string to ANSI.
         pStrCache->buff[pStrCache->BytesUsed+len] = (CHAR)0; // NUL terminate.
         *psz = pStrCache->buff + pStrCache->BytesUsed;
         pStrCache->BytesUsed += alloc;
      }
   }
   return *psz;
}

/*......................................................*/

PUBLIC void
Env_DoubleToString(PSTR S, double X, UINT D)
{
   DWORD i,Ptr,Cnt,Digit;
   BOOL  Negative;

   if (X!=0.0) {
      Negative = (BOOL)(X<0.0);
      Cnt=0; Ptr=Negative;
      if (Negative) X = (-X);
      switch (D) {
         case 0: X+=0.5;      break;
         case 1: X+=(0.5e-1); break;
         case 2: X+=(0.5e-2); break;
         case 3: X+=(0.5e-3); break;
         case 4: X+=(0.5e-4); break;
         case 5: X+=(0.5e-5); break;
         default:
            D=6; X+=0.0000005;
      }
      for (; X>=10.0; Cnt++) X /= 10.0;
      for (i=0; i<=Cnt; i++) {
         Digit = (DWORD)X;
         S[Ptr++] = (char)(Digit+48);
         X = (X-((double)Digit))*10.0;
      }
      if (D) {
         S[Ptr++] = '.';
         for (i=1; i<=D; i++) {
            Digit = (DWORD)X;
            S[Ptr++] = (char)(Digit+48);
            if (i!=D) X = (X-((double)Digit))*10.0;
         }
      }
      if (Negative) S[0] = '-';
   } else {
      lstrcpy(S,"0.0000000000");
      Ptr = 2+D;
   }
   S[Ptr] = (char)0;
}

/*...............................................*/

PUBLIC UINT
Env_GetTempFileName(PSTR path, PSTR pszPrefix, UINT uUnique, PSTR pszOutputFilename)
{
   BYTE szTempPath[1024];
   if (!path) {
      GetTempPath(1024,szTempPath);
      path = szTempPath;
   }
   return GetTempFileName(path,pszPrefix,uUnique,pszOutputFilename);
}

/*...............................................*/

PUBLIC UINT
Env_GetEnvironmentVariable(PSTR pszDest, CPSTR pszVarName, UINT maxLen)
{
   return GetEnvironmentVariable(pszVarName,pszDest,maxLen);
}

/*...............................................*/

PUBLIC BOOL
Env_DetectWine(void)
{
   HMODULE hMod = LoadLibrary("ntdll.dll");
   if (hMod) {
      FARPROC pfn = GetProcAddress(hMod, "wine_get_version");
      FreeLibrary(hMod);
      return (pfn!=NULL);
   }
   return FALSE;
}

/*...............................................*/

PUBLIC BOOL
Env_InitComAPI(BOOL bInit)
{
   BOOL rslt;
   if (bInit) {
      if (!bCOMInitDone) bCOMInitDone = SUCCEEDED(CoInitialize(NULL));
      rslt = bCOMInitDone;
   } else {
      if (bCOMInitDone) CoUninitialize();
      rslt = TRUE;
   }
   return rslt;
}

/*...............................................*/

PUBLIC int
Env_FormatUUID(PSTR pszUUID, S_UUID *pUUID)
{
   int  i;
   PSTR sz;
   sz = pszUUID;
   sz += wsprintf(sz,"{%08lx",pUUID->au32[0]);
   for (i=2; i<4; i++) {
      *sz++ = '-';
      sz += wsprintf(sz,"%04lx",pUUID->au16[i]);
   }
   *sz++ = '-';
   for (i=8; i<10; i++) {
      sz += wsprintf(sz,"%02lx",pUUID->au8[i]);
   }
   *sz++ = '-';
   for (i=10; i<16; i++) {
      sz += wsprintf(sz,"%02lx",pUUID->au8[i]);
   }
   *sz++ = '}';
   *sz = (CHAR)0;
   return (int)(sz-pszUUID);
}

/*....................................................*/

PUBLIC BOOL
Env_BrowseFiles(HANDLE hWndParent, PSTR fnDflt, BOOL bInputFile, PSTR pszTemplate)
{
   BOOL bResult;
   OPENFILENAME ofn;
   ZeroMemory(&ofn,sizeof(ofn)); // needed in case MS decides to extend the structure.
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner   = hWndParent;
   ofn.lpstrCustomFilter = NULL;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = fnDflt;
   ofn.nMaxFile = MAX_PATH;
   ofn.lpstrFileTitle = NULL;
   ofn.lpstrInitialDir = NULL;
   ofn.lpstrDefExt = "vdi";
   ofn.nFileOffset = ofn.nFileExtension = 0;
   ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

   if (pszTemplate) {
      ofn.Flags |= OFN_ENABLETEMPLATE;
      ofn.hInstance = hInstLang;
      ofn.lpTemplateName = pszTemplate;
   }

   if (bInputFile) {
      ofn.Flags |= OFN_FILEMUSTEXIST;
      ofn.lpstrTitle  = RSTR(SELECT_SOURCE);
      ofn.lpstrFilter = RSTR(SRCFILEFILTER);
      bResult = GetOpenFileName(&ofn);
   } else {
      ofn.lpstrTitle  = RSTR(SELECT_DEST);
      ofn.lpstrFilter = RSTR(DSTFILEFILTER);
      bResult = GetSaveFileName(&ofn);
   }
   return bResult;
}

/*....................................................*/

/* end of module env.c */

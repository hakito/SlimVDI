/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* Module mostly responsible for animating the main dialog box */
#include "djwarning.h"
#define NOTOOLBAR
#include <stdarg.h>
#include <windows.h>
#include <commdlg.h>
#include "thermo.h"
#include "hexview.h"
#include "version.h"
#include "filename.h"
#include "vddr.h"
#include "mem.h"
#include "showheader.h"
#include "random.h"
#include "clone.h"
#include "partinfo.h"
#include "sectorviewer.h"
#include "ntfs.h"
#include "djfile.h"
#include "memfile.h"
#include "profile.h"
#include "env.h"
#include "djstring.h"
#include "cmdline.h"
#include "ids.h"

// fixed strings, these don't get localized
static PSTR szINIFileName = "SlimVDI.ini"; // must not be longer than 31 chars.
static PSTR szSrcFileName = "SrcFile";
static PSTR szDstFileName = "DstFile";
static PSTR szLanguage    = "Language";

// these strings get localized
static PSTR pszNONE          /* = "None" */ ;
static PSTR pszOK            /* = "Ok" */ ;
static PSTR pszERROR         /* = "Error" */ ;
static PSTR pszUNKNOWNFS     /* = "Unknown(%02lx)" */ ;
static PSTR pszPLSSELSRC     /* = "Please select a source virtual disk" */ ;
static PSTR pszNOSRC         /* = "Cannot proceed - the source file does not exist!" */ ;
static PSTR pszERRAPPWND     /* = "Could not create app window (error x%x)" */ ;
static PSTR pszDLG_SLIMVDI  /* = "DLG_SLIMVDI" */ ;

/*
// these declarations moved to parms.h
//
#define PARM_FLAG_KEEPUUID  1
#define PARM_FLAG_ENLARGE   2
#define PARM_FLAG_REPART    4
#define PARM_FLAG_COMPACT   8

typedef struct {
   UINT flags;
   FNCHAR srcfn[1024];
   FNCHAR dstfn[1024];
   BYTE MBR[512];
   HVDDR hVDIsrc;
   CHAR  szDestSize[32];
   UINT  DestSectors;
} s_CLONEPARMS;
*/

// main dialog field idents
#define IDD_SOURCE_FN         200
#define IDD_BTN_SBROWSE       201
#define IDD_DEST_FN           202
#define IDD_BTN_DBROWSE       203
#define IDD_VALID_RESULT      204
#define IDD_OLD_SIZE          205
#define IDD_FILESYSTEM        206
#define IDD_UUID_CHANGE       207
#define IDD_UUID_KEEP         208
#define IDD_INCREASE_SIZE     209
#define IDD_NEW_SIZE          210
#define IDD_INCREASE_PARTSIZE 211
#define IDD_COMPACT           212
#define IDD_NOMERGE           214
#define IDD_ABOUT_TEXT        213
#define IDD_BTN_PARTINFO      300
#define IDD_BTN_HDRINFO       301
#define IDD_BTN_SECTOR_VIEW   302
#define IDD_BTN_PROCEED       1
#define IDD_BTN_EXIT          2

static HINSTANCE hInstApp;
static s_CLONEPARMS parm;
static FNCHAR tmpfn[1024];
static BOOL bCompactOptionDefault = FALSE;

/*.......................................................................*/

static BOOL
Error(PSTR pszMsg)
{
   FILE stderr = 0;

   if (parm.flags & PARM_FLAG_CLIMODE) stderr = GetStdHandle(STD_ERROR_HANDLE);
   if (stderr == NULLFILE) stderr = 0;
   if (stderr) {
      File_WrBin(stderr,pszMsg,lstrlen(pszMsg));
      File_WrBin(stderr,"\r\n",2);
   } else {
      MessageBox(GetFocus(), pszMsg, RSTR(ERROR), MB_ICONEXCLAMATION|MB_OK);
   }
   return FALSE;
}

/*..........................................................*/

static PSTR
FSName(HVDDR hVDIsrc, PPART pPart)
{
   WORD superblock[512];
   PSTR psz;
   static char szUnk[16];
   switch (pPart->PartType) {
      case 0x00: psz=RSTR(NONE);  break;
      case 0x01: psz="FAT12"; break;
      case 0x04:
      case 0x06:
      case 0x0E: psz="FAT16"; break;
      case 0x82:
      case 0x05: psz="Linux swap"; break;
      case 0x07: {
         HUGE startLBA;
         startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (NTFS_IsNTFSVolume(hVDIsrc,startLBA)) psz="NTFS"; // if this returns true then it's definitely NTFS.
         else psz="HPFS";
         // could also be FAT64(exFAT) - need distinguishing code.
         break;
      }
      case 0x0B: // fall through
      case 0x0C: psz="FAT32"; break;
      case 0x42: {
         HUGE startLBA;
         startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         if (NTFS_IsNTFSVolume(hVDIsrc,startLBA)) psz="NTFS"; // if this returns true then it's definitely NTFS.
         else psz="LDM";
         break;
      }
      case 0x83: {
         UINT startLBA = (UINT)MAKELONG(pPart->loStartLBA,pPart->hiStartLBA);
         hVDIsrc->ReadSectors(hVDIsrc, (BYTE*)superblock, startLBA+2, 2);
         if (superblock[28]==0xEF53) {
            if (superblock[48] & 0x40) /* incompat: supports extents? */ psz = "ext4";
			else if (superblock[46] & 4) /* compat: journalling support? */ psz = "ext3";
            else psz = "ext2";
         } else {
            psz="ext1";
         }
         break;
      }
      default:
         wsprintf(szUnk,RSTR(UNKNOWNFS),pPart->PartType);
         psz=szUnk;
   }
   return psz;
}

/*....................................................*/

static void
GetFileSystem(PSTR sz)
{
   if (parm.MBR[510]==0x55 && parm.MBR[511]==0xAA) {
      PPART pPart;
      UINT  i;
      PSTR  psz = sz;

      psz[0] = 0;
      for (i=0; i<4; i++) {
         pPart = (PPART)(parm.MBR+(446+i*16));
         if (pPart->PartType) {
            if (i) *psz++ = ',';
            psz += wsprintf(psz,"%s",FSName(parm.hVDIsrc,pPart));
         }
      }
      if (!sz[0]) lstrcpy(sz,RSTR(NONE));

   } else {
      lstrcpy(sz,RSTR(NONE));
   }
}

/*....................................................*/

static void
ShowDriveInfo(HWND hDlg)
{
   SetDlgItemText(hDlg,IDD_VALID_RESULT,VDDR_GetErrorString(0xFFFFFFFF));
   if (parm.hVDIsrc) {
      UINT GB,MB,R;
      CHAR sz[256],numstr[16];
      HUGE drivesize;
      parm.hVDIsrc->GetDriveSize(parm.hVDIsrc,&drivesize);

      GB = (UINT)(drivesize>>30); // yes, drivesize[1] can overflow, but only if size is (2^30)GB or more!
      MB = (UINT)((drivesize & 0x3FFFFFFF)>>20);
      R  = (UINT)(drivesize & 0xFFFFF);

      if (GB) { // if drive is greater than 1GB.
          Env_DoubleToString(numstr,GB+MB/1024.0,2);
          wsprintf(sz, "%s GB", numstr);
      } else {
          Env_DoubleToString(numstr,MB+R/1024.0,2);
          wsprintf(sz, "%s MB", numstr);
      }
      SetDlgItemText(hDlg,IDD_OLD_SIZE,sz);
      SetDlgItemText(hDlg,IDD_NEW_SIZE,sz);

      parm.hVDIsrc->ReadSectors(parm.hVDIsrc, parm.MBR, 0, 1); // read MBR sector.

      GetFileSystem(sz);
      SetDlgItemText(hDlg,IDD_FILESYSTEM,sz);
      EnableWindow(GetDlgItem(hDlg,IDD_BTN_HDRINFO),TRUE);

   } else {
      SetDlgItemText(hDlg,IDD_OLD_SIZE,"");
      SetDlgItemText(hDlg,IDD_FILESYSTEM,"");
      SetDlgItemText(hDlg,IDD_OLD_SIZE,"");
      SetDlgItemText(hDlg,IDD_FILESYSTEM,"");
      EnableWindow(GetDlgItem(hDlg,IDD_BTN_HDRINFO),FALSE);
   }
}

/*....................................................*/

static void
OpenNewSource(HWND hDlg, BOOL bRefreshDlg)
{
   if (parm.hVDIsrc) parm.hVDIsrc->Close(parm.hVDIsrc);
   VDDR_OpenMediaRegistry(parm.srcfn);
   parm.hVDIsrc = VDDR_Open(parm.srcfn,0);
   if (bRefreshDlg) ShowDriveInfo(hDlg);
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_PROCEED),parm.hVDIsrc!=NULL);
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_PARTINFO),parm.hVDIsrc!=NULL);
   EnableWindow(GetDlgItem(hDlg,IDD_BTN_SECTOR_VIEW),parm.hVDIsrc!=NULL);

   parm.flags = 0;
   if (IsDlgButtonChecked(hDlg,IDD_UUID_KEEP)) parm.flags |= PARM_FLAG_KEEPUUID;
   if (IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE)) parm.flags |= PARM_FLAG_ENLARGE;
   if (IsDlgButtonChecked(hDlg,IDD_COMPACT)) parm.flags |= PARM_FLAG_COMPACT;
   if (IsDlgButtonChecked(hDlg,IDD_NOMERGE)) parm.flags |= PARM_FLAG_NOMERGE;
}

/*....................................................*/

static BOOL
DoItForHeavensSake(HWND hWndParent)
{
   if (!Filename_IsExtension(parm.dstfn,"vdi")) {
      if (Filename_IsExtension(parm.dstfn,"vhd") || Filename_IsExtension(parm.dstfn,"vmdk") ||
          Filename_IsExtension(parm.dstfn,"raw") || Filename_IsExtension(parm.dstfn,"img")  ||
          Filename_IsExtension(parm.dstfn,"hdd")) {
         Filename_ChangeExtension(parm.dstfn,"vdi");
      } else {
         Filename_AddExtension(parm.dstfn,"vdi");
      }
   }
   return Clone_Proceed(hInstApp,hWndParent,&parm);
// return Clone_CompareImages(hInstApp,hWndParent,&parm);
}

/*....................................................*/

BOOL CALLBACK
DialogProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   static int busy;

   switch (iMsg) {
      case WM_INITDIALOG: {
         char sz[256],szFmt[256];

         GetWindowText(hDlg,szFmt,256);
         wsprintf(sz,szFmt,SOFTWARE_VERSION>>8,SOFTWARE_VERSION & 0xFF);
         SetWindowText(hDlg,sz);

         GetDlgItemText(hDlg,IDD_ABOUT_TEXT,szFmt,256);
         wsprintf(sz,szFmt,SOFTWARE_VERSION>>8,SOFTWARE_VERSION & 0xFF,COPYRIGHT_YEAR);
         SetDlgItemText(hDlg,IDD_ABOUT_TEXT,sz);

         SetDlgItemText(hDlg,IDD_SOURCE_FN,parm.srcfn);
         SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);

         EnableWindow(GetDlgItem(hDlg,IDD_NEW_SIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);
         EnableWindow(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);

         CheckDlgButton(hDlg,IDD_UUID_KEEP,(parm.flags & PARM_FLAG_KEEPUUID)!=0);
         CheckDlgButton(hDlg,IDD_UUID_CHANGE,(parm.flags & PARM_FLAG_KEEPUUID)==0);

         if (bCompactOptionDefault) parm.flags |= PARM_FLAG_COMPACT;
         else parm.flags &= (~PARM_FLAG_COMPACT);
         CheckDlgButton(hDlg,IDD_COMPACT,(parm.flags & PARM_FLAG_COMPACT)!=0);

         CheckDlgButton(hDlg,IDD_NOMERGE,(parm.flags & PARM_FLAG_NOMERGE)!=0);

         if (parm.srcfn[0]) {
            OpenNewSource(hDlg,TRUE);
         } else {
            ShowDriveInfo(hDlg);
            SetDlgItemText(hDlg,IDD_VALID_RESULT,RSTR(PLSSELSRC));
         }
         EnableWindow(GetDlgItem(hDlg,IDD_BTN_PROCEED),parm.hVDIsrc!=NULL);
         EnableWindow(GetDlgItem(hDlg,IDD_BTN_PARTINFO),parm.hVDIsrc!=NULL);
         EnableWindow(GetDlgItem(hDlg,IDD_BTN_SECTOR_VIEW),parm.hVDIsrc!=NULL);

         busy = FALSE;
         return TRUE;
      }
      case WM_COMMAND: {
         UINT idCtl = LOWORD(wParam);
         busy++;
         if (busy==1) {
            switch (idCtl) {
               case IDD_SOURCE_FN:
                  if (HIWORD(wParam)==EN_KILLFOCUS) {
                     GetDlgItemText(hDlg, IDD_SOURCE_FN, tmpfn, 1024);
                     if (String_Compare(tmpfn,parm.srcfn)) {
                        lstrcpy(parm.srcfn, tmpfn);
                        Env_GenerateCloneName(parm.dstfn,parm.srcfn);
                        SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);
                        OpenNewSource(hDlg,TRUE);
                     }
                  }
                  break;
               case IDD_BTN_SBROWSE:
                  lstrcpy(tmpfn, parm.srcfn);
                  if (Env_BrowseFiles(hDlg,tmpfn,TRUE,NULL)) {
                     lstrcpy(parm.srcfn, tmpfn);
                     SetDlgItemText(hDlg,IDD_SOURCE_FN,parm.srcfn);
                     Env_GenerateCloneName(parm.dstfn,parm.srcfn);
                     SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);
                     OpenNewSource(hDlg,TRUE);
                  }
                  break;
               case IDD_BTN_DBROWSE:
                  if (Env_BrowseFiles(hDlg,parm.dstfn,FALSE,NULL)) {
                     SetDlgItemText(hDlg,IDD_DEST_FN,parm.dstfn);
                  }
                  break;
               case IDD_UUID_CHANGE:
                  parm.flags = (parm.flags & ~PARM_FLAG_KEEPUUID);
                  if (!IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_KEEPUUID;
                  CheckDlgButton(hDlg,IDD_UUID_KEEP,(parm.flags & PARM_FLAG_KEEPUUID)!=0);
                  break;
               case IDD_UUID_KEEP:
                  parm.flags = (parm.flags & ~PARM_FLAG_KEEPUUID);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_KEEPUUID;
                  CheckDlgButton(hDlg,IDD_UUID_CHANGE,(parm.flags & PARM_FLAG_KEEPUUID)==0);
                  break;
               case IDD_INCREASE_SIZE:
                  parm.flags &= (~PARM_FLAG_ENLARGE);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_ENLARGE;
                  EnableWindow(GetDlgItem(hDlg,IDD_NEW_SIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);
                  EnableWindow(GetDlgItem(hDlg,IDD_INCREASE_PARTSIZE),(parm.flags & PARM_FLAG_ENLARGE)!=0);
                  if (!(parm.flags & PARM_FLAG_ENLARGE)) CheckDlgButton(hDlg,IDD_INCREASE_PARTSIZE,FALSE);
                  break;
               case IDD_COMPACT:
                  parm.flags &= (~PARM_FLAG_COMPACT);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_COMPACT;
                  break;
               case IDD_NOMERGE:
                  parm.flags &= (~PARM_FLAG_NOMERGE);
                  if (IsDlgButtonChecked(hDlg,idCtl)) parm.flags |= PARM_FLAG_NOMERGE;
                  break;
               case IDD_BTN_PARTINFO:
                  if (parm.hVDIsrc) {
                     PartInfo_Show(hInstApp,hDlg,parm.MBR);
                  }
                  break;
               case IDD_BTN_SECTOR_VIEW:
                  if (parm.hVDIsrc) {
                     SectorViewer_Show(hInstApp,hDlg,parm.hVDIsrc);
                  }
                  break;
               case IDD_BTN_HDRINFO:
                  if (parm.hVDIsrc) {
                     ShowHeader_Show(hInstApp,hDlg,parm.hVDIsrc);
                  }
                  break;
               case IDD_BTN_PROCEED:
                  if (parm.hVDIsrc) {
                     if (IsDlgButtonChecked(hDlg,IDD_INCREASE_SIZE) && IsDlgButtonChecked(hDlg,IDD_INCREASE_PARTSIZE)) parm.flags |= PARM_FLAG_REPART;
                     parm.hVDIsrc = parm.hVDIsrc->Close(parm.hVDIsrc);
                     GetDlgItemText(hDlg, IDD_SOURCE_FN, parm.srcfn, 1024);
                     GetDlgItemText(hDlg, IDD_DEST_FN, parm.dstfn, 1024);
                     GetDlgItemText(hDlg, IDD_NEW_SIZE, parm.szDestSize, 32);
                     DoItForHeavensSake(hDlg);
                     OpenNewSource(hDlg,FALSE);
                  } else {
                     Error(RSTR(NOSRC));
                  }
                  break;
               case IDD_BTN_EXIT: {
                  //Profile_SetString(szSrcFileName,parm.srcfn);
                  //Profile_SetString(szDstFileName,parm.dstfn);
                  DestroyWindow(hDlg);
                  break;
               }
            }
         }
         busy--;
         return TRUE;
      }
      case WM_CTLCOLORSTATIC:
         if ((HWND)lParam == GetDlgItem(hDlg,IDD_VALID_RESULT)) {
            char sz[256];
            GetDlgItemText(hDlg,IDD_VALID_RESULT,sz,256);
            if (lstrcmpi(sz,RSTR(OK))!=0) {
               HDC hDC = (HDC)wParam;
               SetBkMode(hDC,TRANSPARENT);
               SetTextColor(hDC,RGB(255,0,0));
               return (BOOL)GetSysColorBrush(COLOR_BTNFACE);
            }
         }
         break;
      case WM_CLOSE:
         if (!busy) DestroyWindow(hDlg);
         return TRUE;
      case WM_DESTROY:
         PostQuitMessage(0);
         return TRUE;
   }
   return FALSE;
}

/*.......................................................................*/

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int nCmdShow)
{
   int  rslt,argc=Env_ProcessCmdLine(hInstance, szCmdLine);

   hInstApp = hInstance;
   Thermo.RegisterClass(hInstance);
   Random_Randomize(GetTickCount());

   Profile_Init(hInstApp,szINIFileName); // tell Profile module where the .INI file is.

   Env_SetLanguage(hInstApp, Profile_GetOption(szLanguage)); // Set preferred UI language.

   if (argc == 0) {
      HWND hWnd;
      MSG  msg;
      int status;
      
      parm.flags = 0;
      parm.hVDIsrc = NULL;
      Profile_GetString(szSrcFileName,parm.srcfn,MAX_PATH,"");
      Profile_GetString(szDstFileName,parm.dstfn,MAX_PATH,"");
      bCompactOptionDefault = (Profile_GetOption("Compact")!=0);

      HexView_RegisterClass(hInstance);

      hWnd = CreateDialogParam (hInstApp, RSTR(DLG_SLIMVDI), NULL, DialogProc, 0);

      if (!hWnd) {
         char buf[256];
         wsprintf (buf, RSTR(ERRAPPWND), GetLastError());
         MessageBox (0, buf, "CreateDialog", MB_ICONEXCLAMATION | MB_OK);
         return 1;
      }

      while ((status = GetMessage (& msg, 0, 0, 0)) != 0) {
         if (status==(-1)) break;
         if (!IsDialogMessage (hWnd, & msg)) {
            TranslateMessage ( & msg );
            DispatchMessage ( & msg );
         }
      }

      if (parm.hVDIsrc) parm.hVDIsrc->Close(parm.hVDIsrc);
      rslt = msg.wParam;
   } else {
      // CLI mode.
      AttachConsole(ATTACH_PARENT_PROCESS);
      rslt = 1;
      if (CmdLine_Parse(&parm)) {
         VDDR_OpenMediaRegistry(parm.srcfn);
         if (DoItForHeavensSake(NULL)) rslt = 0;
      }
   }

   Env_InitComAPI(FALSE);

   return rslt;
}

/*.......................................................................*/

/* end of slimvdi.c */



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
#include <commdlg.h>
#include <mmsystem.h>
#include "sectorviewer.h"
#include "profile.h"
#include "partinfo.h"
#include "progress.h"
#include "filename.h"
#include "djfile.h"
#include "mem.h"
#include "hexview.h"
#include "env.h"
#include "ids.h"

static PSTR szfnDump = "SectorDump";
static PSTR szWrSectDlg[] = {
   "DLG_WRITE_SECTORS",
   "DLG_WRITE_SECTORS_DUTCH",
   "DLG_WRITE_SECTORS_DE",
   "DLG_WRITE_SECTORS_FR",
};

/* Fields on sector viewer dialog */
#define IDD_SECTOR_NUM   100
#define IDD_BYTE_OFFSET  101
#define IDD_VSCROLL      102
#define IDD_EDIT_SECTNO  103
#define IDD_HEX_VIEW     200
#define IDD_BTN_WRITE    300
#define IDD_BTN_GOTO     301
#define IDD_BTN_MBRVIEW  302
#define IDD_LBL_GOTO     400

/* Fields on "write sectors" dialog */
#define IDD_START_SECTOR 200
#define IDD_NUM_SECTORS  201
#define IDD_FILENAME     202
#define IDD_BTN_BROWSE   203

#define BYTES_PER_ROW 32
#define BURST_SECTORS (2048*16)

static HINSTANCE hInstDlg;
static UINT  nSector,TotalSectors;
static BYTE  MBR[512];
static BYTE *Sector;

static UINT     nDumpStartSector,nDumpSectors;
static char     DumpFilename[1024];
static ProgInfo prog;

static PSTR pszERROR;

// localization
static PSTR pszSVOFFSET  /* = "Offset = %lu" */ ;
static PSTR pszSVSECTOR  /* = "Sector %lu of %lu" */ ;
static PSTR pszSVDESTFN  /* = "Select destination filename..." */ ;
static PSTR pszSVBINFLT  /* = "Binary files\000*.bin\000\000" */ ;
static PSTR pszSVPEVALSS /* = "Please enter a valid start sector" */ ;
static PSTR pszSVPEVALSW /* = "Please enter a valid number of sectors to write" */ ;
static PSTR pszSVNOSPACE /* = "Not enough free space in target folder" */ ;
static PSTR pszSVINVPATH /* = "Invalid path" */ ;
static PSTR pszSVACCDEN  /* = "Write access was denied" */ ;
static PSTR pszSVWRPROT  /* = "Target drive is write protected" */ ;
static PSTR pszSVOVERWR  /* = "Destination already exists. Are you sure you want to overwrite it?" */ ;
static PSTR pszSVEXISTS  /* = "File Exists" */ ;
static PSTR pszSVLOWMEM  /* = "Low memory! Could not allocate copy buffer!" */ ;
static PSTR pszSVWRSECT  /* = "Writing sectors - please wait..." */ ;
static PSTR pszSVPRCAPT  /* = "Writing sectors..." */ ;
static PSTR pszSVWRERR   /* = "Write error!" */ ;
static PSTR pszSVABORT   /* = "Aborted by user" */ ;
static PSTR pszDLG_WRITE_SECTORS;
static PSTR pszDLG_ALT_SECTOR_VIEWER;

/*.....................................................*/

static BOOL
Error(PSTR pszMsg)
{
   MessageBox(GetFocus(), pszMsg, RSTR(ERROR), MB_ICONEXCLAMATION|MB_OK);
   return FALSE;
}

/*..........................................................*/

static void
GetWindowPos(HWND hWnd, RECT *r)
// Gets a window rect in terms of parent client coordinates.
{
   GetWindowRect(hWnd,r);
   MapWindowPoints(NULL,GetParent(hWnd),(POINT*)r,2);
}

/*..........................................................*/

static void
ResizeWindows(HWND hDlg)
{
   HWND hWndHV  = GetDlgItem(hDlg,IDD_HEX_VIEW);
   HWND hWndCtl;
   LRESULT dims = SendMessage(hWndHV,HVM_GETDIMENSIONS,0,0);
   int cx = LOWORD(dims);
   int cy = HIWORD(dims);
   int w_vscroll,dlgy;
   RECT r,rcCtl;

   GetWindowPos(hWndHV,&r);
   r.right  = r.left + cx+2;
   r.bottom = r.top  + cy*16+2;
   SetWindowPos(hWndHV,NULL,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOREDRAW);

   hWndCtl = GetDlgItem(hDlg,IDD_VSCROLL);
   GetClientRect(hWndCtl,&rcCtl);
   w_vscroll = rcCtl.right;
   SetWindowPos(hWndCtl,NULL,r.right-1,r.top,w_vscroll,r.bottom-r.top,SWP_NOZORDER|SWP_NOREDRAW);
   w_vscroll--;

   hWndCtl = GetDlgItem(hDlg,IDD_BTN_WRITE);
   GetWindowPos(hWndCtl,&rcCtl);
   dlgy = (rcCtl.bottom - rcCtl.top + 7)/14;
   SetWindowPos(hWndCtl,NULL,rcCtl.left,r.bottom+dlgy*8,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOREDRAW);

   hWndCtl = GetDlgItem(hDlg,IDD_LBL_GOTO);
   GetWindowPos(hWndCtl,&rcCtl);
   SetWindowPos(hWndCtl,NULL,rcCtl.left,r.bottom+dlgy*11,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOREDRAW);

   hWndCtl = GetDlgItem(hDlg,IDD_EDIT_SECTNO);
   GetWindowPos(hWndCtl,&rcCtl);
   SetWindowPos(hWndCtl,NULL,rcCtl.left,r.bottom+dlgy*9,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOREDRAW);

   hWndCtl = GetDlgItem(hDlg,IDD_BTN_GOTO);
   GetWindowPos(hWndCtl,&rcCtl);
   SetWindowPos(hWndCtl,NULL,rcCtl.left,r.bottom+dlgy*8,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOREDRAW);

   hWndCtl = GetDlgItem(hDlg,IDD_BTN_MBRVIEW);
   GetWindowPos(hWndCtl,&rcCtl);
   SetWindowPos(hWndCtl,NULL,rcCtl.left,r.bottom+dlgy*8,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOREDRAW);

   hWndCtl = GetDlgItem(hDlg,IDOK);
   GetWindowPos(hWndCtl,&rcCtl);
   SetWindowPos(hWndCtl,NULL,r.right+w_vscroll+rcCtl.left-rcCtl.right,r.bottom+dlgy*8,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOREDRAW);

   // finally, resize the dialog box itself.
   GetClientRect(hDlg,&rcCtl);
   rcCtl.right  = r.right  + w_vscroll + 9;
   rcCtl.bottom = r.bottom + dlgy*28;
   AdjustWindowRect(&rcCtl,GetWindowLong(hDlg,GWL_STYLE),FALSE);
   SetWindowPos(hDlg,NULL,0,0,rcCtl.right-rcCtl.left,rcCtl.bottom-rcCtl.top,SWP_NOMOVE|SWP_NOZORDER);
}

/*..........................................................*/

static void
InitDialog(HWND hDlg)
{
   ResizeWindows(hDlg);
   Sector = (BYTE*)SendDlgItemMessage(hDlg,IDD_HEX_VIEW,HVM_GETBUFFPTR,0,0);
   CopyMemory(Sector,MBR,512);
   SetScrollRange(GetDlgItem(hDlg,IDD_VSCROLL),SB_CTL,0,100,FALSE);
   SetScrollPos(GetDlgItem(hDlg,IDD_VSCROLL),SB_CTL,50,FALSE);
   SetDlgItemInt(hDlg,IDD_EDIT_SECTNO,0,FALSE);
}

/*.....................................................*/

static void
ShowOffset(HWND hDlg)
{
   UINT iCaret = SendDlgItemMessage(hDlg,IDD_HEX_VIEW,HVM_GETOFFSET,0,0);
   char sz[32];
   wsprintf(sz,RSTR(SVOFFSET),iCaret);
   SetDlgItemText(hDlg,IDD_BYTE_OFFSET,sz);
}

/*.....................................................*/

static void
ShowSector(HWND hDlg)
{
   char sz[256];
   wsprintf(sz,RSTR(SVSECTOR),nSector,TotalSectors);
   SetDlgItemText(hDlg,IDD_SECTOR_NUM,sz);
   SendDlgItemMessage(hDlg,IDD_HEX_VIEW,HVM_REPAINT,0,0);
}

/*.....................................................*/

static void
GotoSector(HWND hDlg, HVDDR hVDI, UINT nNewSector)
{
   if (nNewSector!=nSector) {
      nSector = nNewSector;
      hVDI->ReadSectors(hVDI, Sector, nSector, 1);
      ShowSector(hDlg);
   }
}

/*.....................................................*/

static BOOL
BrowseFiles(HWND hWndParent, PSTR fnDflt)
{
   OPENFILENAME ofn;
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner   = hWndParent;
   ofn.lpstrCustomFilter = NULL;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = fnDflt;
   ofn.nMaxFile = MAX_PATH;
   ofn.lpstrFileTitle = NULL;
   ofn.lpstrInitialDir = NULL;
   ofn.lpstrDefExt = NULL;
   ofn.nFileOffset = ofn.nFileExtension = 0;
   ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
   ofn.lpstrTitle  = RSTR(SVDESTFN);
   ofn.lpstrFilter = RSTR(SVBINFLT);
   return GetSaveFileName(&ofn);
}

/*....................................................*/

static BOOL
GuessNumSectors(UINT nStartSector, UINT *guess)
{
   if (nStartSector==0) {
      *guess = TotalSectors;
      return TRUE;
   } else {
      PPART pPart = (PPART)(MBR+446);
      int i;
      for (i=0; i<4; i++) {
         if (pPart->loStartLBA==(nStartSector & 0xFFFF) &&
            pPart->hiStartLBA==((nStartSector>>16) & 0xFFFF)) {
            *guess = (UINT)MAKELONG(pPart->loNumSectors,pPart->hiNumSectors);
            return TRUE;
         }
         pPart++;
      }
   }
   return FALSE;
}

/*....................................................*/

BOOL CALLBACK
WriteSectorsDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         UINT nSectorsToCopy;
         Profile_GetString(szfnDump,DumpFilename,1024,"");
         SetDlgItemInt(hDlg,IDD_START_SECTOR,nSector,FALSE);
         if (!GuessNumSectors(nSector,&nSectorsToCopy)) nSectorsToCopy = 1;
         SetDlgItemInt(hDlg,IDD_NUM_SECTORS,nSectorsToCopy,FALSE);
         SetDlgItemText(hDlg,IDD_FILENAME,DumpFilename);
         break;
      }
      case WM_COMMAND: {
         switch (LOWORD(wParam)) {
            case IDD_START_SECTOR:
               if (HIWORD(wParam)==EN_CHANGE) {
                  BOOL bOk;
                  UINT nSectorsToCopy,nStartSector = GetDlgItemInt(hDlg,IDD_START_SECTOR,&bOk,FALSE);
                  if (bOk && GuessNumSectors(nStartSector,&nSectorsToCopy)) {
                     SetDlgItemInt(hDlg,IDD_NUM_SECTORS,nSectorsToCopy,FALSE);
                  }
               }
               break;
            case IDD_BTN_BROWSE:
               if (BrowseFiles(hDlg,DumpFilename)) {
                  SetDlgItemText(hDlg,IDD_FILENAME,DumpFilename);
               }
               break;
            case IDOK: {
               BOOL bOk;
               nDumpStartSector = GetDlgItemInt(hDlg,IDD_START_SECTOR,&bOk,FALSE);
               if (!bOk) Error(RSTR(SVPEVALSS));
               else {
                  nDumpSectors = GetDlgItemInt(hDlg,IDD_NUM_SECTORS,&bOk,FALSE);
                  if (!bOk) Error(RSTR(SVPEVALSW));
                  else {
                     GetDlgItemText(hDlg,IDD_FILENAME,DumpFilename,1024);
                     Profile_SetString(szfnDump,DumpFilename);
                     EndDialog(hDlg, TRUE);
                  }
               }
               break;
            }
            case IDCANCEL:
               EndDialog(hDlg, FALSE);
               break;
            default:
               return FALSE;
         }
         break;
      }
      default:
         return FALSE;
   }
   return TRUE;
}

/*.....................................................*/

static BOOL
CheckDiskSpace(CPFN pfn, UINT nSectors)
{
   ULARGE_INTEGER UserFreeBytes,BytesUsed,TotalFreeBytes;
   FNCHAR path[1024];

   Filename_SplitPath(pfn,path,NULL);
   if (path[0]=='\\' && path[1]=='\\') { // UNC names need a trailing path separator.
      int slen = Filename_Length(path);
      if (path[slen-1]!='\\') {
         path[slen] = '\\';
         path[slen+1] = (FNCHAR)0;
      }
   }

   if (GetDiskFreeSpaceEx(path,&UserFreeBytes,&BytesUsed,&TotalFreeBytes)) {
      HUGE sizerequired,freespace;

      sizerequired = nSectors;
      sizerequired <<= 9;
      freespace = UserFreeBytes.QuadPart;
      hugeop_sub(freespace, freespace, sizerequired); // this calculates free space remaining after cloning.
      if (HI32(freespace)&0x80000000) { // if freespace went negative...
         Error(RSTR(SVNOSPACE));
         return FALSE;
      }
   } else {
      Error(RSTR(SVINVPATH));
      return FALSE;
   }
   return TRUE;
}

/*.....................................................*/

static BOOL
DoDump(HWND hDlg, HVDDR hVDI)
{
   FILE f;
   BYTE *buffer;
   BOOL bSuccess = FALSE;

   if (!CheckDiskSpace(DumpFilename,nDumpSectors)) return FALSE;

   f = CreateFile(DumpFilename,GENERIC_WRITE,0,0,CREATE_NEW,FILE_FLAG_WRITE_THROUGH|FILE_FLAG_SEQUENTIAL_SCAN,0);
_try_again:
   if (f == NULLFILE) {
      UINT IOR = File_IOresult();
      if (IOR == ERROR_ACCESS_DENIED) return Error(RSTR(SVACCDEN));
      else if (IOR == ERROR_WRITE_PROTECT) return Error(RSTR(SVWRPROT));
      else {
         if (MessageBox(GetFocus(),RSTR(SVOVERWR), RSTR(SVEXISTS), MB_ICONSTOP|MB_YESNO)==IDYES) {
            f = CreateFile(DumpFilename,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH|FILE_FLAG_SEQUENTIAL_SCAN,0);
            goto _try_again;
         }
      }
   }
   if (f!=NULLFILE) {
      UINT BurstLen,SectorsDone=0;
      int rslt;

      buffer = Mem_Alloc(0,BURST_SECTORS*512);
      if (!buffer) Error(RSTR(SVLOWMEM));
      else {
         HUGE LBA = nDumpStartSector;

         // init progress stats and show progress window.
         // -- only show progress if this is going to take a lot of time.
         FillMemory(&prog, sizeof(prog), 0);
         prog.pszFn = DumpFilename;
         prog.pszMsg = RSTR(SVWRSECT);
         prog.pszCaption = RSTR(SVPRCAPT);
         prog.BytesTotal = nDumpSectors*512.0;
         Progress.Begin(hInstDlg, hDlg, &prog);
         Progress.UpdateStats(&prog);

         while (nDumpSectors) {
            BurstLen = (nDumpSectors>=BURST_SECTORS ? BURST_SECTORS : nDumpSectors);
            rslt = hVDI->ReadSectors(hVDI,buffer,LBA,BurstLen);
            if (rslt == VDDR_RSLT_FAIL) goto _err_abort;
            LBA += BurstLen;
            SectorsDone += BurstLen;
            nDumpSectors -= BurstLen;
            BurstLen <<= 9;
            if (File_WrBin(f,buffer,BurstLen)!=BurstLen) {
               Error(RSTR(SVWRERR));
               goto _err_abort;
            }
            prog.BytesDone = SectorsDone*512.0;
            Progress.UpdateStats(&prog);
            if (prog.bUserCancel) {
               Error(RSTR(SVABORT));
               goto _err_abort;
            }
         }
         bSuccess = TRUE;
_err_abort:
         if (bSuccess) PlaySound("notify.wav", NULL, SND_FILENAME);
         Progress.End(&prog);
         Mem_Free(buffer);
      }
      File_Close(f);
      if (!bSuccess) File_Erase(DumpFilename);
   }
   return bSuccess;
}

/*.....................................................*/

BOOL CALLBACK
SectorViewerDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   static HVDDR hVDI;
   switch (iMsg) {
      case WM_INITDIALOG:
         hVDI = (HVDDR)lParam;
         InitDialog(hDlg);
         ShowSector(hDlg);
         ShowOffset(hDlg);
         break;
      case WM_VSCROLL: {
         UINT nNextSector = nSector;
         UINT wNotify = LOWORD(wParam);
         if (wNotify==SB_LINEUP) {
            if (nSector>0) nNextSector--;
         } else if (wNotify==SB_PAGEUP) {
            if (nSector>=63) nNextSector-=63;
            else nNextSector = 0;
         } else if (wNotify==SB_LINEDOWN) {
            nNextSector++;
            if (nNextSector==TotalSectors) nNextSector--;
         } else if (wNotify==SB_PAGEDOWN) {
            nNextSector+=63;
            if (nNextSector>=TotalSectors) nNextSector = TotalSectors-1;
         }
         GotoSector(hDlg,hVDI,nNextSector);
         break;
      }
      case WM_COMMAND:
         switch (LOWORD(wParam)) {
            case IDD_HEX_VIEW: {
               UINT wNotify = HIWORD(wParam);
               if (wNotify==HVN_OFFSET_CHANGED) {
                  ShowOffset(hDlg);
               } else {
                  UINT nNextSector = nSector;
                  if (wNotify==HVN_PAGEUP) {
                     if (nSector>=63) nNextSector-=63;
                     else nNextSector = 0;
                  } else if (wNotify==HVN_PAGEDOWN) {
                     nNextSector = nSector+63;
                     if (nNextSector>=TotalSectors) nNextSector = TotalSectors-1;
                  } else if (wNotify==HVN_LINEUP) {
                     if (nSector>0) nNextSector--;
                  } else if (wNotify==HVN_LINEDOWN) {
                     nNextSector++;
                     if (nNextSector>=TotalSectors) nNextSector = TotalSectors-1;
                  }
                  if (nNextSector!=nSector) GotoSector(hDlg,hVDI,nNextSector);
               }
               break;
            }
            case IDD_BTN_GOTO: {
               BOOL bOk;
               UINT nNextSector = GetDlgItemInt(hDlg,IDD_EDIT_SECTNO,&bOk,FALSE);
               if (bOk && nNextSector>=TotalSectors) bOk = FALSE;
               if (bOk) GotoSector(hDlg,hVDI,nNextSector);
               else SetDlgItemInt(hDlg,IDD_EDIT_SECTNO,nSector,FALSE);
               SetFocus(GetDlgItem(hDlg,IDD_HEX_VIEW));
               break;
            }
            case IDD_BTN_WRITE:
               if (DialogBox(hInstDlg,RSTR(DLG_WRITE_SECTORS),hDlg,WriteSectorsDlgProc)) {
                  DoDump(hDlg,hVDI);
               }
               break;
            case IDD_BTN_MBRVIEW:
               PartInfo_Show(hInstDlg, hDlg, Sector);
               break;
            case IDOK:
            case IDCANCEL: {
               EndDialog(hDlg, LOWORD(wParam));
               break;
            }
            default:
               return FALSE;
         }
         break;
      default:
         return FALSE;
   }
   return TRUE;
}

/*....................................................*/

PUBLIC void
SectorViewer_Show(HINSTANCE hInstRes, HWND hWndParent, HVDDR hVDI)
{
   if (hVDI) {
      HUGE DriveSize;
      hVDI->GetDriveSize(hVDI, &DriveSize);
      DriveSize >>= 9;
      nSector = 0;
      TotalSectors = LO32(DriveSize);
      hVDI->ReadSectors(hVDI, MBR, 0, 1);
      hInstDlg = hInstRes;
      DialogBoxParam(hInstRes,RSTR(DLG_ALT_SECTOR_VIEWER),hWndParent,SectorViewerDlgProc,(LPARAM)hVDI);
   }
}

/*.....................................................*/

/* end of sectorviewer.c */

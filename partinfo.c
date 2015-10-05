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
#include "partinfo.h"
#include "env.h"
#include "ids.h"

#define IDD_FIRST          200

#define N_COLUMNS          6
#define N_ROWS             4
#define COL_BOOTABLE       0
#define COL_CHS_FIRST      1
#define COL_PART_CODE      2
#define COL_CHS_LAST       3
#define COL_LBA_START      4
#define COL_N_SECTORS      5

static PSTR pszNOYES;
static PSTR pszDLG_PARTINFO;

/*....................................................*/

static void
InitDialog(HWND hDlg, BYTE *MBR)
{
   PartTableEntry *p = (PartTableEntry*)(MBR+446);
   char sz[256];
   int nPart,iCtl;

   for (nPart=0; nPart<N_ROWS; nPart++) {
      iCtl = IDD_FIRST + nPart*N_COLUMNS;

      RSTR(NOYES); // inits pszNOYES
      
      // bootable?
      if (p->Status==0x80) sz[0]=pszNOYES[1];
      else if (p->Status==0x00) sz[0]=pszNOYES[0];
      else sz[0]='?';
      sz[1] = (char)0;
      SetDlgItemText(hDlg,iCtl,sz);

      // CHS address of first sector
      wsprintf(sz,"c=%lu,h=%lu,s=%lu",
         p->pstart_chs_cyl+((p->pstart_chs_sect<<2)&0x300),
         p->pstart_chs_head,
         p->pstart_chs_sect & 0x3F);
      SetDlgItemText(hDlg,iCtl+1,sz);

      // Partition type code
      wsprintf(sz,"0x%02lx",p->PartType);
      SetDlgItemText(hDlg,iCtl+2,sz);

      // CHS address of last sector
      wsprintf(sz,"c=%lu,h=%lu,s=%lu",
         p->pend_chs_cyl+((p->pend_chs_sect<<2)&0x300),
         p->pend_chs_head,
         p->pend_chs_sect & 0x3F);
      SetDlgItemText(hDlg,iCtl+3,sz);

      // LBA address of first sector
      SetDlgItemInt(hDlg,iCtl+4,p->loStartLBA + (p->hiStartLBA<<16),FALSE);

      // Number of sectors.
      SetDlgItemInt(hDlg,iCtl+5,p->loNumSectors + (p->hiNumSectors<<16),FALSE);

      p++;
   }
}

/*....................................................*/

static BOOL
IsMBR(BYTE *MBR)
{
   PPART pPart = (PPART)(MBR+440);
   UINT i;
   if (MBR[510]!=0x55 || MBR[511]!=0xAA) return FALSE;
   for (i=0; i<4; i++) {
      // TODO: maybe add more checks.
      if (pPart->Status!=0 && pPart->Status==0x80) return FALSE;
      pPart++;
   }
   return TRUE;
}

/*....................................................*/

BOOL CALLBACK
PartitionInfoDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   static BYTE MBR[512];

   switch (iMsg) {
      case WM_INITDIALOG:
         CopyMemory(MBR, (BYTE*)lParam, 512);
         if (!IsMBR(MBR)) FillMemory(MBR,512,0);
         InitDialog(hDlg,MBR);
         break;
      case WM_COMMAND:
         switch (LOWORD(wParam)) {
            case IDOK:
            case IDCANCEL:
               EndDialog(hDlg, LOWORD(wParam));
               break;
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
PartInfo_Show(HINSTANCE hInstRes, HWND hWndParent, BYTE *MBR)
{
   DialogBoxParam(hInstRes,RSTR(DLG_PARTINFO),hWndParent,PartitionInfoDlgProc,(LPARAM)MBR);
}

/*.....................................................*/

/* end of partinfo.c */



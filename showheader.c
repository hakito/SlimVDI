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
#include "showheader.h"
#include "mem.h"
#include "vdir.h"
#include "vhdr.h"
#include "vmdkr.h"
#include "hddr.h"
#include "env.h"
#include "ids.h"

/* Fields on VDI Header Info dialog */
#define IDD_INFO_STRING       200
#define IDD_SIGNATURE         201
#define IDD_FORMAT_VER        202
#define IDD_VDI_TYPE          203
#define IDD_VDI_FLAGS         204
#define IDD_LEGACY_GEOMETRY   205
#define IDD_DISK_SIZE         206
#define IDD_BLOCK_SIZE        207
#define IDD_BLOCK_EXTRA       208
#define IDD_BLOCKS_TOTAL      209
#define IDD_BLOCKS_ALLOCATED  210
#define IDD_UUID_CREATE       211
#define IDD_UUID_MODIFY       212
#define IDD_UUID_LINKAGE      213
#define IDD_UUID_PMODIFY      214
#define IDD_LCHS_GEOMETRY     215
#define IDD_COMMENT           216

/* Fields on VHD Header Info dialog */
#define IDD_VHD_COOKIE        200
#define IDD_VHD_FEATURES      201
#define IDD_VHD_FTR_VER       202
#define IDD_VHD_CREATOR_APP   203
#define IDD_VHD_CREATOR_OS    204
#define IDD_VHD_DISK_SIZE     205
#define IDD_VHD_TYPE          206
#define IDD_VHD_UUID_CREATE   207
#define IDD_VHD_SAVED_STATE   208
#define IDD_VHD_BLOCKS_TOTAL  209
#define IDD_VHD_BLOCKS_ALLOC  210
#define IDD_VHD_DHDR_VER      211
#define IDD_VHD_BLOCK_SIZE    212
#define IDD_VHD_UUID_PARENT   213

/* Fields on VMDK Header Info dialog */
#define IDD_VMDK_VERSION      200
#define IDD_VMDK_CID          201
#define IDD_VMDK_PARENTCID    202
#define IDD_VMDK_DISKTYPE     203
#define IDD_VMDK_UUID         204
#define IDD_VMDK_NEXTENTS     205
#define IDD_VMDK_TABLE_FIRST  206
#define IDD_VMDK_TABLE_LAST   270
#define IDD_VMDK_TABLE_COLS   8

typedef struct {
   VDI_PREHEADER  phdr;
   VDI_HEADER     hdr;
} VDI_UNI_HDR;

typedef struct {
   VHD_FOOTER ftr;
   VHD_DYN_HEADER dhdr;
} VHD_UNI_HDR;

//static VDI_UNI_HDR    vdihdr;
//static VHD_FOOTER     ftr;
//static VHD_DYN_HEADER dhdr;

#define N_VDI_TYPES 4
static PSTR szVdiType[N_VDI_TYPES] = {
   NULL, /* "Dynamic" */
   NULL, /* "Fixed" */
   NULL, /* "Undo" */
   NULL  /* "Differencing" */
};

// localized strings
static PSTR pszUNKNOWNFS    /* = "Unknown (%lu)" */;
static PSTR pszDYNAMIC      /* = "Dynamic" */;
static PSTR pszFIXED        /* = "Fixed" */;
static PSTR pszDIFFERENCING /* = "Differencing" */;
static PSTR pszSET          /* = "Set" */;
static PSTR pszNOTSET       /* = "Not set" */;
static PSTR pszNOTSPEC      /* = "Not specified" */;
static PSTR pszACCTYPE0     /* = "?" */;
static PSTR pszACCTYPE1     /* = "RW" */;
static PSTR pszACCTYPE2     /* = "RdOnly" */;
static PSTR pszACCTYPE3     /* = "None" */;
static PSTR pszEXTTYPE0     /* = "?" */;
static PSTR pszEXTTYPE1     /* = "Flat" */;
static PSTR pszEXTTYPE2     /* = "Sparse" */;
static PSTR pszEXTTYPE3     /* = "Zero" */;
static PSTR pszDLG_VDI_HDR_INFO;
static PSTR pszDLG_VHD_HDR_INFO;
static PSTR pszDLG_HDD_HDR_INFO;
static PSTR pszDLG_VMDK_HDR_INFO;

/*.....................................................*/

static void
FormatDriveSize(PSTR sz, HUGE drivesize)
{
   UINT GB = (UINT)(drivesize>>30);
   UINT MB = (UINT)((drivesize&0x3FFFFFFF)>>20);
   UINT R  = (UINT)(drivesize & 0xFFFFF);
   if (GB) { // if drive is greater than 1GB.
       Env_DoubleToString(sz,GB+MB/1024.0,2);
       lstrcat(sz," GB");
   } else {
       Env_DoubleToString(sz,MB+R/1024.0,2);
       lstrcat(sz," MB");
   }
}

/*.....................................................*/

static void
SetDlgUUID(HWND hDlg, UINT idCtl, S_UUID *pUUID)
{
   char szbuff[64];
   Env_FormatUUID(szbuff,pUUID);
   SetDlgItemText(hDlg,idCtl,szbuff);
}

/*....................................................*/

static void
CopySig(PSTR sz, BYTE *sig)
{
   UINT i,c;
   for (i=0; i<4; i++) {
      c = *sig++;
      if (c<' ' || c>'~') c=' ';
      sz[i] = (char)c;
   }
}

/*....................................................*/

BOOL CALLBACK
VDIHeaderInfoDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         char sz[512];
         VDI_UNI_HDR *vdihdr = (VDI_UNI_HDR*)lParam;
         SetDlgItemText(hDlg,IDD_INFO_STRING,vdihdr->phdr.szFileInfo);
         wsprintf(sz,"0x%08lX",vdihdr->phdr.u32Signature);
         SetDlgItemText(hDlg,IDD_SIGNATURE,sz);
         wsprintf(sz,"%lu.%02lu",vdihdr->phdr.u32Version>>16,vdihdr->phdr.u32Version & 0xFFFF);
         SetDlgItemText(hDlg,IDD_FORMAT_VER,sz);

         if (vdihdr->hdr.vdi_type>0 && vdihdr->hdr.vdi_type<=N_VDI_TYPES) {
            int i = (vdihdr->hdr.vdi_type-1);
            SetDlgItemText(hDlg,IDD_VDI_TYPE,Env_LoadString(szVdiType+i,IDS_DYNAMIC+i));
         } else {
            wsprintf(sz,RSTR(UNKNOWNFS),vdihdr->hdr.vdi_type);
            SetDlgItemText(hDlg,IDD_VDI_TYPE,sz);
         }
         wsprintf(sz,"0x%08lX",vdihdr->hdr.vdi_flags);
         SetDlgItemText(hDlg,IDD_VDI_FLAGS,sz);

         wsprintf(sz,"(C=%lu, H=%lu, SPT=%lu, BPS=%lu)",
                  vdihdr->hdr.LegacyGeometry.cCylinders,
                  vdihdr->hdr.LegacyGeometry.cHeads,
                  vdihdr->hdr.LegacyGeometry.cSectorsPerTrack,
                  vdihdr->hdr.LegacyGeometry.cBytesPerSector);
         SetDlgItemText(hDlg,IDD_LEGACY_GEOMETRY,sz);

         FormatDriveSize(sz,vdihdr->hdr.DiskSize);
         SetDlgItemText(hDlg,IDD_DISK_SIZE,sz);

         SetDlgItemInt(hDlg,IDD_BLOCK_SIZE,vdihdr->hdr.BlockSize,FALSE);
         SetDlgItemInt(hDlg,IDD_BLOCK_EXTRA,vdihdr->hdr.cbBlockExtra,FALSE);
         SetDlgItemInt(hDlg,IDD_BLOCKS_TOTAL,vdihdr->hdr.nBlocks,FALSE);
         SetDlgItemInt(hDlg,IDD_BLOCKS_ALLOCATED,vdihdr->hdr.nBlocksAllocated,FALSE);

         SetDlgUUID(hDlg,IDD_UUID_CREATE,&vdihdr->hdr.uuidCreate);
         SetDlgUUID(hDlg,IDD_UUID_MODIFY,&vdihdr->hdr.uuidModify);
         SetDlgUUID(hDlg,IDD_UUID_LINKAGE,&vdihdr->hdr.uuidLinkage);
         SetDlgUUID(hDlg,IDD_UUID_PMODIFY,&vdihdr->hdr.uuidParentModify);

         wsprintf(sz,"(C=%lu, H=%lu, SPT=%lu, BPS=%lu)",
                  vdihdr->hdr.LCHSGeometry.cCylinders,
                  vdihdr->hdr.LCHSGeometry.cHeads,
                  vdihdr->hdr.LCHSGeometry.cSectorsPerTrack,
                  vdihdr->hdr.LCHSGeometry.cBytesPerSector);
         SetDlgItemText(hDlg,IDD_LCHS_GEOMETRY,sz);

         SetDlgItemText(hDlg,IDD_COMMENT,vdihdr->hdr.vdi_comment);
         break;
      }
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

BOOL CALLBACK
VHDHeaderInfoDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         char sz[512];
         VHD_UNI_HDR *pVhdHdr = (VHD_UNI_HDR*)lParam;
         CopyMemory(sz,pVhdHdr->ftr.cookie,8); sz[8]=(char)0;
         SetDlgItemText(hDlg,IDD_VHD_COOKIE,sz);
         wsprintf(sz,"%08lX",pVhdHdr->ftr.u32Features);
         SetDlgItemText(hDlg,IDD_VHD_FEATURES,sz);
         wsprintf(sz,"v%lu.%02lu",pVhdHdr->ftr.u32FmtVer>>16,pVhdHdr->ftr.u32FmtVer & 0xFFFF);
         SetDlgItemText(hDlg,IDD_VHD_FTR_VER,sz);
         wsprintf(sz,"'xxxx'");
         CopySig(sz+1,pVhdHdr->ftr.u32CreatorApp);
         SetDlgItemText(hDlg,IDD_VHD_CREATOR_APP,sz);
         wsprintf(sz,"'xxxx'");
         CopySig(sz+1,pVhdHdr->ftr.u32CreatorOS);
         SetDlgItemText(hDlg,IDD_VHD_CREATOR_OS,sz);
         FormatDriveSize(sz,pVhdHdr->ftr.u64CurrentSize);
         SetDlgItemText(hDlg,IDD_VHD_DISK_SIZE,sz);
         if (pVhdHdr->ftr.u32DiskType == VHD_TYPE_FIXED) SetDlgItemText(hDlg,IDD_VHD_TYPE,RSTR(FIXED));
         else if (pVhdHdr->ftr.u32DiskType == VHD_TYPE_DYNAMIC) SetDlgItemText(hDlg,IDD_VHD_TYPE,RSTR(DYNAMIC));
         else if (pVhdHdr->ftr.u32DiskType == VHD_TYPE_DIFFERENCING) SetDlgItemText(hDlg,IDD_VHD_TYPE,RSTR(DIFFERENCING));
         else {
            wsprintf(sz,RSTR(UNKNOWNFS),pVhdHdr->ftr.u32DiskType);
            SetDlgItemText(hDlg,IDD_VHD_TYPE,sz);
         }
         SetDlgUUID(hDlg,IDD_VHD_UUID_CREATE,&pVhdHdr->ftr.uuidCreate);
         if (pVhdHdr->ftr.u8SavedState) wsprintf(sz,RSTR(SET));
         else wsprintf(sz,RSTR(NOTSET));
         SetDlgItemText(hDlg,IDD_VHD_SAVED_STATE,sz);

         SetDlgItemInt(hDlg,IDD_VHD_BLOCKS_TOTAL,pVhdHdr->dhdr.u32BlockCount,FALSE);
         SetDlgItemInt(hDlg,IDD_VHD_BLOCKS_ALLOC,pVhdHdr->dhdr.u32Reserved,FALSE);

         if (pVhdHdr->ftr.u32DiskType == VHD_TYPE_DYNAMIC || pVhdHdr->ftr.u32DiskType == VHD_TYPE_DIFFERENCING) {
            wsprintf(sz,"v%lu.%02lu",pVhdHdr->dhdr.u32HeaderVer>>16,pVhdHdr->dhdr.u32HeaderVer & 0xFFFF);
            SetDlgItemText(hDlg,IDD_VHD_DHDR_VER,sz);
            SetDlgItemInt(hDlg,IDD_VHD_BLOCK_SIZE,pVhdHdr->dhdr.u32BlockSize,FALSE);
            SetDlgUUID(hDlg,IDD_VHD_UUID_PARENT,&pVhdHdr->dhdr.uuidParent);
         } else {
            SetDlgItemText(hDlg,IDD_VHD_DHDR_VER,"");
            SetDlgItemText(hDlg,IDD_VHD_BLOCK_SIZE,"");
            SetDlgItemText(hDlg,IDD_VHD_UUID_PARENT,"");
         }
         break;
      }
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

#define N_VMDK_CREATE_TYPES 6
static PSTR szCreateTypes[N_VMDK_CREATE_TYPES] = {
   "monolithicSparse",
   "monolithicFlat",
   "twoGbMaxExtentSparse",
   "twoGbMaxExtentFlat",
   "fullDevice",
   "partitionedDevice",
};

#define N_VMDK_ACCESS_TYPES 4
static PSTR szAccessTypes[N_VMDK_ACCESS_TYPES] = {
   NULL, /* "?" */
   NULL, /* "RW" */
   NULL, /* "RdOnly" */
   NULL, /* "None" */
};

#define N_VMDK_EXTENT_TYPES 4
static PSTR szExtentTypes[N_VMDK_ACCESS_TYPES] = {
   NULL, /* "?" */
   NULL, /* "Flat" */
   NULL, /* "Sparse" */
   NULL, /* "Zero" */
};

BOOL CALLBACK
VMDKHeaderInfoDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         PVMDKED pE;
         int  i,iRow;
         char sz[512];
         VMDK_HEADER *pVdk = (VMDK_HEADER*)lParam;
         SetDlgItemInt(hDlg,IDD_VMDK_VERSION,pVdk->version,FALSE);
         wsprintf(sz,"0x%08lx",pVdk->CID);
         SetDlgItemText(hDlg,IDD_VMDK_CID,sz);
         wsprintf(sz,"0x%08lx",pVdk->CIDParent);
         SetDlgItemText(hDlg,IDD_VMDK_PARENTCID,sz);
         if (pVdk->CreateType<=N_VMDK_CREATE_TYPES) {
            PSTR psz;
            if (pVdk->CreateType) psz = szCreateTypes[pVdk->CreateType-1];
            else psz = RSTR(NOTSPEC);
            SetDlgItemText(hDlg,IDD_VMDK_DISKTYPE,psz);
         } else {
            wsprintf(sz,RSTR(UNKNOWNFS),pVdk->CreateType);
            SetDlgItemText(hDlg,IDD_VMDK_DISKTYPE,sz);
         }
         SetDlgUUID(hDlg,IDD_VMDK_UUID,&pVdk->uuidCreate);
         SetDlgItemInt(hDlg,IDD_VMDK_NEXTENTS,pVdk->nExtents,FALSE);
         iRow = IDD_VMDK_TABLE_FIRST;
         for (i=0; i<pVdk->nExtents; i++) {
            pE = pVdk->extdes+i;
            if (pE->iomode<N_VMDK_ACCESS_TYPES) SetDlgItemText(hDlg,iRow,Env_LoadString(szAccessTypes+pE->iomode,IDS_ACCTYPE0+pE->iomode));
            else SetDlgItemText(hDlg,iRow,"?");
            if (pE->type<N_VMDK_EXTENT_TYPES) SetDlgItemText(hDlg,iRow+1,Env_LoadString(szExtentTypes+pE->type,IDS_EXTTYPE0+pE->type));
            else SetDlgItemText(hDlg,iRow+1,"?");
            SetDlgItemInt(hDlg,iRow+2,pE->sectors,FALSE);
            SetDlgItemInt(hDlg,iRow+3,pE->offset,FALSE);
            SetDlgItemInt(hDlg,iRow+4,pE->BlkSize,FALSE);
            SetDlgItemInt(hDlg,iRow+5,pE->nBlocks,FALSE);
            SetDlgItemInt(hDlg,iRow+6,pE->nAlloc,FALSE);
            SetDlgItemText(hDlg,iRow+7,pE->fn);
            iRow += IDD_VMDK_TABLE_COLS;
            if (iRow==IDD_VMDK_TABLE_LAST) break;
         }
         for (; iRow<IDD_VMDK_TABLE_LAST; iRow++) SetDlgItemText(hDlg,iRow,"");
         break;
      }
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

BOOL CALLBACK
HDDHeaderInfoDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         char sz[512];
         HDD_HEADER *pHddHdr = (HDD_HEADER*)lParam;
         HUGE DriveSize;
         CopyMemory(sz,pHddHdr->szSig,16); sz[16]=(char)0;
         SetDlgItemText(hDlg,200,sz);
         wsprintf(sz,"v%lu.%02lu",pHddHdr->u32Version & 0xFFFF,pHddHdr->u32Version>>16);
         SetDlgItemText(hDlg,201,sz);
         wsprintf(sz,"CHS=(%lu,%lu,63)",pHddHdr->u32Cylinders,pHddHdr->u32Heads);
         SetDlgItemText(hDlg,202,sz);
         SetDlgItemInt(hDlg,203,pHddHdr->u32BlockSize<<9,FALSE);
         SetDlgItemInt(hDlg,204,pHddHdr->nBlocks,FALSE);
         DriveSize = pHddHdr->DriveSize;
         FormatDriveSize(sz,DriveSize<<9);
         SetDlgItemText(hDlg,205,sz);
         break;
      }
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
ShowHeader_Show(HINSTANCE hInstRes, HWND hWndParent, HVDDR hVDI)
{
   if (hVDI) {
      switch (hVDI->GetDriveType(hVDI)) {
         case VDD_TYPE_VDI: {
            VDI_UNI_HDR *pHdr = Mem_Alloc(0,sizeof(VDI_UNI_HDR));
            if (VDIR_GetHeader(hVDI,&pHdr->phdr,&pHdr->hdr)) {
               DialogBoxParam(hInstRes,RSTR(DLG_VDI_HDR_INFO),hWndParent,VDIHeaderInfoDlgProc,(LPARAM)pHdr);
            }
            Mem_Free(pHdr);
            break;
         }
         case VDD_TYPE_VHD: {
            VHD_UNI_HDR *pVhdHdr = Mem_Alloc(0,sizeof(VHD_UNI_HDR));
            if (VHDR_GetHeader(hVDI,&pVhdHdr->ftr,&pVhdHdr->dhdr)) {
               DialogBoxParam(hInstRes,RSTR(DLG_VHD_HDR_INFO),hWndParent,VHDHeaderInfoDlgProc,(LPARAM)pVhdHdr);
            }
            Mem_Free(pVhdHdr);
            break;
         }
         case VDD_TYPE_VMDK:
         case VDD_TYPE_RAW:
         case VDD_TYPE_PART_RAW: {
            VMDK_HEADER *pVdk = Mem_Alloc(0,sizeof(VMDK_HEADER));
            if (VMDKR_GetHeader(hVDI,pVdk)) {
               DialogBoxParam(hInstRes,RSTR(DLG_VMDK_HDR_INFO),hWndParent,VMDKHeaderInfoDlgProc,(LPARAM)pVdk);
            }
            Mem_Free(pVdk);
            break;
         }
         case VDD_TYPE_PARALLELS: {
            HDD_HEADER *pHddHdr = Mem_Alloc(0,sizeof(HDD_HEADER));
            if (HDDR_GetHeader(hVDI,pHddHdr)) {
               DialogBoxParam(hInstRes,RSTR(DLG_HDD_HDR_INFO),hWndParent,HDDHeaderInfoDlgProc,(LPARAM)pHddHdr);
            }
            Mem_Free(pHddHdr);
            break;
         }
         default:
            ;
      }
   }
}

/*.....................................................*/

/* end of showheader.c */



/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "vddr.h"
#include "vdir.h"
#include "vhdr.h"
#include "vmdkr.h"
#include "hddr.h"
#include "filename.h"
#include "mem.h"
#include "env.h"
#include "ids.h"
#include "cow.h"
#include "djstring.h"
#include "mediareg.h"

UINT VDDR_LastError;

static PSTR pszUNKERROR /* = "Unknown Error" */;
static PSTR pszOK       /* = "Ok" */;

/*...........................................................................*/

PUBLIC UINT
VDDR_GetLastError(void)
{
   return VDDR_LastError;
}

/*...........................................................................*/

PUBLIC PSTR
VDDR_GetErrorString(UINT nErr)
{
   PSTR pszErr = RSTR(UNKERROR);
   if (nErr==0xFFFFFFFF) nErr = VDDR_LastError;
   if (nErr==0) pszErr = RSTR(OK);
   else {
      switch (nErr>>16) {
         case VDD_TYPE_VDI:
            pszErr = VDIR_GetErrorString(nErr);
            break;
         case VDD_TYPE_VHD:
            pszErr = VHDR_GetErrorString(nErr);
            break;
         case VDD_TYPE_VMDK:
         case VDD_TYPE_RAW:
         case VDD_TYPE_PART_RAW:
            pszErr = VMDKR_GetErrorString(nErr);
            break;
         case VDD_TYPE_PARALLELS:
            pszErr = HDDR_GetErrorString(nErr);
            break;
         case VDD_TYPE_COW:
            pszErr = COW_GetErrorString(nErr);
            break;
		 default:
            ;
      }
   }
   return pszErr;
}

/*...........................................................................*/

PUBLIC BOOL
VDDR_OpenMediaRegistry(CPFN fn)
{
   MediaReg_Discard();
   MediaReg_ScanFolder(fn);
   return FALSE;
}

/*...........................................................................*/

PUBLIC HVDDR
VDDR_Open(CPFN fn, UINT iChain)
{
   HVDDR pObj=NULL;
   if (Mem_Compare(fn,"\\\\.\\",4)==0) { // if UNC name for physical drive
      pObj = VMDKR_OpenRaw(fn, iChain);
   } else if (Filename_IsExtension(fn,"vdi")) {
      pObj = VDIR_Open(fn, iChain);
   } else if (Filename_IsExtension(fn,"vhd")) {
      pObj = VHDR_Open(fn, iChain);
   } else if (Filename_IsExtension(fn,"vmdk")) {
      pObj = VMDKR_Open(fn, iChain);
   } else if (Filename_IsExtension(fn,"raw") || Filename_IsExtension(fn,"img")) {
      pObj = VMDKR_OpenRaw(fn, iChain);
   } else if (Filename_IsExtension(fn,"hdd")) {
      pObj = HDDR_Open(fn, iChain);
   }
   return pObj;
}

/*...........................................................................*/

PUBLIC HVDDR
VDDR_OpenByUUID(S_UUID *pUUID, UINT iChain)
{
   PFN pfn = MediaReg_FilenameFromUUID(pUUID);
   VDDR_LastError = VDIR_ERR_BADFORMAT;
   if (pfn) return VDDR_Open(pfn,iChain);
   return NULL;
}

/*...........................................................................*/

PUBLIC BOOL
VDDR_QuickGetUUID(CPFN fn, S_UUID *UUID)
{
   if (Filename_IsExtension(fn,"vdi")) {
      return VDIR_QuickGetUUID(fn, UUID);
   } else if (Filename_IsExtension(fn,"vmdk")) {
      return VMDKR_QuickGetUUID(fn, UUID);
   } else if (Filename_IsExtension(fn,"vhd")) {
      return VHDR_QuickGetUUID(fn, UUID);
   }
   return FALSE;
}

/*...........................................................................*/

/* end of vddr.c */


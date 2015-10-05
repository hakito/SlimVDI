/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VDIR_H
#define VDIR_H

/*==========================================================================*/
/* Module which lets me read from VirtualBox Virtual Drive (VDI) files.     */
/*--------------------------------------------------------------------------*/
/* This module actually implements a subclass of the VDDR object class (see */
/* vddr.h). However I also define a full interface for this module so it    */
/* could be made stand-alone.                                               */
/*==========================================================================*/

#include "vddr.h"
#include "vdistructs.h"

// error codes
#define VDIR_ERR_NONE         0
#define VDIR_ERR_FILENOTFOUND ((VDD_TYPE_VDI<<16)+1)
#define VDIR_ERR_READ         ((VDD_TYPE_VDI<<16)+2)
#define VDIR_ERR_NOTVDI       ((VDD_TYPE_VDI<<16)+3)
#define VDIR_ERR_INVHANDLE    ((VDD_TYPE_VDI<<16)+4)
#define VDIR_ERR_OUTOFMEM     ((VDD_TYPE_VDI<<16)+5)
#define VDIR_ERR_INVBLOCK     ((VDD_TYPE_VDI<<16)+6)
#define VDIR_ERR_BADFORMAT    ((VDD_TYPE_VDI<<16)+7)
#define VDIR_ERR_SEEK         ((VDD_TYPE_VDI<<16)+8)
#define VDIR_ERR_BLOCKMAP     ((VDD_TYPE_VDI<<16)+9)
#define VDIR_ERR_SHARE        ((VDD_TYPE_VDI<<16)+10)

UINT VDIR_GetLastError(void);
/* All of the functions in this module set an error code to provide
 * information on any failure. This function can be called to retrieve
 * the error code.
 */

PSTR VDIR_GetErrorString(UINT nErr);
/* This function can use to convert an error code into an a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text version of the last error
 * (this will return NULL if lasterror is 0).
 */

HVDDR VDIR_Open(CPFN fn, UINT iChain);
/* Opens a VDI file, creating internal data structures that allow us to access
 * the contents efficiently. This module always requests exclusive (nonshared) file
 * access, since we can't have VBox modifying the VDI while we are using it.
 *
 * Returns handle to internal data structures if successful. Success indicates
 * that the file was found and had a recognized header. VDIR_GetLastErrorString
 * should be called even after success, as it may indicate a non-fatal error,
 * such as an inconsistency between the file size and the size indicated
 * by the header.
 */

BOOL VDIR_QuickGetUUID(CPFN fn, S_UUID *UUID);

UINT VDIR_GetDriveType(HVDDR pThis); // returns VDD_TYPE_VDI
BOOL VDIR_GetDriveSize(HVDDR pThis, HUGE *drive_size);
UINT VDIR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift);
UINT VDIR_AllocatedBlocks(HVDDR pThis, UINT SPBshift);
UINT VDIR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end);
BOOL VDIR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid);
BOOL VDIR_IsSnapshot(HVDDR pThis);
/* These functions return various stats for the source VDI */

BOOL VDIR_GetHeader(HVDDR pThis, VDI_PREHEADER *vdiph, VDI_HEADER *vdih);
/* -- Note not part of the VDDR class.
 * This function provides direct access to the private pre-header or header structures from
 * the source VDI. This is intended to be used for diagnostic features, such as by the ShowHeader module.
 * Otherwise these are private structures which should not be referenced directly by other modules. If
 * real working code needs to know something about the source disk it should call one of the basic
 * functions such as GetBlockSize()etc. Either of the vdiph or vdih arguments may be NULL if you don't
 * need that item.
 */

int  VDIR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift);
/* Reads one page (block) from the source VDI. This function returns one of the
 * VDDR_RSLT_xxxx codes listed in vddr.h. SPBshift is the requested page size,
 * in SECTORS, expressed as a shift amount.
 */

int  VDIR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors);
/* Reads zero or more sectors from the drive using absolute LBA addressing ("absolute" meaning that
 * sector numbers are relative to the drive, not relative to a partition).
 * This function also returns one of the VDDR_RSLT_xxxx codes.
 */

HVDDR VDIR_Close(HVDDR pThis);
/* Closes a previously opened VDI file, discarding the internal structures created
 * when the file was opened.
 */

/*----------------------------------------------------------------------*/

#endif


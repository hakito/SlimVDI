/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VMDKR_H
#define VMDKR_H

/*==========================================================================*/
/* Module which lets me read from VMWare Virtual Hard Disk (VMDK) files.    */
/*--------------------------------------------------------------------------*/
/* This module actually implements a subclass of the VDDR object class (see */
/* vddr.h). However I also define a full interface for this module so it    */
/* could also be made to work stand-alone.                                  */
/*==========================================================================*/

#include "vddr.h"
#include "vmdkstructs.h"

UINT VMDKR_GetLastError(void);
/* All of the functions in this module set an error code to provide
 * information on any failure. This function can be called to retrieve
 * the error code.
 */

PSTR VMDKR_GetErrorString(UINT nErr);
/* This function can use to convert an error code into an a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text version of the last error
 * (this will return NULL if lasterror is 0).
 */

HVDDR VMDKR_Open(CPFN fn, UINT iChain);
/* Opens a VMDK file, creating internal data structures that allow us to access
 * its contents efficiently. This module always requests exclusive (nonshared) file
 * access, since we can't have VBox or VMWare modifying the VMDK while we are using it.
 *
 * Return handle to internal data structures if successful. Success indicates
 * that the file was found and had a recognized header. VMDKR_GetLastErrorString
 * should be called even after success, as it may indicate a non-fatal error,
 * such as an inconsistency between the file size and the size indicated
 * by the header.
 */

HVDDR VMDKR_OpenRaw(CPFN fn, UINT iChain);
/* Opens a raw disk or partition image file. The VMDK module implements this function
 * because a VMDK FLAT extent IS a raw image, so all this function has to do internally
 * is substitute for the missing VMDK descriptor.
 */

UINT VMDKR_GetDriveType(HVDDR pThis); // returns VDD_TYPE_VMDK
BOOL VMDKR_GetDriveSize(HVDDR pThis, HUGE *drive_size);
UINT VMDKR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift);
UINT VMDKR_AllocatedBlocks(HVDDR pThis, UINT SPBshift);
UINT VMDKR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end);
BOOL VMDKR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid);
BOOL VMDKR_IsSnapshot(HVDDR pThis);
/* Return one of several drive metrics. */

BOOL VMDKR_GetHeader(HVDDR pThis, VMDK_HEADER *vmdkh);
/* -- Note this function is not part of the VDDR class.
 * This function provides direct access to the private header structures from the source VMDK.
 * This is intended to be used for diagnostic features, such as by the ShowHeader module.
 * Otherwise these are private structures which should not be referenced directly by other modules. If
 * real working code needs to know something about the source disk it should call one of the basic
 * functions such as GetBlockSize()etc. Either of the vdiph or vdih arguments may be NULL if you don't
 * need that item.
 */

int  VMDKR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift);
/* Reads one page (block) from the source VMDK. This function returns one of the
 * VMDKR_RSLT_xxxx codes listed above. SPBshift is the requested page size,
 * in SECTORS, expressed as a shift amount.
 */

int  VMDKR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors);
/* Reads zero or more sectors from the drive using absolute LBA addressing ("absolute meaning that
 * sector numbers are relative to the drive, not relative to a partition).
 * The function also returns VMDKR_RSLT_xxxx codes as listed above.
 */

HVDDR VMDKR_Close(HVDDR pThis);
/* Closes a previously opened VMDK file, discarding the structures created
 * when the file was opened.
 */

BOOL VMDKR_QuickGetUUID(CPFN fn, S_UUID *UUID);
/* Quickly fetches signature UUID from VMDK file, without performing a full "open and initialize",
 * e.g. without resolving any snapshot chain.
 */

/*----------------------------------------------------------------------*/

#endif


/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef HDDR_H
#define HDDR_H

/*==========================================================================*/
/* Module which lets me read from Parallels (HDD) files, v2.0 format.       */
/*--------------------------------------------------------------------------*/
/* This module actually implements a subclass of the VDDR object class (see */
/* vddr.h). However I also define a full interface for this module so it    */
/* could be made stand-alone.                                               */
/*==========================================================================*/

#include "vddr.h"
#include "parallels.h"

UINT HDDR_GetLastError(void);
/* All of the functions in this module set an error code to provide
 * information on any failure. This function can be called to retrieve
 * the error code.
 */

PSTR HDDR_GetErrorString(UINT nErr);
/* This function can use to convert an error code into an a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text version of the last error
 * (this will return NULL if lasterror is 0).
 */

HVDDR HDDR_Open(CPFN fn, UINT iChain);
/* Opens a HDD file, creating internal data structures that allow us to access
 * the contents efficiently. This module always requests exclusive (nonshared) file
 * access, since we can't have VBox modifying the HDD while we are using it.
 *
 * Returns handle to internal data structures if successful. Success indicates
 * that the file was found and had a recognized header. HDDR_GetLastErrorString
 * should be called even after success, as it may indicate a non-fatal error,
 * such as an inconsistency between the file size and the size indicated
 * by the header.
 */

UINT HDDR_GetDriveType(HVDDR pThis); // returns VDD_TYPE_VDI
BOOL HDDR_GetDriveSize(HVDDR pThis, HUGE *drive_size);
UINT HDDR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift);
UINT HDDR_AllocatedBlocks(HVDDR pThis, UINT SPBshift);
UINT HDDR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end);
BOOL HDDR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid);
BOOL HDDR_IsSnapshot(HVDDR pThis);
/* These functions return various stats for the source HDD */

BOOL HDDR_GetHeader(HVDDR pThis, HDD_HEADER *hddh);
/* -- Note not part of the VDDR class.
 * This function provides direct access to the private header structure from the source HDD. This is
 * intended to be used for diagnostic features, such as by the ShowHeader module.
 */

int  HDDR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift);
/* Reads one page (block) from the source HDD. This function returns one of the
 * VDDR_RSLT_xxxx codes listed in vddr.h. SPBshift is the requested page size,
 * in SECTORS, expressed as a shift amount.
 */

int  HDDR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors);
/* Reads zero or more sectors from the drive using absolute LBA addressing ("absolute" meaning that
 * sector numbers are relative to the drive, not relative to a partition).
 * This function also returns one of the VDDR_RSLT_xxxx codes.
 */

HVDDR HDDR_Close(HVDDR pThis);
/* Closes a previously opened HDD file, discarding the internal structures created
 * when the file was opened.
 */

/*----------------------------------------------------------------------*/

#endif


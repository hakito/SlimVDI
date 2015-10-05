/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VHDR_H
#define VHDR_H

/*==========================================================================*/
/* Module which lets me read from Virtual PC Virtual Hard Disk (VHD) files. */
/*--------------------------------------------------------------------------*/
/* This module actually implements a subclass of the VDDR object class (see */
/* vddr.h). However I also define a full interface for this module so it    */
/* could also be used stand-alone.                                          */
/*==========================================================================*/

#include "vddr.h"
#include "vhdstructs.h"

UINT VHDR_GetLastError(void);
/* All of the functions in this module set an error code to provide
 * information on any failure. This function can be called to retrieve
 * the error code.
 */

PSTR VHDR_GetErrorString(UINT nErr);
/* This function can use to convert an error code into an a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text version of the last error
 * (this will return NULL if lasterror is 0).
 */

HVDDR VHDR_Open(CPFN fn, UINT iChain);
/* Opens a VHD file, creating internal data structures that allow us to access
 * its contents efficiently. This module always requests exclusive (nonshared) file
 * access, since we can't have VBox or VPC modifying the VHD while we are using it.
 *
 * Return handle to internal data structures if successful. Success indicates
 * that the file was found and had a recognized header. VHDR_GetLastErrorString
 * should be called even after success, as it may indicate a non-fatal error,
 * such as an inconsistency between the file size and the size indicated
 * by the header.
 */

UINT VHDR_GetDriveType(HVDDR pThis); // returns VDD_TYPE_VHD
BOOL VHDR_GetDriveSize(HVDDR pThis, HUGE *drive_size);
UINT VHDR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift);
UINT VHDR_AllocatedBlocks(HVDDR pThis, UINT SPBshift);
UINT VHDR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end);
BOOL VHDR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid);
BOOL VHDR_IsSnapshot(HVDDR pThis);
/* Return one of several drive metrics. */

BOOL VHDR_GetHeader(HVDDR pThis, VHD_FOOTER *vhdf, VHD_DYN_HEADER *vhdh);
/* -- Note this function is not part of the VDDR class.
 * This function provides direct access to the private footer/header structures from the source VHD.
 * This is intended to be used for diagnostic features, such as by the ShowHeader module.
 * Otherwise these are private structures which should not be referenced directly by other modules. If
 * real working code needs to know something about the source disk it should call one of the basic
 * functions such as GetBlockSize()etc. Either of the vdiph or vdih arguments may be NULL if you don't
 * need that item.
 */

int  VHDR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift);
/* Reads one page (block) from the source VHD. This function returns one of the
 * VHDR_RSLT_xxxx codes listed above. SPBshift is the requested page size,
 * in SECTORS, expressed as a shift amount.
 */

int  VHDR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors);
/* Reads zero or more sectors from the drive using absolute LBA addressing ("absolute meaning that
 * sector numbers are relative to the drive, not relative to a partition).
 * The function also returns VHDR_RSLT_xxxx codes as listed above.
 */

HVDDR VHDR_Close(HVDDR pThis);
/* Closes a previously opened VHD file, discarding the structures created
 * when the file was opened.
 */

BOOL VHDR_QuickGetUUID(CPFN fn, S_UUID *UUID);
/* Allows the image UUID to be extracted without fully opening the VHD, i.e. without
 * resolving the snapshot chain.
 */

/*----------------------------------------------------------------------*/

#endif


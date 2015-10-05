/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VDIW_H
#define VDIW_H

/*=========================================================================*/
/* Module which lets me create VirtualBox Virtual Drive Image (VDI) files. */
/*=========================================================================*/

#include "filename.h"

typedef struct {UINT dummy;} *HVDIW;

// error codes
#define VDIW_ERR_NONE     0
#define VDIW_ERR_BADPATH  1 /* could not create file, bad path */
#define VDIW_ERR_EXISTS   2 /* destination file exists already */
#define VDIW_ERR_NOSPACE  3 /* not enough space on dest drive for the expected file size */
#define VDIW_ERR_WPROTECT 4 /* dest drive is write protected */
#define VDIW_ERR_ACCDENY  5 /* access was denied to dest drive */
#define VDIW_ERR_HANDLE   6 /* bad HVDI handle passed to a module function */
#define VDIW_ERR_NOMEM    7 /* ran out of memory */
#define VDIW_ERR_WRITE    8 /* I/O error on write */
#define VDIW_ERR_BLOCKNO  9 /* The write block number is more than the maximum set at creation time */

UINT VDIW_GetLastError(void);
/* All of the functions in this module set an error code to provide
 * information on any failure. This function can be called to retrieve
 * the error code.
 */

PSTR VDIW_GetErrorString(UINT nErr);
/* This function can use to convert an error code into a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text version of the last error
 * (this will return NULL if lasterror is 0).
 */

HVDIW VDIW_Create(CPFN fn, UINT BlockSize, UINT nBlocks, UINT nBlocksUsed);
/* Creates a VDI file, creating internal data structures that allow us to access
 * its contents efficiently. This function will prompt the user for confirmation
 * if the file exists already, and return NULL if the user refuses.
 *
 *   fn          - Name of VDI file to create (including path).
 *   BlockSize   - The size of a VDI block. Must be a power of 2, 512 or greater. In fact please stick to
 *                 using 1MB (1024^2) until further notice.
 *   nBlocks     - The max size of the disk, expressed as a block count.
 *   nBlocksUsed - The number of blocks you intend to allocate during creation (by calling VDIW_WritePage).
 *                 (This argument is only used to check disk space requirements, you are not strongly
 *                 committed to writing exactly that number of pages).
 *
 * Returns handle to internal data structures if successful. Success indicates
 * that the file was created - nothing is written to it yet. This function creates
 * a representation of the header which will include a new UUID.
 *
 */

BOOL VDIW_SetDriveUUID(HANDLE hVDI, S_UUID *drvuuid);
/* Assigns a specific UUID to the drive. Calling this function is optional, but if you
 * call it then you must do so prior to writing the first page/block. Returns TRUE
 * on success, check GetLastError() if it returns FALSE.
 */

BOOL VDIW_SetFileSize(HVDIW hVDI, UINT nBlocks);
/* This function causes the output file to be immediately extended to its expected final
 * size. This hopefully eliminates the cluster allocation overhead while writing the
 * VDI.
 */

BOOL VDIW_FixMBR(HVDIW hVDI, BYTE *MBR);
/* The virtual drive geometry changes if you enlarge a virtual disk which was previously
 * less than ~8GB, and this messes up the partition CHS-start and CHS-end fields in the
 * MBR. This function can be called to fix those errors. In the future we could perhaps
 * also fix other MBR problems.
 */

BOOL VDIW_WritePage(HVDIW hVDI, void *buffer, UINT iPage, BOOL bAllZero);
/* Writes one page (block) to the destination VDI. If this is the first page write then
 * this also causes an implied write of the header structures. Returns TRUE on success,
 * call GetLastError() if the return value is FALSE.
 *
 * Note that you can't write to the same page twice. Allocating each page once, in order,
 * should be optimal.
 *
 * This function need not be called for unallocated blocks. If you pass a block which is
 * filled with zeros then it is not written, instead a zero block marker is set in the
 * block map. If you know in advance that the block is zero filled then you can save
 * processing time by passing TRUE in the bAllZero argument.
 */

HVDIW VDIW_Close(HVDIW hVDI);
/* Finishes up and then closes the newly created VDI file.
 */

HVDIW VDIW_Discard(HVDIW hVDI);
/* Closes and discards the new file. Typically performed after an error.
 */

/*----------------------------------------------------------------------*/

#endif


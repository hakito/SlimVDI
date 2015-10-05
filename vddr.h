/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef VDDR_H
#define VDDR_H

/*=======================================================================*/
/* Module which lets me read from Virtual Disk Drives. This is a generic */
/* class interface with most of the real work done by format specific    */
/* instances of that class (eg. for VDI,VHD). The implementation part of */
/* this module, i.e. vddr.c itself, is very simple, since all it really  */
/* does is decide which instance type to create for a particular source  */
/* format.                                                               */
/*=======================================================================*/

#include "djtypes.h"
#include "filename.h"

typedef CLASS(VDDR) *HVDDR;

// Results returned by page(block) and sector read functions.
#define VDDR_RSLT_FAIL      0 /* I/O error reading page     */
#define VDDR_RSLT_NORMAL    1 /* page was read successfully */
#define VDDR_RSLT_BLANKPAGE 2 /* page was read successfully, and is filled with zeros */
#define VDDR_RSLT_NOTALLOC  3 /* page was read successfully, but is not allocated (data needn't be stored in clone) */

// Supported Virtual Disk Types.
#define VDD_TYPE_VDI        0 /* VirtualBox native format */
#define VDD_TYPE_VHD        1 /* Microsoft's Virtual Hard Disk format */
#define VDD_TYPE_VMDK       2 /* VMWare's virtual hard disk format */
#define VDD_TYPE_RAW        3 /* A raw drive image */
#define VDD_TYPE_PART_RAW   4 /* A raw partition image */
#define VDD_TYPE_PARALLELS  5 /* Parallels .hdd virtual disk format */
#define VDD_TYPE_COW        6 /* "Copy on Write" overlay projected onto the underlying format */

// Global LastError variable. I hate global variables, but there's no good way to
// get an error code from an object instance if object creation is what failed.
UINT VDDR_LastError;

UINT VDDR_GetLastError(void);
/* All disk read functions set an error code to provide information on any
 * failure. Always use this function, do not access the global variable
 * directly.
 */

PSTR VDDR_GetErrorString(UINT nErr);
/* This function can be used to convert an error code into an a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text description of the most recent
 * error (this will return a pointer to "Ok" if lasterror is 0).
 */

BOOL VDDR_OpenMediaRegistry(CPFN fn);
/* fn is the absolute path to a hard disk image file. This function looks for a
 * file called "SlimVDI_Media.xml" in the same folder, and loads it into memory
 * if it exists (returning TRUE). This optional file allows SlimVDI to resolve
 * snapshot links even if the VirtualBox media registry is broken.
 */

HVDDR VDDR_Open(CPFN fn, UINT iChain);
/* Opens a virtual disk file, creating an object instance whose methods can be
 * called to access the virtual disk contents efficiently. The virtual disk read
 * objects always request FILE_SHARE_READ access (not allowing FILE_SHARE_WRITE),
 * since we can't have VBox modifying the file while we are using it. The only
 * exception is when the VMDK object opens a physical disk or partition, in
 * which case the sharing mode allows both read and write, otherwise SlimVDI
 * couldn't clone a running OS drive.
 *
 * Returns an object instance pointer if successful. Success indicates
 * that the file was found, the format was recognized and had a valid header.
 * The application can call the GetError functions if the result is NULL.
 *
 * The "iChain" argument gives the index of this virtual disk in a chain of
 * differencing disks, with index 0 indicating the first disk in the chain,
 * the chain terminating at index n, the base disk.
 */

HVDDR VDDR_OpenByUUID(S_UUID *UUID, UINT iChain);
/* Opens a virtual disk file identified by UUID rather than filename. This
 * would be used when cloning snapshots and other differencing images, and requires
 * that I constructed a media registry somehow (i.e. that I'm able to turn a
 * UUID into a filename).
 */

BOOL VDDR_QuickGetUUID(CPFN fn, S_UUID *UUID);
/* This primitive function is used to get the creation UUID from a drive file in
 * the simplest way possible. This differs from the <object>::GetDriveUUID() method
 * in that the latter completely opens up the virtual drive, resolving snapshot
 * references etc, whereas this function is intended to be called - and will
 * work - without requiring the entire snapshot chain to be resolved.
 */

/*----------------------------------------------------------------------*/

// Remaining functions must be directed to an object instance. If the VDDR_Open()
// function returns a non-NULL object pointer P then these instance functions
// can be called using e.g. P->GetDriveSize(P,&u64DriveSize)
//
typedef CLASS(VDDR) {

UINT PUBLIC_METHOD(GetDriveType)(HVDDR pThis);
// Returns one of the VDD_TYPE_xxx values defined above, identifying the virtual drive format. There
// are few reasons for the app to need to know the underlying source drive type, the only good one
// being the "ShowHeader" diagnostic feature, which is of course format specific.

BOOL PUBLIC_METHOD(GetDriveSize)(HVDDR pThis, HUGE *drive_size);
// Returns (maximum) size in bytes of the virtual drive (note not the same as current allocated size).

UINT PUBLIC_METHOD(GetDriveBlockCount)(HVDDR pThis, UINT SPBshift);
// This is the same as GetDriveSize(), except the virtual drive size is expressed as a block count,
// each block being 'BlockSize' bytes in size. BlockSize would typically match what the app intends
// to use as the block size in the output virtual disk file - this is NOT the block size of the
// source virtual disk.
//
// Note 1: that 'BlockSize' is actually passed in SPBshift, expressed as log2(SectorsPerBlock).
// Note 2: the result is simply the drive size expressed in different units. This function does NOT
// return a count of blocks actually used.
//

UINT PUBLIC_METHOD(BlockStatus)(HVDDR pThis, HUGE LBA_start, HUGE LBA_end);
// This function is used to test the used/notused status of a block, the block being defined this time as
// an INCLUSIVE logical block address (LBA) range, i.e. LBA_end should index the last LBA in the block,
// not the first LBA of the next block. This function returns one of the VDDR_RSLT_xxxx codes, e.g.
// VDDR_RSLT_NORMAL indicates that the block is normal (it contains some mix of used and unused sectors),
// VDDR_RSLT_BLANKPAGE means all sectors are filled with zeros, VDDR_RSLT_NOTALLOC means that none of the
// sectors have ever been written to. VDDR_RSLT_FAIL is not expected from this function since no physical
// read should occur (out of range LBAs should return VDDR_RSLT_NOTALLOC).
//

BOOL PUBLIC_METHOD(GetDriveUUID)(HVDDR pThis, S_UUID *drvuuid);
// Gets the creation UUID of the virtual drive.

BOOL PUBLIC_METHOD(IsSnapshot)(HVDDR pThis);
// Returns TRUE if the virtual drive is a differencing type image dependant on some parent file.
// Implementation note: SlimVDI will reject the source disk if this function returns TRUE, so if the
// underlying VDDR object actually supports snapshots then this function should always return FALSE.

int PUBLIC_METHOD(ReadPage)(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift);
// Reads one page (block) from the source virtual disk. This function returns one
// of the VDDR_RSLT_xxxx codes listed above. SPBshift is once again the applications
// preferred page (block) size, expressed as log2(SectorsPerBlock). E.g. a 1MB block has
// 2048 sectors, 2048==2^11, therefore you pass 11. iPage is an absolute block number. The dest
// buffer must be big enough to receive the requested page size. Page sizes are an application
// convenience and bear no necessary relationship to native page sizes (if any) which exist
// in the source virtual disk format.
//

int PUBLIC_METHOD(ReadSectors)(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors);
// When reading a page at a time proves cumbersome, this method can be used instead for
// sector level reads. This function reads zero or more sectors from the drive using absolute
// LBA addressing ("absolute" meaning that sector numbering is relative to the first sector
// of the drive, not the first sector of a partition). This function also returns one the
// VDDR_RSLT_xxxx codes listed above.
//

HVDDR PUBLIC_METHOD(Close)(HVDDR pThis);
// Closes a previously opened virtual drive, destroying the VDDR object. This function always
// returns NULL, it is recommended that you use this to NULLify the newly obsolete object
// pointer, i.e.  P = P->Close(P).
//

} /* End definition */ VDDR;

/*----------------------------------------------------------------------*/

#endif


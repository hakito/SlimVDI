/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef COW_H
#define COW_H

/*================================================================================*/
/* This module creates an "overlay" on top of a virtual drive object, to provide  */
/* "Copy on write" (COW) capability. This overlay would be used when I want to    */
/* modify the data in the source disk before cloning it. An alternative to COW    */
/* would be to modify the data after the cloning operation, but if the changes    */
/* are extensive (as they might be when e.g. enlarging a partition), then doing   */
/* this after cloning may result in a clone which is no longer fully optimized or */
/* compacted. That is not a problem when the COW idea is used.                    */
/*                                                                                */
/* The COW overlay works in a similar way to a VirtualBox snapshot or difference  */
/* image, i.e. it allows the appearance and simplicity of being able to modify an */
/* underlying file without actually risking user data. The overlay abstraction    */
/* works on top of any underlying physical format (eg. VDI, VHD, VMDK, RAW, HDD). */
/*--------------------------------------------------------------------------------*/
/* The COW module is designed as a superset of the VDDR class (as implemented in  */
/* vdir.c, vhdr.c, etc). It inherits all of the methods of a normal VDDR object,  */
/* to which it adds new methods for modifying the virtual disk in various ways.   */
/*================================================================================*/

#include "vddr.h"

typedef CLASS(COWOVL) *HCOW;

UINT COW_GetLastError(void);
/* All of the functions in this module set an error code to provide
 * information on any failure. This function can be called to retrieve
 * the error code.
 */

PSTR COW_GetErrorString(UINT nErr);
/* This function can be used to convert an error code into an a readable
 * error string. Pass an error code returned by GetLastError(), or
 * pass nErr=0xFFFFFFFF to retrieve a text version of the last error
 * (this will return NULL if lasterror is 0).
 */

HVDDR COW_CreateCowOverlay(HVDDR SourceDisk);
/* Note that this function returns a normal HVDDR object, which must be
 * cast to an HCOW object when the additional COW overlay methods
 * are called.
 */

typedef CLASS(COWOVL) {

CLASS(VDDR) vddr; // inherit the methods of the VDDR class.

// and add the following additional methods

int PUBLIC_METHOD(InsertSectors)(HCOW pThis, HUGE LBA, UINT nSectors);
/* Inserts nSectors additional unallocated sectors into the virtual drive
 * at the given LBA offset. This effectively increases the drive size.
 * Returns an error code, 0 = no error.
 */

int PUBLIC_METHOD(MoveSectors)(HCOW pThis, HUGE LBA_from, HUGE LBA_to, UINT nSectors);
/* Moves nSectors sectors from one part of the virtual drive to another
 * part of the drive. No physical copying is done, we just adjust the
 * internal list of extents. This function will fail if the insertion point falls
 * inside the group of sectors being moved.
 * Returns an error code, 0 = no error.
 */

int PUBLIC_METHOD(WriteSectors)(HCOW pThis, void *buffer, HUGE LBA, UINT nSectors);
/* Writes (virtually) to the drive by copying affected blocks and writing to the copy.
 *
 * !WARNING! !WARNING! All large scale modifications (InsertSectors, MoveSectors) must
 * be done before the first WriteSectors() call. This is because the former allows sectors
 * to be moved and inserted on arbitrary sector boundaries, but every sector write
 * creates a COW block, and for performance reasons cow blocks have a fixed 1MB
 * granularity and page alignment and hence cannot currently be split or moved once
 * they are created.
 */

} COWOVL;

/*----------------------------------------------------------------------*/

#endif


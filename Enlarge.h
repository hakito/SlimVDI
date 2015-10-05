/*================================================================================*/
/* Copyright (C) 2010, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/*================================================================================*/
/* Module to handle enlarging of drives and partitions.                           */
/*================================================================================*/
#ifndef ENLARGE_H
#define ENLARGE_H

#include "djtypes.h"
#include "vddr.h"

HVDDR Enlarge_Drive(HVDDR hVDI, UINT NewDiskSizeMB);
/* This function is only called if the user selects the "Increase drive size" AND
 * "Enlarge partition" options. It increases the drive size by inserting unallocated
 * sectors into the disk image immediately after the current boot partition, and
 * then calls on the filesystem handler which owns that partition to "grow" into
 * the available space.
 *
 * Note that this function creates a modifiable Copy-On-Write (COW) overlay on top
 * of the original source drive, hence this function passes out a new HVDDR handle
 * which should henceforth be used instead of the handle passed in.
 *
 * If "Enlarge partition" is not set but "Enlarge disk" is, then an pre-existing
 * (simpler) function is called to expand the disk.
 */

#endif

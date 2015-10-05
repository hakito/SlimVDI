/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef UNPART_H
#define UNPART_H

/* Free space mapping object (FSYS) for unpartitioned regions of the disk */

#include "fsys.h"

HFSYS Unpart_OpenVolume(HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize);
/* Checks the source drive to see if unpartitioned regions account for a significant
 * portion of the disk (unpartitioned fragments of at least 1MB), in which case I can
 * treat that entire area as unused space when compacting (is this dangerous? does
 * any OS hide things in unallocated drive space?).
 *
 *    hVDI is the VDD object to read from. This handle must remain valid for as long
 *    as the mapper object is open.
 *
 *    iLBA is ignored, this module looks at the entire drive.
 *
 *    cLBA is the size of the drive, in sectors.
 *
 *    cSectorSize is the size of one sector, usually 512.
 */

HFSYS Unpart_CloseVolume(HFSYS hUnpart);
/* Closes a previously opened free space mapping object, returning NULL. Passing NULL to
 * this function is a NOP.
 */

int Unpart_IsBlockUsed(HFSYS hUnpart, UINT iBlock, UINT SectorsPerBlockShift);
/* This function checks whether the given block falls inside an unpartitioned region
 * of the drive. A block is an arbitrarily sized region of disk space - however the block
 * size must be a power of two number of sectors.
 *
 * o hUnpart must be a handle returned by a prior call to Unpart_OpenVolume().
 *
 * o iBlock is an offset from the start of the disk image, in units of BlockSize. Note that
 *   this is an absolute disk offset.
 *
 * o SectorsPerBlockShift is the number of 512 byte sectors in a block, expressed as a shift
 *   amount, i.e. log2(SectorsPerBlock). For example a 1MB block has 2048 sectors,
 *   which when expressed as a shift amount equals 11.
 *
 * The function returns one of the FSYS_BLOCK_xxxx result codes defined in fsys.h.
 *
 *   FSYS_BLOCK_OUTSIDE - The block falls at least partly outside the unallocated regions of the
 *                        drive (i.e. it falls (partly) inside at least one of the allocated regons).
 *   FSYS_BLOCK_UNUSED  - The block falls wholly within an unpartitioned region of the drive.
 */

#endif


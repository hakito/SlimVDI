/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef NTFS_H
#define NTFS_H

/* Support for NTFS guest filesystem */

#include "fsys.h"

BOOL NTFS_IsNTFSVolume(HVDDR hVDI, HUGE iLBA);
/* Does quick check to find out if an NTFS partition starts at the given LBA.
 * Returns TRUE if so.
 */

HFSYS NTFS_OpenVolume(HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize);
/* Attempts to open an NTFS volume (partition). The function returns a non-NULL handle if
 * the volume was successfully recognized as an NTFS volume.
 *
 *    hVDI is the VDD object to read from. This handle must remain valid for as long
 *    as the NTFS volume is open.
 *
 *    iLBA is the LBA start address of the volume (boot sector). Note NOT the disk MBR,
 *    at least not unless the MBR and the boot sector are the same.
 *
 *    cLBA is the length of the volume, in sectors.
 *
 *    cSectorSize is the size of one sector (usually 512).
 */

HFSYS NTFS_CloseVolume(HFSYS hNTFS);
/* Closes a previously opened NTFS volume, returning NULL. Passing NULL to this function
 * is a NOP.
 */

int NTFS_IsBlockUsed(HFSYS hNTFS, UINT iBlock, UINT SectorsPerBlockShift);
/* This function determines whether a given "block" is being used by the NTFS filesystem. A
 * block is an arbitrarily sized region of disk space - however the block size must be a
 * power of two.
 *
 * o hNTFS must be a handle returned by a prior call to NTFS_OpenVolume().
 *
 * o iBlock is an offset from the start of the disk image, in units of BlockSize. Note that
 *   this is an absolute disk offset. The referenced block may fully or partly or not at
 *   all intersect the NTFS volume.
 *
 * o SectorsPerBlockShift is the number of 512 byte sectors in a block, expressed as a shift
 *   amount, i.e. log2(SectorsPerBlock). For example a 1MB block has 2048 sectors,
 *   which when expressed as a shift amount equals 11.
 *
 * The function returns one of the FSYS_BLOCK_xxxx result codes defined in fsys.h.
 *
 *   FSYS_BLOCK_OUTSIDE - The block falls at least partly outside the NTFS volume, therefore
 *                        the NTFS manager cannot determine whether this block is needed (best
 *                        assume that it is, unless it falls inside some other partition).
 *   FSYS_BLOCK_USED    - The block falls wholly inside the NTFS volume, and the NTFS code has
 *                        determined that at least one used cluster falls inside this block.
 *   FSYS_BLOCK_UNUSED  - The block falls wholly inside the NTFS volume, and the NTFS code has
 *                        determined that no used clusters fall inside this block. (i.e. this
 *                        block can be discarded from a VDI clone).
 */

UINT NTFS_GrowPartition(HVDDR hVDI, UINT iLBA, UINT OldSectors, UINT NewSectors, UINT cHeads);
/* Enlarges the partition to fill the space vacated by enlarging the drive. The free space
 * is assumed to immediately follow the current partition.
 *
 *  o hVDI is a handle to the COW overlay over a physical disk (it must be a COW overlay to
 *    allow writes to the drive.
 *  o iLBA is the start LBA of the NTFS partition.
 *  o OldSectors is the current size of the partition, in sectors.
 *  o NewSectors is the maximum size of the enlarged partition.
 *  o cHeads is the number of heads (for CHS addressing) in the new drive geometry.
 *
 * The function returns the actual size in sectors of the enlarged partition. On an error
 * this will be unchanged from the "OldSectors" argument.
 *
 */

#endif


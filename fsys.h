/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/*=======================================================================================*/
/* Generic Class whose instances handle various guest filesystems (NTFS, EXTx, FAT etc). */
/*=======================================================================================*/
#ifndef FSYS_H
#define FSYS_H

#include "djtypes.h"
#include "vddr.h"

#define FSYS_BLOCK_OUTSIDE 0
#define FSYS_BLOCK_USED    1
#define FSYS_BLOCK_UNUSED  2

typedef CLASS(FSYS) *HFSYS;

HFSYS FSys_OpenVolume(UINT PartCode, HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize);
/* Attempts to open a volume (partition). The function returns a non-NULL handle if
 * the volume contains a filesystem which the application recognizes.
 *
 *    PartCode is the partition type code from the MBR. This gives the fsys object
 *    a hint as to which possible guest filesystems to try.
 *
 *    hVDI is the virtual disk object to read from. This handle must remain valid for
 *    as long as the volume is open.
 *
 *    iLBA is the LBA start address of the partition (usually the volume boot sector). Note
 *    this is NOT the MBR address, at least not unless the MBR and the boot sector are the same.
 *
 *    cLBA is the length of the volume, in sectors.
 *
 *    cSectorSize is the size of one sector (usually 512).
 *
 * This function is called to instantiate an object of the FSys class. The remaining
 * functions in this interface call methods of previously instantiated objects.
 *
 * Note there is no GetLastError() function in this module. We do error detection, but not
 * error reporting. That is because if we don't recognize the guest filesystem then we fall
 * back silently to "No compacting while cloning" behaviour.
 */

UINT FSys_GrowPartition(UINT PartCode, HVDDR hVDI, UINT iLBA, UINT OldSectors, UINT NewSectors, UINT cHeads);
/* Enlarges the partition to fill the space vacated by enlarging the drive. The free space
 * is assumed to immediately follow the current partition.
 *
 *  o PartCode is the partition type from the MBR. This gives the FSys module a hint as to
 *    which guest filesystem should process the request.
 *  o hVDI is a handle to the COW overlay over a physical disk (it must be a COW overlay to
 *    allow writes to the drive).
 *  o iLBA is the start LBA of the guest filesystem partition.
 *  o OldSectors is the current size of the partition, in sectors.
 *  o NewSectors is the maximum size of the enlarged partition.
 *  o cHeads is the number of heads (for CHS addressing) in the new drive geometry.
 *
 * The function returns the actual size in sectors of the enlarged partition. On an error
 * this will be unchanged from the "OldSectors" argument.
 *
 */

// -- Method Docs
typedef CLASS(FSYS) {

HFSYS PUBLIC_METHOD(CloseVolume)(HFSYS pThis);
// Closes a previously opened filesystem volume object instance, returning NULL.

int PUBLIC_METHOD(IsBlockUsed)(HFSYS pThis, UINT iBlock, UINT SPBshift);
// This function determines whether a given "block" is being used by the filesystem. A
// block is a region of disk space whose exact size is application determined, but it must
// be a power of two x 512 byte sectors.
//
// o pThis is an instance handle for a volume, returned by a previous call to FSys_OpenVolume().
//
// o iBlock is an offset from the start of the disk image, in units of BlockSize. This is an
//   absolute disk offset, it is NOT partition relative. The indicated block may fully, partly,
//   or not at all intersect the filesystem partition represented by the FSys object.
//
// o SPBshift is the number of 512 byte sectors in BlockSize, expressed as a shift
//   amount, i.e. log2(SectorsPerBlock). For example a standard 1MB block has 2048 sectors,
//   which when expressed as a shift amount equals 1l. Passing this argument as a shift
//   value alleviates the need for the function to do a slow log2(SectorsPerBlock)
//   calculation on every call. It also guarantees that BlockSize is always a power of
//   two, and always a multiple of 512 bytes.
//
// Block number and SectorsPerBlock values are an application convenience and need not
// match the native block numbering and block sizes used in the source virtual
// disk.
//
// The function returns one of the FSYS_BLOCK_xxxx result codes defined above.
//
//   FSYS_BLOCK_OUTSIDE - The block falls at least partly outside the partition, therefore
//                        this partition handler cannot determine whether this block is needed (best
//                        assume that it is, unless it falls inside some other partition).
//   FSYS_BLOCK_USED    - The block falls wholly inside the partition, and the handler has
//                        determined that some clusters in this block are used.
//   FSYS_BLOCK_UNUSED  - The block falls wholly inside the partition, and the handler has
//                        determined that none of the clusters in this block are used: this
//                        means that the block can be discarded from a clone.

} FSYS;

#endif

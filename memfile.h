/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef MEMFILE_H
#define MEMFILE_H

/* -----------------------------------------------------------------------
 * Module for creating, reading and writing a memory resident file.
 * -----------------------------------------------------------------------
 */

typedef struct {UINT dummy;} *HMEMFILE;

#define m_printf MemFile.mprintf

/*

HMEMFILE MemFile.Create(UINT initialAllocBytes);
 * Creates a memory resident file that can be written to. The memory resident
 * file consists of two HGLOBAL objects: the first is a "device context" structure
 * private to the module (this is what the HMEMFILE handle references), the other
 * is an I/O buffer which is the actual file image. The memory allocated to the
 * I/O buffer will grow as necessary, as the file is written to.
 *
 * "initialAllocBytes" is the size of the IO buffer initially allocated
 * for this file. If this parameter is zero then a default size will
 * be chosen, otherwise if it is known ahead of time how big the buffer
 * will get, then a number of global heap operations can be eliminated by
 * allocating the correct size buffer at the beginning, rather than
 * waiting for it to grow on demand.
 *

HMEMFILE MemFile.Open(HANDLE hMem, UINT objLenBytes);
 * "Opens" an existing global memory object as a memfile. This version
 * copies the memory object so that it can be both read from and written
 * to.
 *

HMEMFILE MemFile.OpenRead(HANDLE hMem, UINT objLenBytes, BOOL bAssignOwnership);
 * "Opens" an existing global memory object as a memfile in read-only mode.
 * This version of the Open() function does not copy the global memory
 * object. Writes are not allowed, hence the object will never grow in size.
 *
 * If bAssignOwnership is TRUE then ownership of the global memory object is
 * passed to the MemFile module despite the fact that the latter did not create
 * it. This ensures that the global memory object is discarded when the MemFile
 * handle is closed. Obviously this should not be done if the object will be needed
 * subsequently - setting the bAssignOwnership flag to FALSE ensures that closing
 * the memfile does NOT discard the original memory object.
 *

int MemFile.RdBin(HMEMFILE h, void *pData, int len);
 * Reads up to len bytes from a memory file, starting at the current r/w position.
 * The internal r/w position is then advanced by the the length of the read. The
 * function result is the number of bytes actually read; this can be less than the
 * len argument if old_rwpos+len was > EOF.
 *

int MemFile.WrBin(HMEMFILE h, void *pData, int len);
 * Writes len bytes to a memory file, provided the file was not opened in
 * read-only mode. The function result is the number of bytes written: this will
 * equal len unless the file was read-only, in which case it will be zero.
 *

int MemFile.DwordAlign(HMEMFILE h);
 * Advances the r/w pointer to the next dword boundary. You could do this
 * "longhand" using GetPos() and Seek(), but a single function call is more
 * convenient. No padding bytes are written - the pointer is simply moved. If
 * the hidden I/O buffer was allocated by the MemFile module then the buffer
 * will have been zero-filled on allocation however.
 *

int MemFile.Seek(HMEMFILE h, int pos);
 * Moves the read/write pointer to an arbitrary position (an offset from the
 * start of the file). Seeking beyond EOF is allowed, however the memory file
 * will not be enlarged until you write into the file at the extended seek
 * position. In the latter case any intermediate unwritten bytes will be
 * filled with zeroes by the memory allocator.
 *

int MemFile.GetPos(HMEMFILE h);
 * Returns the current position of the r/w pointer in this memory file.
 *

PVOID MemFile.GetPtr(HMEMFILE h, int pos);
 * Returns a pointer to the given location in the I/O buffer. Note that this
 * function does not check if pos is beyond EOF. Also, bear in mind that the
 * pointer is volatile and should not be stored, ie. if there is a subsequent
 * write to the file then the write buffer could move in memory, invalidating
 * any stored pointer. This idea is to use this feature for "quick and dirty"
 * patches to data already in the IO buffer, eg. correcting the header on a
 * file with information not known until the file was completed. This feature
 * can also be used to access data already written, without have to copy it.
 *

int MemFile.Size(HMEMFILE h);
 * Returns the current size in bytes of the memory file. This is the position of
 * the EOF marker, which is also the position just beyond the "rightmost" write.
 * This is not necessarily the same as the current r/w pointer pos (ie. since
 * the latter can be moved by a seek).
 *

BOOL MemFile.EOF(HMEMFILE h);
 * Returns TRUE if the r/w pointer is currently at or beyond EOF.
 *

PVOID MemFile.Extract(HMEMFILE h, int *lenBytes);
 * This function assigns ownership of the internal I/O buffer to the caller,
 * and returns a pointer to its base. This makes the internal buffer "read only",
 * and hence no further writes are possible - in fact you are expected to call
 * the Close() function in the very near future. Since ownership is assigned to
 * the caller the buffer will NOT be discarded when the file closes.
 *

HANDLE MemFile.Copy(HMEMFILE h, UINT gMemFlags);
 * Creates a copy of the memory file data on the global heap. The copy will be
 * truncated to exactly the size required, with allocation flags (passed to
 * GlobalAlloc) as per the gMemFlags arguments. The handle returned is unlocked.
 * This function differs from Extract() in that the buffer is copied, hence the
 * original buffer remains writeable (if that was previously allowed), plus you
 * can control the memory allocation flags for the new buffer, eg. you can
 * specify the GMEM_DDESHARE flag if you intend to place the buffer copy on the
 * Windows clipboard.
 *

void MemFile.Close(HMEMFILE h);
 * Closes the file, invalidating the HMEMFILE handle. If the file had ownership
 * of the I/O buffer then that too is discarded.
 */

typedef struct {
   HMEMFILE (*Create)(UINT initialAllocBytes);
   HMEMFILE (*Open)(HANDLE hMem, UINT objLenBytes);
   HMEMFILE (*OpenRead)(HANDLE hMem, UINT objLenBytes, BOOL bAssignOwnership);
   int      (*RdBin)(HMEMFILE h, void *pData, int len);
   int      (*WrBin)(HMEMFILE h, const void *pData, int len);
   int      (*DwordAlign)(HMEMFILE h);
   int      (*Seek)(HMEMFILE h, int pos);
   int      (*GetPos)(HMEMFILE h);
   PVOID    (*GetPtr)(HMEMFILE h, int pos);
   int      (*Size)(HMEMFILE h);
   BOOL     (*EOF)(HMEMFILE h);
   PVOID    (*Extract)(HMEMFILE h, int *lenBytes);
   HANDLE   (*Copy)(HMEMFILE h, UINT gMemFlags);
   void     (*Close)(HMEMFILE h);
   void     (*mprintf)(HMEMFILE f, PSTR s, ...);
} MemFile_DEF;

extern MemFile_DEF MemFile;

#endif


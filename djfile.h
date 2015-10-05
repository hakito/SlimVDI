/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* Header file for module DJFile */
#ifndef DJFILE_H
#define DJFILE_H

#include "djtypes.h"
#include "filename.h"

typedef struct {UINT dummy;} *FILE;
#define NULLFILE ((FILE)INVALID_HANDLE_VALUE)

#define DJFILE_ERROR_ACCESS_DENIED 5
#define DJFILE_ERROR_WRITEPROTECT  19
#define DJFILE_ERROR_SHARE         32

// flags for File_Create()
#define DJFILE_FLAG_OVERWRITE        1
#define DJFILE_FLAG_DELETEONCLOSE    2
#define DJFILE_FLAG_WRITETHROUGH     4
#define DJFILE_FLAG_SEQUENTIAL       8
#define DJFILE_FLAG_READWRITE        16

FILE   File_Create(CPFN fn, UINT flags);
// Opens a file in create mode, exclusive access. Creation will fail if the
// file already exists, unless the DJFILE_FLAG_OVERWRITE flag is set.
//

FILE   File_OpenRead(CPFN fn);
// Opens an existing file in read-only mode, allowing read access to other apps.
//

FILE   File_OpenReadShared(CPFN fn);
// Opens an existing file in read-only mode, allowing read/write access to other apps.
//

FILE   File_Open(CPFN fn);
// This opens an existing file in read/write mode, exclusive access.

void   File_Close(FILE f);

UINT   File_RdBin(FILE f, PVOID Buf, UINT Size);
UINT   File_WrBin(FILE f, PVOID Buf, UINT Size);

void   File_Seek(FILE f, HUGE pos);

UINT   File_GetPos(FILE f, HUGE *pos);
// Return value is low dword of pos. Full 64bit pos is returned in pos argument.
// pos argument may be NULL if you know the position will be less than 4GB.

UINT   File_Size(FILE f, HUGE *size);
// Return value is low dword of file size. Full 64bit size is returned in size argument.
// size argument may be NULL if you know the file size is less than 4GB.

void   File_Erase(CPFN fn);
BOOL   File_Exists(CPFN fn);
BOOL   File_Rename(CPFN oldname, CPFN name);

UINT   File_GetDate(FILE f);         // for legacy reasons, returns date in DOS format.
void   File_SetDate(FILE f, UINT d); // for legacy reasons, expects date in DOS format.

UINT   File_IOresult(void);
void   File_SetIOR(UINT err);

#endif


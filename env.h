/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef ENV_H
#define ENV_H

/*======================================================================*/
/* Miscellaneous interactions with the OS environment                   */
/*======================================================================*/

#include "djtypes.h"

int  Env_sprintf(PSTR szDest, CPSTR s, ...);
BOOL Env_GetDiskFreeSpace(CPFN szPath, HUGE *freebytes);
BOOL Env_GetDriveSectors(HANDLE hDrive, UINT *DriveSectors);

// BOOL Env_Error(PSTR szErr);
// Displays error message, always returns FALSE.

UI32 Env_AskYN(PSTR szMsg, PSTR szCaption);
UI32 Env_AskYNC(PSTR szMsg, PSTR szCaption);
// Yes/No? and Yes/No/Cancel message boxes.
// Returns 0=NO, 1=YES, 2=Cancel (latter only if cancel is allowed).

BOOL Env_BrowseFiles(HANDLE hWndParent, PSTR fnDflt, BOOL bInputFile, PSTR pszTemplate);
// Open/Save file dialog box.

// Command line processing
UI32 Env_ProcessCmdLine(HANDLE hInstApp, PSTR rawcmdline); // This must be called before the other functions in this group can be called. Returns argc.
void Env_GetProgramPath(PFN path);

UINT Env_GetEnvironmentVariable(PSTR pszDest, CPSTR pszVarName, UINT maxLen);
// This function returns the length of copied string, so 0 indicates failure or a zero length string.

UI32 Env_ParamCount(void); // returns argc
PSTR Env_ParamStr(UI32 n); // returns argv[n]. argv[0] returns program filename.

// Localization support
void  Env_SetLanguage(HANDLE hInstLangDLL, UINT idLanguage);
// must be called at an early stage in the app. hInstLangDLL names the module that the string table
// is stored in. idLanguage is a code indicating the preferred language. So far I have assigned
// 0=English, 1=Dutch, 2=German etc (new numbers are assigned in the order that people send me
// translations).
UINT Env_GetLanguage(void);
PSTR  Env_LoadString(PSTR *psz, UINT ids);
// Load a string and return a pointer to it. The pointer is guaranteed to be valid for the life
// of the app. If *psz is NULL on entry then I look up the string, copy the memory address to *psz, and
// also return the ptr as the function result. If *psz is not NULL then *psz is returned as the result.
// The idea is that the app stores static pointers to strings, hence we only need to do the string
// lookup once, on demand. Note that although strings in Win32 resources are stored in Unicode, I
// convert them to ANSI before returning a pointer. This means that I only currently support ANSI
// or multibyte encoded strings.
#define RSTR(szid) Env_LoadString(&psz##szid,IDS_##szid)
// The RSTR macro provides a less verbose way to access localized strings, provided you keep
// to certain naming conventions. I.e. if you have a string called STRING then a pointer to
// that string has to be called pszSTRING and the numeric id has to be called IDS_STRING.

// misc
void  Env_GenerateCloneName(PFN pfnClone, CPFN pfnSource);
PVOID Env_LoadBinData(PSTR szResource);
void  Env_DoubleToString(PSTR S, double X, UINT D);
UINT  Env_GetTempFileName(PSTR path, PSTR pszPrefix, UINT uUnique, PSTR pszOutputFilename);
BOOL  Env_DetectWine(void);
BOOL  Env_InitComAPI(BOOL bInit);
int   Env_FormatUUID(PSTR pszUUID, S_UUID *pUUID);

#endif


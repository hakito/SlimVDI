/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include "cmdline.h"
#include "env.h"
#include "djfile.h"
#include "djstring.h"
#include "version.h"
#include "ids.h"

static FNCHAR tmpfn[1024];

// localized strings
static PSTR pszERROR          /* = "Error" */ ;
static PSTR pszBANNER0        /* = "\015\012SlimVDI %lu.%02lx\015\012" */ ;
static PSTR pszBANNER1        /* = "Copyright (C) %lu, Don Milne\015\012" */ ;
static PSTR pszBANNER2        /* = "Feedback to: mpack on forums.virtualbox.org\r\n" */ ;
static PSTR pszARGERR         /* = "Error in argument %lu: %s.\015\012" */ ;
static PSTR pszOPTTWICE       /* = "Error in argument %lu: %s option specified twice.\015\012" */ ;
static PSTR pszNOFILEN        /* = "output option specified, no filename provided" */ ;
static PSTR pszNODSIZE        /* = "enlarge option specified, no disk size provided" */ ;
static PSTR pszUNKOPT         /* = "unknown option." */ ;
static PSTR pszINVOPT         /* = "Invalid option format (embedded space?)" */ ;
static PSTR pszSRCTWICE       /* = "Source name given twice? Dest file should be specified using --output <fn> option" */ ;
static PSTR pszNEEDSRC        /* = "Source filename is missing" */ ;

// I decided not to allow localisation of command line option names
// after all, as it could break scripts.
static PSTR pszVOPTOUTPUT     = "output";
static PSTR pszVOPTKEEPUUID   = "keepuuid";
static PSTR pszVOPTENLARGE    = "enlarge";
static PSTR pszVOPTCOMPACT    = "compact";
static PSTR pszVOPTREPART     = "repart";
static PSTR pszVOPTNOMERGE    = "nomerge";
static PSTR pszVOPTHELP       = "help";
static PSTR pszCHAROPT        = "okechr";

/*.....................................................*/

static BOOL
Error(PSTR pszMsg)
{
   FILE stderr = GetStdHandle(STD_ERROR_HANDLE);
   if (stderr == NULLFILE) stderr = 0;
   if (stderr) {
      File_WrBin(stderr,pszMsg,lstrlen(pszMsg));
      File_WrBin(stderr,"\r\n",2);
   } else {
      MessageBox(GetFocus(), pszMsg, RSTR(ERROR), MB_ICONEXCLAMATION|MB_OK);
   }
   return FALSE;
}

/*..........................................................*/

static BOOL
Usage(BOOL bShowBanner)
{
   FILE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
   if (stdout) {
      UINT ids;
      if (bShowBanner) {
         char sz[64];
         wsprintf(sz,RSTR(BANNER0),SOFTWARE_VERSION>>8,SOFTWARE_VERSION & 0xFF);
         File_WrBin(stdout, sz, lstrlen(sz));
         wsprintf(sz,RSTR(BANNER1),COPYRIGHT_YEAR);
         File_WrBin(stdout, sz, lstrlen(sz));
         wsprintf(sz,RSTR(BANNER2));
         File_WrBin(stdout, sz, lstrlen(sz));
      }
      for (ids=IDS_USAGE00; ids<=IDS_USAGE_LAST; ids++) {
         PSTR pszUsage = NULL;
         Env_LoadString(&pszUsage, ids);
         File_WrBin(stdout, pszUsage, lstrlen(pszUsage));
      }
   }
   return FALSE;
}

/*.......................................................................*/

static BOOL
ArgError(PSTR pszErr, UINT iArg)
{
   CHAR szErr[256];
   wsprintf(szErr,RSTR(ARGERR),iArg);
   Error(szErr);
   return Usage(FALSE);
}

/*.......................................................................*/

static BOOL
ErrOptionSetTwice(PSTR pszOptName, UINT iArg)
{
   CHAR szErr[256];
   wsprintf(szErr,RSTR(OPTTWICE),pszOptName,iArg);
   Error(szErr);
   return Usage(FALSE);
}

/*.......................................................................*/

static UINT
GetOutputOption(s_CLONEPARMS *parm, UINT iArg, BOOL gotDstFn)
{
   PSTR pszArg = Env_ParamStr(iArg);
   if (gotDstFn) return ErrOptionSetTwice(pszVOPTOUTPUT,iArg-1);
   if (!pszArg) return ArgError(RSTR(NOFILEN),iArg-1);
   String_Copy(parm->dstfn, pszArg, 1024);
   return (iArg+1);
}

/*.......................................................................*/

static UINT
GetEnlargeOption(s_CLONEPARMS *parm, UINT iArg)
{
   PSTR pszArg = Env_ParamStr(iArg);
   if (parm->flags & PARM_FLAG_ENLARGE) return ErrOptionSetTwice(pszVOPTENLARGE,iArg-1);
   if (!pszArg) return ArgError(RSTR(NODSIZE),iArg-1);
   String_Copy(parm->szDestSize, pszArg, 32);
   parm->flags |= PARM_FLAG_ENLARGE;
   return (iArg+1);
}

/*.......................................................................*/

static BOOL
GetOption(s_CLONEPARMS *parm, UINT iArg, UINT flag, PSTR pszOptName)
// Get generic option which has no parameters.
{
   if (parm->flags & flag) return ErrOptionSetTwice(pszOptName,iArg-1);
   parm->flags |= flag;
   return TRUE;
}

/*.......................................................................*/

static PSTR
VerboseSubOption(PSTR pszDest, UINT len, PSTR psz)
{
   UINT c = *psz++;
   PSTR pszDestMax = pszDest+len-1;
   while (c) {
      if (c=='+') break;
      if (pszDest<pszDestMax) *pszDest++ = (char)c;
      c = *psz++;
   }
   if (c!='+') psz--;
   *pszDest = (char)0;
   return psz;
}

/*.....................................................*/

PUBLIC BOOL
CmdLine_Parse(s_CLONEPARMS *parm)
{
   BOOL gotSrcFn=FALSE,gotDstFn=FALSE;
   PSTR pszArg;
   UINT argc = Env_ParamCount();
   UINT iArg = 1;

   ZeroMemory(parm,sizeof(s_CLONEPARMS));
   parm->flags = PARM_FLAG_CLIMODE;

   while (iArg <= argc) {
      pszArg = Env_ParamStr(iArg);
      if (!pszArg) break; // can't happen.
      
      iArg++;
      if (pszArg[0]=='-') { // if option
         if (pszArg[1]=='-') {
            // verbose options
            char szItem[32];
            pszArg+=2;
            for (;;) {
               pszArg = VerboseSubOption(szItem,32,pszArg);
               if (szItem[0]==0) break;
               if (String_Compare(szItem,pszVOPTHELP)==0) {
                  return Usage(TRUE);
               } else if  (String_Compare(szItem,pszVOPTOUTPUT)==0) {
                  iArg = GetOutputOption(parm,iArg,gotDstFn);
                  if (iArg==0) return FALSE;
                  gotDstFn = TRUE;
               } else if  (String_Compare(szItem,pszVOPTKEEPUUID)==0) {
                  if (!GetOption(parm,iArg,PARM_FLAG_KEEPUUID,pszVOPTKEEPUUID)) return FALSE;
               } else if  (String_Compare(szItem,pszVOPTCOMPACT)==0) {
                  if (!GetOption(parm,iArg,PARM_FLAG_COMPACT,pszVOPTCOMPACT)) return FALSE;
               } else if  (String_Compare(szItem,pszVOPTNOMERGE)==0) {
                  if (!GetOption(parm,iArg,PARM_FLAG_NOMERGE,pszVOPTNOMERGE)) return FALSE;
               } else if  (String_Compare(szItem,pszVOPTREPART)==0) {
                  if (!GetOption(parm,iArg,PARM_FLAG_REPART,pszVOPTREPART)) return FALSE;
               } else if  (String_Compare(szItem,pszVOPTENLARGE)==0) {
                  iArg = GetEnlargeOption(parm,iArg);
                  if (iArg==0) return FALSE;
               } else {
                  return ArgError(RSTR(UNKOPT),iArg-1);
               }
            }
         } else {
            // single char options.
            CHAR c;
            pszArg++;
            c = *pszArg++;
            if (!c) return ArgError(RSTR(INVOPT),iArg-1);
            while (c) {
               if (c==pszCHAROPT[4]) { // 'h'
                  return Usage(TRUE);
               } else if (c==pszCHAROPT[0]) { // 'o'
                  iArg = GetOutputOption(parm,iArg,gotDstFn);
                  if (iArg==0) return FALSE;
                  gotDstFn = TRUE;
               } else if (c==pszCHAROPT[1]) { // 'k'
                  if (!GetOption(parm,iArg,PARM_FLAG_KEEPUUID,pszVOPTKEEPUUID)) return FALSE;
               } else if (c==pszCHAROPT[3]) { // 'c'
                  if (!GetOption(parm,iArg,PARM_FLAG_COMPACT,pszVOPTCOMPACT)) return FALSE;
               } else if (c==pszCHAROPT[5]) { // 'r'
                  if (!GetOption(parm,iArg,PARM_FLAG_REPART,pszVOPTREPART)) return FALSE;
               } else if (c==pszCHAROPT[2]) { // 'e'
                  iArg = GetEnlargeOption(parm,iArg);
                  if (iArg==0) return FALSE;
               } else {
                  return ArgError(RSTR(UNKOPT),iArg-1);
               }
               c = *pszArg++;
            }
         }
      } else if (gotSrcFn) {
         return ArgError(RSTR(SRCTWICE),iArg-1);
      } else {
         GetFullPathName(pszArg,1024,parm->srcfn,0);
         gotSrcFn = TRUE;
      }
   }
   if (!gotSrcFn) {
      return ArgError(RSTR(NEEDSRC),iArg-1);
   }
   if (!gotDstFn) {
      Env_GenerateCloneName(parm->dstfn,parm->srcfn);
   }
   GetFullPathName(parm->dstfn,1024,tmpfn,0);
   String_Copy(parm->dstfn, tmpfn, 1024);

   if (!(parm->flags & PARM_FLAG_ENLARGE)) parm->flags &= ~PARM_FLAG_REPART;

   return TRUE;
}

/*.......................................................................*/

/* end of cmdline.c */


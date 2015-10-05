/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* Instance of VDDR class to handle reading of VMDK files */
#include "djwarning.h"
#include "djtypes.h"
#include "vmdkr.h"
#include "vmdkstructs.h"
#include "mem.h"
#include "djstring.h"
#include "djfile.h"
#include "partinfo.h"
#include "random.h"
#include "env.h"
#include "ids.h"

// error codes
#define VMDKR_ERR_NONE          0
#define VMDKR_ERR_FILENOTFOUND  ((VDD_TYPE_VMDK<<16)+1)
#define VMDKR_ERR_READ          ((VDD_TYPE_VMDK<<16)+2)
#define VMDKR_ERR_NOTVMDK       ((VDD_TYPE_VMDK<<16)+3)
#define VMDKR_ERR_INVHANDLE     ((VDD_TYPE_VMDK<<16)+4)
#define VMDKR_ERR_OUTOFMEM      ((VDD_TYPE_VMDK<<16)+5)
#define VMDKR_ERR_INVBLOCK      ((VDD_TYPE_VMDK<<16)+6)
#define VMDKR_ERR_BADFORMAT     ((VDD_TYPE_VMDK<<16)+7)
#define VMDKR_ERR_SEEK          ((VDD_TYPE_VMDK<<16)+8)
#define VMDKR_ERR_BLOCKMAP      ((VDD_TYPE_VMDK<<16)+9)
#define VMDKR_ERR_NODESCRIP     ((VDD_TYPE_VMDK<<16)+10)
#define VMDKR_ERR_BADDESC       ((VDD_TYPE_VMDK<<16)+11)
#define VMDKR_ERR_BADVERSION    ((VDD_TYPE_VMDK<<16)+12)
#define VMDKR_ERR_MISSINGFIELDS ((VDD_TYPE_VMDK<<16)+13)
#define VMDKR_ERR_SHARE         ((VDD_TYPE_VMDK<<16)+14)
#define VMDKR_ERR_COMPRESSION   ((VDD_TYPE_VMDK<<16)+15)
#define VMDKR_ERR_NOEXTENT      ((VDD_TYPE_VMDK<<16)+16)

static UINT OSLastError;

typedef struct {
   CLASS(VDDR) Base;
   UINT DiskType;
   VMDK_HEADER *hdr;
   HUGE DriveSectors;
   HUGE SectorsAllocated;
   int  PageReadResult;
   HVDDR hVDIparent;
   VMDK_EXTENT extent[VMDK_MAX_EXTENTS]; // actually variable length
} VMDK_INFO, *PVMDK;

static BYTE *Template_MBR; // binary data (RCDATA) loaded from resource file.

// localization strings
static PSTR pszOK          /* = "Ok" */;
static PSTR pszUNKERROR    /* = "Unknown Error" */;
static PSTR pszVDNOEXIST   /* = "The source file does not exist" */;
static PSTR pszVDERRSHARE  /* = "Source file already in use (is VirtualBox running?)" */;
static PSTR pszVDERRREAD   /* = "Got OS error %lu when reading from source file" */;
static PSTR pszVDNOTVMDK   /* = "Source file is not a recognized VMDK file format" */;
static PSTR pszVDINVHANDLE /* = "Invalid handle passed to VDx source object" */;
static PSTR pszVDNOMEM     /* = "Not enough memory to map source file" */;
static PSTR pszVDSEEKRANGE /* = "App attempted to seek beyond end of drive!" */;
static PSTR pszVDERRFMT    /* = "Source has strange format which is incompatible with this tool" */;
static PSTR pszVDERRSEEK   /* = "Got OS error %lu when seeking inside source file" */;
static PSTR pszVDBLKMAP    /* = "Source file corrupt - block map contains errors" */;
static PSTR pszVDNODESC    /* = "VMDK incomplete - disk descriptor not found" */;
static PSTR pszVDINVDESC   /* = "Invalid VMDK disk descriptor" */;
static PSTR pszVDBADVER    /* = "VMDK version number was not recognized" */;
static PSTR pszVDMISSING   /* = "Required parameters are missing from the VMDK descriptor" */;
static PSTR pszVDERRCOMP   /* = "Compressed VMDKs are not currently supported" */;
static PSTR pszVDNOEXTENT  /* = "A descriptor referenced extent file is missing (renamed?)" */;

/*.....................................................*/

static UINT
PowerOfTwo(UINT x)
// Returns the base 2 log of x, same as the index of the most significant 1 bit in x.
// Returns 0xFFFFFFFF if x was 0 on entry.
{
   int y;
   for (y=(-1); x>255; x>>=9) y+=9;
   while (x) {
      x >>= 1;
      y++;
   }
   return (UINT)y;
}

/*.....................................................*/

PUBLIC UINT
VMDKR_GetLastError(void)
{
   return VDDR_LastError;
}

/*.....................................................*/

PUBLIC PSTR
VMDKR_GetErrorString(UINT nErr)
{
   PSTR pszErr;
   static char sz[256];
   if (nErr==0xFFFFFFFF) nErr = VDDR_LastError;
   if (nErr==VMDKR_ERR_NONE) pszErr=RSTR(OK);
   else {
      switch (nErr) {
         case VMDKR_ERR_FILENOTFOUND:
            pszErr=RSTR(VDNOEXIST);
            break;
         case VMDKR_ERR_READ:
            Env_sprintf(sz,RSTR(VDERRREAD),OSLastError);
            pszErr=sz;
            break;
         case VMDKR_ERR_SHARE:
            pszErr=RSTR(VDERRSHARE);
            break;
         case VMDKR_ERR_NOTVMDK:
            pszErr=RSTR(VDNOTVMDK);
            break;
         case VMDKR_ERR_INVHANDLE:
            pszErr=RSTR(VDINVHANDLE);
            break;
         case VMDKR_ERR_OUTOFMEM:
            pszErr=RSTR(VDNOMEM);
            break;
         case VMDKR_ERR_INVBLOCK:
            pszErr=RSTR(VDSEEKRANGE);
            break;
         case VMDKR_ERR_BADFORMAT:
            pszErr=RSTR(VDERRFMT);
            break;
         case VMDKR_ERR_SEEK:
            Env_sprintf(sz,RSTR(VDERRSEEK),OSLastError);
            pszErr=sz;
            break;
         case VMDKR_ERR_BLOCKMAP:
            pszErr = RSTR(VDBLKMAP);
            break;
         case VMDKR_ERR_NODESCRIP:
            pszErr = RSTR(VDNODESC);
            break;
         case VMDKR_ERR_BADDESC:
            pszErr = RSTR(VDINVDESC);
            break;
         case VMDKR_ERR_BADVERSION:
            pszErr = RSTR(VDBADVER);
            break;
         case VMDKR_ERR_MISSINGFIELDS:
            pszErr = RSTR(VDMISSING);
            break;
         case VMDKR_ERR_COMPRESSION:
            pszErr = RSTR(VDERRCOMP);
            break;
         case VMDKR_ERR_NOEXTENT:
            pszErr = RSTR(VDNOEXTENT);
            break;
         default:
            pszErr = RSTR(UNKERROR);
      }
   }
   return pszErr;
}

/*.....................................................*/

static UINT
DoSeek(FILE f, HUGE pos)
{
   HUGE currpos;
   UINT err = 0;
   if (File_GetPos(f,&currpos)==0xFFFFFFFF) err = File_IOresult();
   else if (pos!=currpos) {
      File_Seek(f,pos);
      err = File_IOresult();
   }
   return err;
}

/*.....................................................*/

static UINT
ReadTextLine(PSTR *ppsz, PSTR pszEnd, PSTR sz)
{
   PSTR psz = *ppsz;
   UINT c=10,slen=0;

   // skip leading whitespace
   while (psz<pszEnd) {
      c = *psz++;
      if (c==10 || c>' ') break;
   }
   if (c!=10 && c!='#') { // if a non-whitespace was found then...
      sz[slen++] = (char)c;
      while (psz<pszEnd && slen<255) {
         c = *psz++;
         if (c==10 || c=='#') break;
         if (c<' ') continue;
         sz[slen++] = (char)c;
      }
   }
   if (c=='#') { // discard anything following a comment char, up to EOL.
      while (psz<pszEnd) {
         c = *psz++;
         if (c==10) break;
      }
   }
   *ppsz = psz;
   // discard trailing whitespace from the string.
   while (slen && sz[slen-1]<=' ') slen--;
   sz[slen] = (char)0;
   return slen;
}

/*.....................................................*/

#define TOKEN_TYPE_TOKEN  0
#define TOKEN_TYPE_EQUALS 1
#define TOKEN_TYPE_STRING 2
#define TOKEN_TYPE_NONE   3 /* unexpected EOL */

typedef struct {
   PSTR pszToken;
   UINT idToken;
} SymbolDef;

#define TOK_UNKNOWN        0

#define TOK_VERSION        1
#define TOK_ENCODING       2
#define TOK_CID            3
#define TOK_PARENTCID      4
#define TOK_CREATETYPE     5
#define TOK_RW             6  /* RW,RDONLY and NOACCESS tokens must be consecutively numbered, in that order */
#define TOK_RDONLY         7
#define TOK_NOACCESS       8
#define TOK_ADAPTER        9
#define TOK_SECTORSPT     10
#define TOK_NHEADS        11
#define TOK_NCYLS         12
#define TOK_UUID          13
#define TOK_VHWVER        14
#define TOK_UUID_IMAGE    15
#define TOK_UUID_PARENT   16
#define TOK_UUID_MODIFY   17
#define TOK_UUID_PMODIFY  18
#define N_LEAD_SYMBOLS    18

static SymbolDef LeadSymbols[N_LEAD_SYMBOLS] = {
   {"cid",                    TOK_CID},
   {"createtype",             TOK_CREATETYPE},
   {"ddb.adaptertype",        TOK_ADAPTER},
   {"ddb.geometry.cylinders", TOK_NCYLS},
   {"ddb.geometry.heads",     TOK_NHEADS},
   {"ddb.geometry.sectors",   TOK_SECTORSPT},
   {"ddb.uuid",               TOK_UUID},
   {"ddb.uuid.image",         TOK_UUID_IMAGE},
   {"ddb.uuid.modification",  TOK_UUID_MODIFY},
   {"ddb.uuid.parent",        TOK_UUID_PARENT},
   {"ddb.uuid.parentmodification", TOK_UUID_PMODIFY},
   {"ddb.virtualhwversion",   TOK_VHWVER},
   {"encoding",               TOK_ENCODING},
   {"noaccess",               TOK_NOACCESS},
   {"parentcid",              TOK_PARENTCID}, // deprecated I think - ddb.uuid.parent is preferred
   {"rdonly",                 TOK_RDONLY},
   {"rw",                     TOK_RW},
   {"version",                TOK_VERSION},
};

#define N_CREATE_TYPES 7
static SymbolDef CreateTypes[N_CREATE_TYPES] = {
   { "monolithicflat",        VMDK_CRTYPE_MONOFLAT },
   { "monolithicsparse",      VMDK_CRTYPE_MONOSPARSE },
   { "twogbmaxextentflat",    VMDK_CRTYPE_2GBFLAT },
   { "twogbmaxextentsparse",  VMDK_CRTYPE_2GBSPARSE },
   { "fulldevice",            VMDK_CRTYPE_FULLDEVICE },
   { "partitioneddevice",     VMDK_CRTYPE_PARTDEVICE },
   { "streamoptimized",       VMDK_CRTYPE_STREAMOPT },
};

#define N_EXTENT_TYPES 3
static SymbolDef ExtentTypes[N_EXTENT_TYPES] = {
   { "flat",   VMDK_EXT_TYPE_FLAT },
   { "sparse", VMDK_EXT_TYPE_SPARSE },
   { "zero",   VMDK_EXT_TYPE_ZERO },
};

/*.....................................................*/

static UINT
GetToken(PSTR *ppsz, PSTR pszToken)
{
   PSTR psz = *ppsz;
   UINT TokenType;
   UINT c = *psz++;
   UINT tlen=0; // I limit any token to 255 chars + terminating NUL.

   // discard leading whitespace.
   while (c<=' ') {
      if (c==0) break;
      c = *psz++;
   }
   if (c==0) {
      TokenType = TOKEN_TYPE_NONE;
   } else if (c=='=') {
      TokenType = TOKEN_TYPE_EQUALS;
   } else if (c=='"') {
      // read a string.
      TokenType = TOKEN_TYPE_STRING;
      c = *psz++;
      while (c && c!='"') {
         if (tlen<255) *pszToken++ = (char)c;
         tlen++;
         c = *psz++;
      }
   } else {
      // any other token type is terminated by EOL, '=' or whitespace.
      TokenType = TOKEN_TYPE_TOKEN;
      while (c>' ' && c!='=') {
         if (tlen<255) *pszToken++ = (char)c;
         tlen++;
         c = *psz++;
      }
      if (c=='=') psz--;
   }
   *pszToken = (char)0;
   if (c==0) psz--;
   *ppsz = psz;
   return TokenType;
}

/*.....................................................*/

static UINT
TokenID(SymbolDef *pTable, UINT nTableEntries, PSTR pszToken)
{
   UINT i,tt=TOK_UNKNOWN;
   for (i=0; i<nTableEntries; i++) {
      if (String_CompareCI(pTable[i].pszToken,pszToken)==0) {
         tt = pTable[i].idToken;
         break;
      }
   }
   return tt;
}

/*.....................................................*/

static UINT
Decimal(PSTR psz)
{
   UINT v=0,c = *psz++;
   while (c>='0' && c<='9') {
      v = v*10 + (c-'0');
      c = *psz++;
   }
   return v;
}

/*.....................................................*/

static UINT
HexNum(PSTR psz, UINT digits)
{
   UINT v=0,c = *psz++;
   for (; digits; digits--) {
      if (c>='0' && c<='9') v = (v<<4) + (c-'0');
      else if (c>='a' && c<='f') v = (v<<4) + 10 + (c-'a');
      else if (c>='A' && c<='F') v = (v<<4) + 10 + (c-'A');
      else break;
      c = *psz++;
   }
   return v;
}

/*.....................................................*/

static void
ReadUUID(S_UUID *pUUID, char *token)
{
   UINT i;
   pUUID->au32[0] = HexNum(token,8);
   for (i=2; i<4; i++) pUUID->au16[i] = (UI16)HexNum(token+9+(i-2)*5,4);
   pUUID->au8[8] = (BYTE)(HexNum(token+19,2));
   pUUID->au8[9] = (BYTE)(HexNum(token+21,2));
   for (i=10; i<16; i++) pUUID->au8[i] = (UI08)HexNum(token+4+(i*2),2);
}

/*.....................................................*/

static BOOL
ParseLine(VMDK_HEADER **ppvhdr, PSTR psz, UINT slen)
{
   VMDK_HEADER *pvhdr = *ppvhdr;
   char token[256];
   if (GetToken(&psz,token)==TOKEN_TYPE_TOKEN) {
      UINT idToken = TokenID(LeadSymbols,N_LEAD_SYMBOLS,token);
      switch (idToken) {
         case TOK_UNKNOWN:
            return TRUE; // skip unknown lines
         case TOK_VERSION:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_TOKEN) {
                  pvhdr->version = Decimal(token);
                  pvhdr->flags  |= VMDK_PARSED_VERSION;
                  return TRUE;
               }
            }
            break;
         case TOK_ENCODING:
         case TOK_CREATETYPE:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  if (idToken==TOK_ENCODING) {
                     pvhdr->flags  |= VMDK_PARSED_ENCODING;
                     return TRUE;
                  } else { // createType
                     pvhdr->flags  |= VMDK_PARSED_CREATETYPE;
                     pvhdr->CreateType = TokenID(CreateTypes,N_CREATE_TYPES,token);
                     return (pvhdr->CreateType!=VMDK_CRTYPE_UNKNOWN);
                  }
               }
            }
            break;
         case TOK_CID:
         case TOK_PARENTCID:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_TOKEN) {
                  if (idToken==TOK_CID) {
                     pvhdr->CID = HexNum(token,8);
                     pvhdr->flags  |= VMDK_PARSED_CID;
                  } else {
                     pvhdr->CIDParent = HexNum(token,8);
                     pvhdr->flags  |= VMDK_PARSED_PARENTCID;
                  }
                  return TRUE;
               }
            }
            break;
         case TOK_RW:
         case TOK_RDONLY:
         case TOK_NOACCESS: { /* new disk extent */
            UINT i = pvhdr->nExtents;
            if (i==(UINT)(pvhdr->nMaxExtents)) {
               UINT bytes;
               pvhdr->nMaxExtents += VMDK_MAX_EXTENTS;
               bytes = VMDK_HEADER_BASE_SIZE+pvhdr->nMaxExtents*sizeof(VMDK_EXTENT_DESCRIPTOR);
               pvhdr = Mem_ReAlloc(pvhdr, MEMF_ZEROINIT, bytes);
               *ppvhdr = pvhdr;
            }
            pvhdr->nExtents++;
            pvhdr->flags  |= VMDK_PARSED_EXTENT;
            pvhdr->extdes[i].iomode = (idToken-TOK_RW)+VMDK_EXT_IOMODE_RW;
            if (GetToken(&psz,token)==TOKEN_TYPE_TOKEN) {
               pvhdr->extdes[i].sectors = Decimal(token);
               if (GetToken(&psz,token)==TOKEN_TYPE_TOKEN) {
                  pvhdr->extdes[i].type = TokenID(ExtentTypes,N_EXTENT_TYPES,token);
                  if (pvhdr->extdes[i].type!=VMDK_EXT_TYPE_UNKNOWN && GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                     String_Copy(pvhdr->extdes[i].fn, token, 256); // GetToken() guarantees token length <= 256 chars, including the NUL.
                     if (GetToken(&psz,token)==TOKEN_TYPE_TOKEN) { // offset is optional
                        pvhdr->extdes[i].offset = Decimal(token);
                     }
                     return TRUE;
                  }
               }
            }
            break;
         }
         case TOK_ADAPTER:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) return TRUE;
            }
            break;
         case TOK_SECTORSPT:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  pvhdr->flags  |= VMDK_PARSED_SECTORSPT;
                  pvhdr->geometry.cBytesPerSector = 512;
                  pvhdr->geometry.cSectorsPerTrack = Decimal(token);
                  return TRUE;
               }
            }
            break;
         case TOK_NHEADS:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  pvhdr->flags  |= VMDK_PARSED_HEADS;
                  pvhdr->geometry.cHeads = Decimal(token);
                  return TRUE;
               }
            }
         case TOK_NCYLS:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  pvhdr->flags  |= VMDK_PARSED_CYLINDERS;
                  pvhdr->geometry.cHeads = Decimal(token);
                  return TRUE;
               }
            }
         case TOK_UUID:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  UINT i;
                  pvhdr->flags  |= VMDK_PARSED_UUID;
                  for (i=0; i<16; i++) pvhdr->uuidCreate.au8[i] = (BYTE)HexNum(token+i*3,2);
                  return TRUE;
               }
            }
         case TOK_UUID_IMAGE:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  ReadUUID(&pvhdr->uuidCreate,token);
                  pvhdr->flags  |= VMDK_PARSED_UUID;
                  return TRUE;
               }
            }
            break;
         case TOK_UUID_PARENT:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  ReadUUID(&pvhdr->uuidParent,token);
                  pvhdr->flags  |= VMDK_PARSED_UUID_PARENT;
                  return TRUE;
               }
            }
            break;
         case TOK_UUID_MODIFY:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  ReadUUID(&pvhdr->uuidModify,token);
                  pvhdr->flags  |= VMDK_PARSED_UUID_MODIFY;
                  return TRUE;
               }
            }
            break;
         case TOK_UUID_PMODIFY:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  ReadUUID(&pvhdr->uuidPModify,token);
                  pvhdr->flags  |= VMDK_PARSED_UUID_PMODIFY;
                  return TRUE;
               }
            }
            break;
         case TOK_VHWVER:
            if (GetToken(&psz,token)==TOKEN_TYPE_EQUALS) {
               if (GetToken(&psz,token)==TOKEN_TYPE_STRING) {
                  pvhdr->flags  |= VMDK_PARSED_VHWVER;
                  pvhdr->vhwver = Decimal(token);
                  return TRUE;
               }
            }
         default:
            ;
      }
   }
   return FALSE;
}

/*.....................................................*/

static PSTR
ReadDescriptor(FILE f, UINT *bytes, BOOL *bEmbedded)
// Reads the descriptor text into memory. The descriptor text may be stored in
// a small text file, or embedded in the first extent. On success this function
// returns a pointer to the descriptor text buffer.
{
   HUGE fsize;
   PSTR pMem;

   *bEmbedded = FALSE;
   File_Size(f,&fsize);
   if (fsize<16384) { // I'll assume that a small file indicates a text descriptor file
      *bytes = LO32(fsize);
   } else {
      VMDK_SPARSE_HEADER hdr;
      if (File_RdBin(f,&hdr,sizeof(hdr))!=sizeof(hdr)) {
         VDDR_LastError = VMDKR_ERR_READ;
         OSLastError = File_IOresult();
         return NULL;
      }
      VDDR_LastError = VMDKR_ERR_NOTVMDK;
      if (hdr.magic != VMDK_MAGIC_NUM) return NULL;
      if (hdr.descriptorOffset==0 || hdr.descriptorSize==0 || hdr.descriptorSize>16384) {
         VDDR_LastError = VMDKR_ERR_NODESCRIP;
         return NULL;
      }
      File_Seek(f,hdr.descriptorOffset<<9);
      *bytes = LO32(hdr.descriptorSize<<9);
      *bEmbedded = TRUE;
   }
   VDDR_LastError = VMDKR_ERR_READ;
   pMem = Mem_Alloc(MEMF_ZEROINIT, *bytes);
   if (File_RdBin(f,pMem,*bytes) != *bytes) OSLastError = File_IOresult();
   else {
      VDDR_LastError = 0;
      return pMem;
   }
   Mem_Free(pMem);
   return NULL;
}

/*.....................................................*/

static BOOL
ParseDescriptor(VMDK_HEADER **ppvhdr, PSTR pMem, UINT bytes)
// Parses each descriptor line in turn. Returns TRUE if the complete
// descriptor was parsed successfully.
{
   PSTR psz = pMem;
   PSTR pszEnd = pMem+bytes;
   UINT slen;
   char sz[512];

   VDDR_LastError = 0;
   while (psz<pszEnd && *psz) {
      slen = ReadTextLine(&psz,pszEnd,sz);
      if (slen) { // if non-blank, non-comment line was read...
         if (!ParseLine(ppvhdr,sz,slen)) {
            VDDR_LastError = VMDKR_ERR_BADDESC;
            break;
         }
      }
   }
   Mem_Free(pMem);
   if (VDDR_LastError==0) {
      if (((*ppvhdr)->flags & VMDK_HEADER_MUST_HAVE)!=VMDK_HEADER_MUST_HAVE) VDDR_LastError=VMDKR_ERR_MISSINGFIELDS;
      else if ((*ppvhdr)->version != VMDK_FORMAT_VER) VDDR_LastError=VMDKR_ERR_BADVERSION;
      else if ((*ppvhdr)->CreateType==VMDK_CRTYPE_STREAMOPT) VDDR_LastError=VMDKR_ERR_COMPRESSION;
   }
   return (VDDR_LastError==0);
}

/*.....................................................*/

static BOOL
ValidateMap(PEXTENT pe)
// Only applied to sparse extents
{
   UINT i,nAlloc,nFree,sid,nSectors;
   HUGE fsize;

   File_Size(pe->f,&fsize);
   fsize>>=9;
   nSectors = LO32(fsize); // this is actual size, not virtual extent size.

   nAlloc = nFree = 0;
   for (i=0; i<pe->nBlocks; i++) {
      sid = pe->blockmap[i];
      if (sid == VMDK_PAGE_FREE) nFree++;
      else if (sid<nSectors) nAlloc++;
      else {
         return FALSE; // SID out of range.
      }
   }
   pe->nBlocksAllocated = nAlloc;
   return ((nAlloc+nFree) == pe->nBlocks); // this doesn't really do anything useful. More to come.
}

/*.....................................................*/

static BOOL
OpenExtent(CPFN fn, PVMDKED ped, VMDK_EXTENT *pe)
{
   BOOL bOK = TRUE;
   if (ped->type != VMDK_EXT_TYPE_ZERO) {
      if (Mem_Compare(fn,"\\\\.\\",4)==0) pe->f = File_OpenReadShared(fn);
      else pe->f = File_OpenRead(fn);
      if (pe->f==NULLFILE) bOK = FALSE;
   }
   if (!bOK) {
      if (File_IOresult()==DJFILE_ERROR_SHARE) VDDR_LastError = VMDKR_ERR_SHARE;
      else VDDR_LastError = VMDKR_ERR_NOEXTENT;
   } else {
      pe->ExtentSize = ped->sectors; // in sectors.
      pe->type = ped->type;
      if (ped->type==VMDK_EXT_TYPE_SPARSE) {
         VDDR_LastError = VMDKR_ERR_READ;
         if (File_RdBin(pe->f, &pe->hdr, sizeof(VMDK_SPARSE_HEADER))==sizeof(VMDK_SPARSE_HEADER)) {
            VDDR_LastError = VMDKR_ERR_COMPRESSION;
            if (pe->hdr.compressAlgorithm==0 && pe->hdr.compressAlgorithmHi==0) {
               UINT mapsize;

               pe->SectorsPerBlock = pe->hdr.grainSize;  // VMDK spec says that a sparse extent grain/block size is always a power of 2, >=4096.
               pe->SPBshift = PowerOfTwo(pe->SectorsPerBlock);
               pe->nBlocks = (pe->ExtentSize + (pe->SectorsPerBlock-1)) >> pe->SPBshift;
               pe->nBlocksAllocated = 0; // TBD by ValidateMap.

               mapsize = pe->nBlocks*sizeof(UINT);
               pe->blockmap = Mem_Alloc(0, mapsize);
               VDDR_LastError = VMDKR_ERR_OUTOFMEM;
               if (pe->blockmap) {
                  VDDR_LastError = VMDKR_ERR_SEEK;
                  File_Seek(pe->f, pe->hdr.gdOffset<<9);
                  OSLastError = File_IOresult();
                  if (OSLastError==0) {
                     HUGE gtoffset=0;
                     VDDR_LastError = VMDKR_ERR_READ;
                     if (File_RdBin(pe->f, &gtoffset, 4)==4) {
                        VDDR_LastError = VMDKR_ERR_SEEK;
                        File_Seek(pe->f, gtoffset<<9);
                        OSLastError = File_IOresult();
                        if (OSLastError==0) {
                           VDDR_LastError = VMDKR_ERR_READ;
                           if (File_RdBin(pe->f, pe->blockmap, mapsize)==mapsize) {
                              VDDR_LastError = VMDKR_ERR_BLOCKMAP;
                              if (ValidateMap(pe)) VDDR_LastError = 0;
                           } else {
                              OSLastError = File_IOresult();
                           }
                        }
                     }
                  }
                  if (VDDR_LastError) pe->blockmap = Mem_Free(pe->blockmap);
               }
            }
         }
      } else {
         // We treat flat and zero extents as consisting of a single block whose size equals the extent size.
         pe->SectorsPerBlock = pe->ExtentSize;
         pe->nBlocks    = 1;
         pe->nBlocksAllocated = 1;
         pe->blockmap   = NULL;
         VDDR_LastError = 0;
      }
   }
   return (VDDR_LastError==0);
}

/*.....................................................*/

static void
OpenCommon(CPFN fn, PVMDK pVMDK)
// Open/init stuff common to VMDK and RAW.
{
   static char path[4096],extfn[4096];
   int i;

   VDDR_LastError = 0;
   pVMDK->Base.GetDriveType       = VMDKR_GetDriveType;
   pVMDK->Base.GetDriveSize       = VMDKR_GetDriveSize;
   pVMDK->Base.GetDriveBlockCount = VMDKR_GetDriveBlockCount;
   pVMDK->Base.BlockStatus        = VMDKR_BlockStatus;
   pVMDK->Base.GetDriveUUID       = VMDKR_GetDriveUUID;
   pVMDK->Base.IsSnapshot         = VMDKR_IsSnapshot;
   pVMDK->Base.ReadPage           = VMDKR_ReadPage;
   pVMDK->Base.ReadSectors        = VMDKR_ReadSectors;
   pVMDK->Base.Close              = VMDKR_Close;
   Filename_SplitPath(fn,path,NULL);
   for (i=0; i<pVMDK->hdr->nExtents; i++) {
      if (!pVMDK->extent[i].buff) { // if not dummy extent (i.e. not memory image of new track0 and MBR)
         PSTR psz = pVMDK->hdr->extdes[i].fn;
         if (psz[0]=='\\' || psz[1]==':') String_Copy(extfn,psz,4096); // check for absolute path to extent file.
         else Filename_MakePath(extfn,path,psz); // else assume tail or relative path.
         if (!OpenExtent(extfn,pVMDK->hdr->extdes+i,pVMDK->extent+i)) break;
      }
      pVMDK->DriveSectors += pVMDK->extent[i].ExtentSize;
      pVMDK->SectorsAllocated += (pVMDK->extent[i].ExtentSize);
      if (pVMDK->hdr->extdes[i].type==VMDK_EXT_TYPE_SPARSE) {
         pVMDK->hdr->extdes[i].BlkSize = (pVMDK->extent[i].SectorsPerBlock<<9);
         pVMDK->hdr->extdes[i].nBlocks = pVMDK->extent[i].nBlocks;
         pVMDK->hdr->extdes[i].nAlloc  = pVMDK->extent[i].nBlocksAllocated;
      }
   }
   if (VDDR_LastError) { // close all the extents again!
      for (i=0; i<pVMDK->hdr->nExtents; i++) {
         if (pVMDK->extent[i].buff) Mem_Free(pVMDK->extent[i].buff);
         else {
            FILE f = pVMDK->extent[i].f;
            if (f==((FILE)0) || f==NULLFILE) break;
            File_Close(f);
            Mem_Free(pVMDK->extent[i].blockmap);
         }
      }
   }
}

/*.....................................................*/

PUBLIC HVDDR
VMDKR_Open(CPFN fn, UINT iChain)
{
   PVMDK pVMDK = NULL;
   FILE f = File_OpenRead(fn);
   VDDR_LastError = VMDKR_ERR_FILENOTFOUND;
   if (f!=NULLFILE) {
      UINT bytes;
      BOOL bEmbedded;
      PSTR pMem = ReadDescriptor(f,&bytes,&bEmbedded);
      File_Close(f);
      if (pMem) {
         VMDK_HEADER *pVMDKhdr = Mem_Alloc(MEMF_ZEROINIT,sizeof(VMDK_HEADER));
         pVMDKhdr->CIDParent = 0xFFFFFFFF;
         pVMDKhdr->nMaxExtents = VMDK_MAX_EXTENTS;
         VDDR_LastError = VMDKR_ERR_OUTOFMEM;
         if (pVMDKhdr && ParseDescriptor(&pVMDKhdr,pMem,bytes)) { // IMPORTANT: ParseDescriptor discards pMem whether it succeeds or fails.
            UINT bytes = sizeof(VMDK_INFO)-(VMDK_MAX_EXTENTS*sizeof(VMDK_EXTENT)) + pVMDKhdr->nExtents*sizeof(VMDK_EXTENT);
            pVMDK = Mem_Alloc(MEMF_ZEROINIT,bytes);
            pVMDK->hdr = pVMDKhdr;
            pVMDK->DiskType = VDD_TYPE_VMDK;
            if (bEmbedded) String_Copy(pVMDK->hdr->extdes[0].fn, fn, 2048);
            OpenCommon(fn,pVMDK);
         }

         if (VDDR_LastError==0 && VMDKR_IsSnapshot((HVDDR)pVMDK)) {
            pVMDK->hVDIparent = VDDR_OpenByUUID(&pVMDK->hdr->uuidParent,iChain+1); // may set VDDR_LastError
            if (iChain==0) VDDR_LastError = 0; // we let the user open the first in a broken snapshot chain for diagnostic purposes, though we don't let him clone it.
         }

         if (VDDR_LastError) {
            Mem_Free(pVMDKhdr);
            pVMDK = Mem_Free(pVMDK);
         }
      }
   }
   return (HVDDR)pVMDK;
}

/*.....................................................*/

PUBLIC BOOL
VMDKR_GetDriveSize(HVDDR pThis, HUGE *drive_size)
{
   if (pThis) {
      PVMDK pVMDK = (PVMDK)pThis;
      *drive_size = (pVMDK->DriveSectors<<9);
      VDDR_LastError = 0;
      return TRUE;
   }
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

PUBLIC BOOL
VMDKR_GetDriveUUID(HVDDR pThis, S_UUID *drvuuid)
{
   if (pThis) {
      PVMDK pVMDK = (PVMDK)pThis;
      if (pVMDK->hVDIparent) return pVMDK->hVDIparent->GetDriveUUID(pVMDK->hVDIparent, drvuuid);
      VDDR_LastError = 0;
      Mem_Copy(drvuuid, &pVMDK->hdr->uuidCreate, sizeof(S_UUID));
      return TRUE;
   }
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

PUBLIC BOOL
VMDKR_GetHeader(HVDDR pThis, VMDK_HEADER *vmdkh)
{
   if (pThis) {
      PVMDK pVMDK = (PVMDK)pThis;
      VDDR_LastError = 0;
      Mem_Copy(vmdkh, pVMDK->hdr, sizeof(VMDK_HEADER));
      return TRUE;
   }
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   return FALSE;
}

/*.....................................................*/

static int
RawReadPage(PEXTENT pe, void *buffer, UINT iPage, UINT offset, UINT length)
// Read a block/page using the native block size from a VMDK extent. "offset"
// is a sector offset within the block selected by "iPage". Length is in bytes.
{
   VDDR_LastError = VMDKR_ERR_INVBLOCK;
   if (iPage<pe->nBlocks) {
      UINT SID;

      VDDR_LastError = 0;

      if (pe->type == VMDK_EXT_TYPE_ZERO) {
         return VDDR_RSLT_BLANKPAGE;
      } else if (pe->type == VMDK_EXT_TYPE_SPARSE) {
         SID = pe->blockmap[iPage];
         if (SID==VMDK_PAGE_FREE) return VDDR_RSLT_NOTALLOC;
      } else { // flat
         SID = iPage;
      }

      SID += offset;
      
      if (pe->buff) {
         // dummy extent
         VDDR_LastError = 0;
         Mem_Copy(buffer, pe->buff+(SID<<9), length);
         return VDDR_RSLT_NORMAL;
      } else {
         VDDR_LastError = VMDKR_ERR_SEEK;
         OSLastError = DoSeek(pe->f, ((HUGE)SID)<<9);
         if (OSLastError==0) {
            VDDR_LastError = VMDKR_ERR_READ;
            if (File_RdBin(pe->f, buffer, length)==length) {
               VDDR_LastError = 0;
               return VDDR_RSLT_NORMAL;
            }
            OSLastError = File_IOresult();
         }
      }
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
VMDKR_ReadSectors(HVDDR pThis, void *buffer, HUGE LBA, UINT nSectors)
{
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   if (pThis) {
      BYTE *pDest=buffer;
      PVMDK pVMDK = (PVMDK)pThis;
      PEXTENT pe;
      UINT SectorsToCopy,Offset,iPage,BytesToCopy;
      HUGE LBAoffset = LBA;
      int  rslt,iExtent;

      if (nSectors==0) return VDDR_RSLT_NORMAL;

      // find the extent the first LBA falls into.
      pe = pVMDK->extent;
      for (iExtent=0; iExtent<pVMDK->hdr->nExtents; iExtent++) {
         if (LBAoffset<pe->ExtentSize) break;
         LBAoffset -= pe->ExtentSize;
         pe++;
      }
      if (iExtent==pVMDK->hdr->nExtents) {
         VDDR_LastError = VMDKR_ERR_INVBLOCK;
         return VDDR_RSLT_FAIL;
      }

      if (pe->type==VMDK_EXT_TYPE_SPARSE) {
         // These operations are valid, because sparse extent sizes are always a power of two sectors in length.
         iPage  = LO32(LBAoffset) >> pe->SPBshift;
         Offset = LO32(LBAoffset) & (pe->SectorsPerBlock-1);
      } else {
         iPage = 0;
         Offset = LO32(LBAoffset);
      }

      rslt   = VDDR_RSLT_NOTALLOC;

      do {
         SectorsToCopy = pe->SectorsPerBlock - Offset;
         if (SectorsToCopy>nSectors) SectorsToCopy = nSectors;

         BytesToCopy = (SectorsToCopy<<9);
         pVMDK->PageReadResult = RawReadPage(pe, pDest, iPage, Offset, BytesToCopy);
         if (pVMDK->PageReadResult==VDDR_RSLT_NOTALLOC && pVMDK->hVDIparent) {
            // reading from unallocated page in snapshot child: pass read request to parent VDI.
            pVMDK->PageReadResult = pVMDK->hVDIparent->ReadSectors(pVMDK->hVDIparent,pDest,LBA,SectorsToCopy);
         }
         if (pVMDK->PageReadResult == VDDR_RSLT_FAIL) return VDDR_RSLT_FAIL;
         if (pVMDK->PageReadResult != VDDR_RSLT_NORMAL) Mem_Zero(pDest,BytesToCopy);
         if (rslt>pVMDK->PageReadResult) rslt = pVMDK->PageReadResult;
         nSectors -= SectorsToCopy;
         if (!nSectors) break;
         LBA += SectorsToCopy;
         pDest += BytesToCopy;
         Offset = 0;
         iPage++;
         if (iPage==pe->nBlocks) {
            pe++; iExtent++; iPage=0;
            if (iExtent==pVMDK->hdr->nExtents) return rslt; // return with partial result when we reach EOF.
         }

      } while (nSectors);

      return rslt;
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC int
VMDKR_ReadPage(HVDDR pThis, void *buffer, UINT iPage, UINT SPBshift)
{
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   if (pThis) {
      HUGE LBA;
      hugeop_fromuint(LBA,iPage);
      hugeop_shl(LBA,LBA,SPBshift);
      return VMDKR_ReadSectors(pThis, buffer, LBA, 1<<SPBshift);
   }
   return VDDR_RSLT_FAIL;
}

/*.....................................................*/

PUBLIC UINT
VMDKR_GetDriveType(HVDDR pThis)
{
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   if (pThis) {
      PVMDK pVMDK = (PVMDK)pThis;
      return pVMDK->DiskType;
   }
   return VDD_TYPE_VMDK;
}

/*.....................................................*/

static BOOL
IsNullUUID(S_UUID *uuid)
{
   int i;
   for (i=0; i<4; i++) {
      if (uuid->au32[i]) return FALSE;
   }
   return TRUE;
}

/*.....................................................*/

PUBLIC BOOL
VMDKR_IsSnapshot(HVDDR pThis)
{
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   if (pThis) {
      PVMDK pVMDK = (PVMDK)pThis;
      VDDR_LastError = 0;
      if (pVMDK->hVDIparent) return FALSE; // if we've already resolved the snapshot then don't alarm the app...
//    return (pVMDK->hdr->CIDParent!=0xFFFFFFFF);
      return !IsNullUUID(&pVMDK->hdr->uuidParent);
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC UINT
VMDKR_GetDriveBlockCount(HVDDR pThis, UINT SPBshift)
// VMDK can use different block sizes in each extent. For API purposes I'll
// pretend that it uses 4K blocks throughout.
{
   if (pThis) {
      PVMDK pVMDK = (PVMDK)pThis;
      VDDR_LastError = 0;
      return LO32(pVMDK->DriveSectors>>SPBshift);
   }
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   return 0;
}

/*.....................................................*/

static UINT
BlockStatus(PVMDK pVMDK, HUGE LBA_start, HUGE LBA_end)
// See comment for VMDK_AllocatedBlocks().
{
   UINT rslt = VDDR_RSLT_NOTALLOC;
   int  nSectors,stepsize,iExtent;
   PEXTENT pe;
   HUGE LBA;

   // find the extent the first LBA falls into.
   LBA = LBA_start;
   pe = pVMDK->extent;
   for (iExtent=0; iExtent<pVMDK->hdr->nExtents; iExtent++) {
      if (LBA<pe->ExtentSize) break;
      LBA -= pe->ExtentSize;
      pe++;
   }
   if (iExtent<pVMDK->hdr->nExtents) {
      // our first step will align us with the first sector of the next grain. That ensures that
      // we won't overshoot first grain on next extent if the grain size there shrinks drastically.
      stepsize = pe->SectorsPerBlock - (LO32(LBA) & (pe->SectorsPerBlock-1));

      nSectors = ((int)(LBA_end - LBA_start))+1;
      while (nSectors>0) {
         if (LBA >= pe->ExtentSize) {
            LBA -= pe->ExtentSize;
            pe++; iExtent++;
            if (iExtent==pVMDK->hdr->nExtents) break;
            stepsize = pe->SectorsPerBlock;
         }
         if (pe->type==VMDK_EXT_TYPE_FLAT) return VDDR_RSLT_NORMAL;
         if (pe->type==VMDK_EXT_TYPE_ZERO) rslt = VDDR_RSLT_BLANKPAGE;
         if (pe->type==VMDK_EXT_TYPE_SPARSE) {
            UINT SID = LO32(LBA>>pe->SPBshift);
            if (pe->blockmap[SID]!=VMDK_PAGE_FREE) return VDDR_RSLT_NORMAL;
            if (pVMDK->hVDIparent) {
               UINT sub_rslt = BlockStatus((PVMDK)(pVMDK->hVDIparent), LBA_start, LBA_start+stepsize-1);
               if (sub_rslt==VDDR_RSLT_NORMAL) return sub_rslt;
               if (rslt==VDDR_RSLT_NOTALLOC) rslt = sub_rslt;
            }
         }
         LBA_start += stepsize; // absolute
         LBA += stepsize;
         nSectors -= stepsize;
         stepsize = pe->SectorsPerBlock;
      }
   }
   return rslt;
}

/*.....................................................*/

PUBLIC HVDDR
VMDKR_Close(HVDDR pThis)
{
   VDDR_LastError = 0;
   if (pThis) { // we silently handle closing of an already closed file.
      int i;
      PVMDK pVMDK = (PVMDK)pThis;
      for (i=0; i<pVMDK->hdr->nExtents; i++) {
         if (pVMDK->extent[i].buff) Mem_Free(pVMDK->extent[i].buff);
         else {
            File_Close(pVMDK->extent[i].f);
            Mem_Free(pVMDK->extent[i].blockmap);
         }
      }
      Mem_Free(pVMDK->hdr);
      Mem_Free(pVMDK);
   }
   return NULL;
}

/*.....................................................*/

PUBLIC UINT
VMDKR_BlockStatus(HVDDR pThis, HUGE LBA_start, HUGE LBA_end)
{
   VDDR_LastError = VMDKR_ERR_INVHANDLE;
   if (pThis) {
      VDDR_LastError = 0;
      return BlockStatus((PVMDK)pThis,LBA_start,LBA_end);
   }
   return FALSE;
}

/*....................................................*/

static BOOL
IsMBR(BYTE *MBR)
{
   PPART pPart = (PPART)(MBR+446);
   if (pPart->Status==0 || pPart->Status==0x80) {
      if (/*pPart->loStartLBA==63 &&*/ pPart->hiStartLBA==0) {
         return TRUE;
      }
   }
   return FALSE;
}

/*....................................................*/

static void
CalcCHSEnd(PPART pPart, UINT PartEndLBA, UINT nSectors)
// Calculate the CHS end address of a partition which is nSectors long.
// Max values for CHS are, H=255, SPT=63, CYL=1023.
//
// To calculate LBA given CHS:  LBA = (C*HEADS + H)*SPT + S - 1
// To calculate CHS given LBA:
//   C = LBA / (HEADS * SPT)
//   temp = LBA % (HEADS * SPT)
//   H = temp / SPT
//   S = (temp % SPT)+1
//
// A complication when working with virtual drives is that the calculation
// requires the logical drive geometry, which for a virtual drive is not
// fixed - we have to invent a reasonable geometry instead.
{
   UINT cHeads,C,H,S,temp;

   if (PartEndLBA >= (1023*254*63)) {
      C = 1023;
      H = 254;
      S = 63;
   } else {
      // calculate cHeads parameter of drive geometry
      if (nSectors >= (1024*255*63)) cHeads = 255;
      else {
         C = nSectors / (16*63);
         cHeads = 16;
         while (C>1024) {
            cHeads += cHeads;
            C >>= 1;
         }
         if (cHeads>255) cHeads = 255;
      }

      // apply cHeads to calculate CHS equivalent of PartEndLBA.
      C = PartEndLBA / (cHeads*63);
      temp = PartEndLBA - (C*cHeads*63);
      H = temp / 63;
      S = (temp - H*63)+1;
   }

   pPart->pend_chs_cyl  = (BYTE)(C & 0xFF);
   pPart->pend_chs_head = (BYTE)H;
   pPart->pend_chs_sect = (BYTE)((S & 0x3F) | ((C>>2)&0xC0));
}

/*....................................................*/

static void
CreateDummyExtent(PVMDK pVMDK, UINT ExtType, BYTE *data, UINT nSectors)
{
   UINT i = pVMDK->hdr->nExtents;
   pVMDK->hdr->nExtents++;
   pVMDK->hdr->extdes[i].flags   = 0;
   pVMDK->hdr->extdes[i].sectors = nSectors;
   pVMDK->hdr->extdes[i].iomode  = VMDK_EXT_IOMODE_RW;
   pVMDK->hdr->extdes[i].type    = ExtType;
   pVMDK->hdr->extdes[i].fn[0]   = (BYTE)0;
   pVMDK->extent[i].type        = ExtType;
   pVMDK->extent[i].ExtentSize  = nSectors;
   pVMDK->extent[i].nBlocks     = 1;
   pVMDK->extent[i].nBlocksAllocated = 1;
   pVMDK->extent[i].SectorsPerBlock = nSectors;
   pVMDK->extent[i].SPBshift    = 0;
   pVMDK->extent[i].buff        = data;
}

/*....................................................*/

static BOOL
CheckRawFileType(PVMDK pVMDK, BYTE *MBR, UINT nSectors, UINT DiskSectors)
// Users may try to create a VDI using a partition image instead of
// a disk image. This function tries to identify whether the first
// sector (in the MBR buff) is in fact an MBR. If not we assume that
// 'MBR' in fact contains a partition boot sector, and we try to
// identify the filesystem type. If we succeed then we create
// a dummy track0 for the partition, with a valid MBR.
//
// We don't bother trying to handle a Linux partition since it
// probably needs a grub/swap partition to go with it anyway, and
// I don't yet know how to synthesize those.
{
   if (MBR[510]==0x55 && MBR[511]==0xAA) {
      if (!IsMBR(MBR)) {
         UINT FileSystem = 0;
         char sz[16];
         Mem_Copy(sz,MBR+3,8);
         sz[8] = (char)0;
         if (String_Compare(sz,"NTFS    ")==0) {
            FileSystem = 7;
         } else {
            sz[5] = (char)0;
            if (String_Compare(sz,"MSDOS")==0 || String_Compare(sz,"MSWIN")==0 || String_Compare(sz,"FRDOS")==0) {
               Mem_Copy(sz, MBR+82, 8);
               sz[8] = (char)0;
               if (String_Compare(sz,"FAT32   ")==0) FileSystem = 11;
               else {
                  Mem_Copy(sz, MBR+54, 8);
                  sz[8] = (char)0;
                  if (String_Compare(sz,"FAT16   ")==0 || String_Compare(sz,"FAT     ")==0) FileSystem=6;
               }
            }
         }

         if (FileSystem) {
            BYTE *pTrack0 = Mem_Alloc(MEMF_ZEROINIT, 63*512);
            PPART pPart;
            UINT i;
            if (!Template_MBR) {
               Template_MBR = Env_LoadBinData("MBR_TEMPLATE");
            }
            Mem_Copy(pTrack0, Template_MBR, 512);
            for (i=440; i<444; i++) pTrack0[i] = (BYTE)Random_Integer(256); // disk signature used by NT and others.
            pPart = (PPART)(pTrack0+446);
            pPart->Status   = (BYTE)0x80;
            pPart->pstart_chs_head = (BYTE)1;
            pPart->pstart_chs_sect = (BYTE)1;
            pPart->pstart_chs_cyl  = (BYTE)0;
            pPart->loStartLBA      = (BYTE)63;
            pPart->hiStartLBA      = (BYTE)0;
            pPart->PartType = (BYTE)FileSystem;
            pPart->loNumSectors = (WORD)(nSectors & 0xFFFF);
            pPart->hiNumSectors = (WORD)(nSectors >> 16);
            CalcCHSEnd(pPart, 63+nSectors-1, DiskSectors);
            CreateDummyExtent(pVMDK, VMDK_EXT_TYPE_FLAT, pTrack0, 63);
            return TRUE;
         }
      }
   }
   return FALSE;
}

/*....................................................*/

PUBLIC HVDDR
VMDKR_OpenRaw(CPFN fn, UINT iChain)
// When implementing VMDK support I noticed that the VMDK variant "monolithicFlat" is just a raw
// disk image plus a small textfile to describe it; and if I support /that/ then I could easily
// extend support to raw images without the descriptor (which I can create internally).
// Hence this function. It also turned out to be easy to support raw partition images (as opposed to
// whole disk images) - just treat the partition image as one flat extent, then create another
// extent in memory which precedes it and provides the MBR and track0, and an optional third extent
// on the end which just pads the drive size out to a nice round number if necessary.
//
// mod: I now also allow a physical drive to be opened.
{
   PVMDK pVMDK = NULL;
   FILE f = File_OpenRead(fn);
   VDDR_LastError = VMDKR_ERR_FILENOTFOUND;
   if (f!=NULLFILE) {
      UINT nSectors;
      BYTE MBR[512];
      if (Mem_Compare("\\\\.\\",fn,4)==0) {
         Env_GetDriveSectors(f,&nSectors);
      } else {
         HUGE fsize;
         File_Size(f,&fsize);
         nSectors = (UINT)(fsize>>9);
      }
      File_RdBin(f,MBR,512);
      File_Close(f);
      VDDR_LastError = VMDKR_ERR_NOTVMDK;
      if (nSectors>2048) {
         VMDK_HEADER *pVMDKhdr = Mem_Alloc(MEMF_ZEROINIT,sizeof(VMDK_HEADER));
         if (pVMDKhdr) pVMDK = Mem_Alloc(MEMF_ZEROINIT,sizeof(VMDK_INFO));
         VDDR_LastError = VMDKR_ERR_OUTOFMEM;
         if (pVMDK) {
            UINT i;
            UINT DiskSectors = nSectors+63;
            UINT PadSectors;
            BOOL bDummyExtent;
               
            pVMDK->hdr = pVMDKhdr;
            
            if (DiskSectors < (2048*2048)) {
               // if drive is less than 2GB then round up to nearest 1MB.
               PadSectors = ((DiskSectors+2047) & ~2047) - DiskSectors;
            } else {
               // if drive is greater than one gig then round up to nearest 1GB.
               PadSectors = ((DiskSectors+0x1FFFFF) & ~0x1FFFFF) - DiskSectors;
               if (PadSectors > (32*2048)) PadSectors=0;
            }
            DiskSectors += PadSectors;

            bDummyExtent = CheckRawFileType(pVMDK,MBR,nSectors,DiskSectors);
            pVMDK->DiskType = (bDummyExtent ? VDD_TYPE_PART_RAW : VDD_TYPE_RAW);
            
            // synthesize the missing VMDK descriptor data
            pVMDK->hdr->flags      = VMDK_HEADER_MUST_HAVE;
            pVMDK->hdr->version    = VMDK_FORMAT_VER;
            pVMDK->hdr->CIDParent  = 0xFFFFFFFF;
            pVMDK->hdr->CreateType = VMDK_CRTYPE_MONOFLAT;
            pVMDK->hdr->vhwver     = 4;

            // create an extent descriptor for the raw file
            i = pVMDK->hdr->nExtents;
            pVMDK->hdr->extdes[i].flags   = 0;
            pVMDK->hdr->extdes[i].sectors = nSectors;
            pVMDK->hdr->extdes[i].iomode  = VMDK_EXT_IOMODE_RW;
            pVMDK->hdr->extdes[i].type    = VMDK_EXT_TYPE_FLAT;
            String_Copy(pVMDK->hdr->extdes[i].fn, fn, 64);
            pVMDK->hdr->nExtents++;

            if (bDummyExtent && PadSectors) {
               CreateDummyExtent(pVMDK, VMDK_EXT_TYPE_ZERO, NULL, PadSectors);
            }

            OpenCommon(fn,pVMDK);

            if (VDDR_LastError) pVMDK = Mem_Free(pVMDK);
         }
      }
   }
   return (HVDDR)pVMDK;
}

/*.....................................................*/

PUBLIC BOOL
VMDKR_QuickGetUUID(CPFN fn, S_UUID *UUID)
{
   FILE f = File_OpenRead(fn);
   BOOL bOK = FALSE;
   VDDR_LastError = VMDKR_ERR_FILENOTFOUND;
   if (f!=NULLFILE) {
      UINT bytes;
      BOOL bEmbedded;
      PSTR pMem = ReadDescriptor(f,&bytes,&bEmbedded);
      File_Close(f);
      if (pMem) {
         VMDK_HEADER *pVMDKhdr = Mem_Alloc(MEMF_ZEROINIT,sizeof(VMDK_HEADER));
         pVMDKhdr->CIDParent = 0xFFFFFFFF;
         pVMDKhdr->nMaxExtents = VMDK_MAX_EXTENTS;
         VDDR_LastError = VMDKR_ERR_OUTOFMEM;
         if (pVMDKhdr && ParseDescriptor(&pVMDKhdr,pMem,bytes)) { // IMPORTANT: ParseDescriptor discards pMem whether it succeeds or fails.
            if (pVMDKhdr->flags & VMDK_PARSED_UUID) {
               Mem_Copy(UUID, &pVMDKhdr->uuidCreate, sizeof(S_UUID));
               bOK = TRUE;
            }
         }
            
         Mem_Free(pVMDKhdr);
         return bOK;
      }
   }
   return bOK;
}

/*.....................................................*/

/* end of module vmdkr.c */

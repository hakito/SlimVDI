/*================================================================================*/
/* Copyright (C) 2010, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "xml.h"
#include "djfile.h"
#include "djstring.h"
#include "mem.h"

#define BUFFSIZE           16384
#define MAXTOKLEN          4096
#define DATACACHE_SIZE     4096
#define HASH_TABLE_ENTRIES 1024 /* must be a power of 2, <= 65536 */
#define HASH_BITS          10   /* log2(HASH_TABLE_ENTRIES) */
#define MAX_DEPTH          64

#define CR  13
#define LF  10
#define TAB 9

// error codes
#define XML_ERR_OPENING      1  /* could not open input file */
#define XML_ERR_UNEX_EOF     2  /* unexpected EOF while parsing input file */
#define XML_ERR_BADNUM       3  /* badly formatted number */
#define XML_ERR_NLINSTRLIT   4  /* newline embedded in a string literal */
#define XML_ERR_STRTOOBIG    5  /* newline embedded in a string literal */
#define XML_ERR_BADCHAR      6  /* illegal character in markup */
#define XML_ERR_BADCMT       7  /* badly formed comment */
#define XML_ERR_EXPHDR       8  /* expected xml header */
#define XML_ERR_BADHDR       9  /* badly formatted xml header */
#define XML_ERR_EXPOPENTAG   10 /* expected open tag '<' char */
#define XML_ERR_EXPIDENT     11 /* expected an identifier */
#define XML_ERR_EXPEQUALS    12 /* expected '=' */
#define XML_ERR_EXPSTRLIT    13 /* expected string literal */
#define XML_ERR_IDMISMATCH   14 /* closing tag ident doesn't match opening tag */
#define XML_ERR_EXPCLOSETAG  15 /* expected close tag '>' char */

static UINT LastError;
static WORD CRCtable[256];

// file reading stuff - only valid while parsing a new file.
typedef enum {
   EndOfFile, Identifier, StringLit, IntLit, RealLit, HexLit,
   Lbrace, Rbrace, Colon, BoolTRUE, BoolFALSE, Equals,
   HeaderBegin, HeaderEnd, OpenTagBegin, CloseTagBegin, CloseTag, CloseEmptyTag
} Symbols;

static Symbols Sy;
static UINT prevc[4],prevc_sp,idx,BytesRead,Column,LineNo,errCol,errLine;
static BYTE *pBuff,*LastToken;
static FILE fin;

#define NODE_TYPE_HEADER    0
#define NODE_TYPE_ELEMENT   1
#define NODE_TYPE_ATTRIBUTE 2
#define NODE_TYPE(p) ((p)->flags & 0x3F)

// XML document defines a tree data structure, each node is of the following type *
typedef struct t_NODE {
   UINT  flags;               /* bottom six bits stores node type */
   struct t_NODE *pNext;      /* Next (sibling) node. In root nodes each link is to the root of another xml document */
   struct t_NODE *pNextHash;  /* Next node with the same hash index - ie. a collision list. */
   struct t_NODE *pParent;    /* pointer to parent node */
   struct t_NODE *pAttr;      /* pointer to the list of attributes associated with this node */
   struct t_NODE *pChild;     /* pointer to first child */
   PSTR  pszIdent;            /* pointer to identifier name for this node (NUL terminated) */
   PBYTE pValue;
} NODE, *PNODE;

// we have a special node to represent the XML object header
typedef struct {
   UINT  flags;
   PNODE pChild;             /* link to the root node of an XML document - note that our container can hold more than one */
   PNODE HashTable[HASH_TABLE_ENTRIES]; // hash table used to check whether a symbol name has been used before.
   UINT  dcUsed;             /* Bytes used in current data cache block. */
   PBYTE dcache;             /* pointer to current data cache */
   PVOID malloc_list;        /* linked list of allocate cache blocks */
} XMLINFO, *PXMLINF;

/*.....................................................*/

PUBLIC UINT
XML_GetLastError(void)
{
   return LastError;
}

/*.....................................................*/

PUBLIC PSTR
XML_GetLastErrorString(UINT nErr)
{
   if (nErr==0xFFFFFFFF) nErr = LastError;
   return NULL;
}

/*.....................................................*/

static UINT
Error(UINT nErr)
{
   LastError=nErr;
   return 0xFFFFFFFF;
}

/*.....................................................*/

static UINT
getc(void)
{
   UINT c;

   if (prevc_sp) {
      c = prevc[--prevc_sp];
      return c;
   }

try_again:
   if (idx == BytesRead) {
      BytesRead = File_RdBin(fin,pBuff,BUFFSIZE);
      if (BytesRead==0) return Error(XML_ERR_UNEX_EOF);
      idx = 0;
   }

   c = pBuff[idx++];
   if (c==LF) {
      Column=1; LineNo++;
   } else if (c==CR) {
      goto try_again;
   } else if (c==TAB) {
      Column = ((Column-1) & 0xFFFFFFF8)+9;
      c = ' ';
   } else {
      Column++;
   }
   return c;
}

/*................................................*/

#define PushBack(c) prevc[prevc_sp++]=(c)

/*................................................*/

static UINT
Integer(UINT c, PSTR LastToken)
{
   UINT i = 0;
   if (c=='-' || c=='+') {
      if (c=='-') LastToken[i++] = (char)c;
      c = getc();
   }
   if (c<'0' || c>'9') return Error(XML_ERR_BADNUM);
   else {
      do {
         LastToken[i++] = (char)c;
         c = getc();
      } while (c>='0' && c<='9');
   }
   LastToken[i] = 0;
   return c;
}

/*................................................*/

static void
Number(UINT c)
{
   Sy = IntLit;
   c = Integer(c,LastToken);
   if (c=='.') { /* real number? */
      UINT i = String_Length(LastToken);
      Sy = RealLit;
      LastToken[i++] = '.';
      c = Integer(getc(),LastToken+i); /* read mantissa */
      if (c=='E' || c=='e') { /* exponent? */
         i = String_Length(LastToken);
         LastToken[i++] = 'E';
         c = Integer(getc(),LastToken+i);
      }
   }
   PushBack(c);
}

/*................................................*/

static UINT
StringLiteral(UINT quotechar)
{
   UINT c,len = 0;
   for (c=getc(); c!=quotechar; c=getc()) {
      if (c==0xFFFFFFFF) return c;
      if (c==LF) return Error(XML_ERR_NLINSTRLIT);
      if ((len+1)==MAXTOKLEN) return Error(XML_ERR_STRTOOBIG);
      LastToken[len++] = (BYTE)c;
   }
   LastToken[len] = 0;
   Sy = StringLit;
   return 0;
}

/*................................................*/

static void
GetIdentifier(UINT c)
{
   UINT len=0;
   Sy = Identifier;
   LastToken[len++] = (BYTE)c;
   for (;;) {
      c = getc();
      if ((c=='_') || (c>='0' && c<='9') || (c>='A' && c<='Z') || (c>='a' && c<='z')) LastToken[len++]=(BYTE)c;
      else break;
   }
   LastToken[len] = (BYTE)0;
   PushBack(c);

   if (String_CompareCI(LastToken,"TRUE")==0) Sy=BoolTRUE;
   if (String_CompareCI(LastToken,"FALSE")==0) Sy=BoolFALSE;
}

/*................................................*/

static BOOL
Comment(void)
{
   // On entry we've already seen the '<!' sequence to begin a comment, we
   // need to parse the remainder of the comment opening, then skip until we
   // see the comment close sequence.
   //
   UINT state,c;
   
   if (getc()!='-') {Error(XML_ERR_BADCMT); return FALSE;}
   if (getc()!='-') {Error(XML_ERR_BADCMT); return FALSE;}

   state = 0;
   do {
      c = getc();
      if (c==0xFFFFFFFF) return FALSE;
      switch (state) {
         case 0:
            if (c=='-') state = 1;
            break;
         case 1:
            if (c=='-') state = 2;
            else state = 0;
            break;
         case 2:
            if (c=='>') state = 3;
            else if (c!='-') state = 0;
            break;
      }
   } while (state<3);
   return TRUE;
}

/*................................................*/

static Symbols
GetSymbol(void)
{
   UINT c;

again:
   Sy = EndOfFile;
   
   /* skip over whitespace */
   do c=getc(); while (c<=' ');

   /* mark symbol start position in case an error is detected */
   errCol = Column-1;
   errLine = LineNo;

   /* select appropriate tokenising function based on first character of token. */
   if (c>0xFFFF) return EndOfFile;
   else if (c>='0' && c<='9') Number(c);
   else if (c>='A' && c<='Z') GetIdentifier(c);
   else if (c>='a' && c<='z') GetIdentifier(c);
   else {
      switch (c) {
         case '+':
         case '-':
            Number(c);
            break;
         case '=':
            Sy = Equals;
            break;
         case '\'':
         case '"':
            StringLiteral(c);
            break;
         case '<':
            c = getc();
            if (c==0xFFFFFFFF) return EndOfFile;
            if (c=='?') {
               Sy = HeaderBegin;
            } else if (c=='/') {
               Sy = CloseTagBegin;
            } else if (c=='!') {
               if (Comment()) goto again;
               else Sy = EndOfFile;
            } else {
               PushBack(c);
               Sy = OpenTagBegin;
            }
            break;
         case '>':
            Sy = CloseTag;
            break;
         case '?':
            c = getc();
            if (c==0xFFFFFFFF) return EndOfFile;
            if (c=='>') Sy=HeaderEnd;
            else {
               PushBack(c);
               Error(XML_ERR_BADCHAR);
            }
            break;
         case '/':
            c = getc();
            if (c==0xFFFFFFFF) return EndOfFile;
            if (c=='>') Sy=CloseEmptyTag;
            else {
               PushBack(c);
               Error(XML_ERR_BADCHAR);
            }
            break;
         case ':':
            Sy = Colon;
            break;
         case '{':
            Sy = Lbrace;
            break;
         case '}':
            Sy = Rbrace;
            break;
         default:
            Error(XML_ERR_BADCHAR);
      }
   }

   return Sy;
}

/*................................................*/

static PVOID
AllocMemBlock(PXMLINF pXML, int bytes)
// Allocate a block of memory, and add it to the list of memory blocks
// associated with the object.
{
   PBYTE p;
   
   // round block allocation size up to 32 byte boundary, and add space for header
   bytes = ((bytes+31) & ~31)+32;

   p = Mem_Alloc(0,bytes);
   if (!p) return NULL;

   /* Add new allocation to the chain of allocated memory blocks */
   ((PVOID *)p)[0] = pXML->malloc_list;
   pXML->malloc_list = p;
   return p+32;
}

/*...................................................................*/

static void
FreeMemBlocks(PXMLINF pXML)
// Free all of the allocated heap memory blocks associated with this object.
{
   PVOID p,pNext;
   for (p=pXML->malloc_list; p; p=pNext) {
      pNext = ((PVOID *)p)[0];
      Mem_Free(p);
   }
   Mem_Free(pXML);
}

/*...................................................................*/

static PVOID
AllocMem(PXMLINF pXML, CPVOID pData, UINT len)
/* Allocate memory for the storage of arbitrary data. The allocation size is rounded
 * up to the next dword boundary. The memory can optionally be initialized from a
 * data template.
 */
{
   PVOID pMem;
   UINT nBytes = (len + 3) & ~3; // ensure dword alignment for efficiency
   if (nBytes>=(DATACACHE_SIZE/2)) {     // large allocations get their own block
      pMem = AllocMemBlock(pXML,nBytes);
   } else {                              // smaller allocations are suballocated from the curent cache
      if (pXML->dcUsed+nBytes > DATACACHE_SIZE) {
         pXML->dcache = AllocMemBlock(pXML,DATACACHE_SIZE);
         pXML->dcUsed = 0;
      }
      pMem = pXML->dcache+pXML->dcUsed;
      if (pData) Mem_Copy(pMem,pData,len);
      else Mem_Zero(pMem,nBytes);
      pXML->dcUsed += nBytes;
   }
   return pMem;
}

/*...................................................................*/

static BOOL
Header(void)
{
   LastError = XML_ERR_EXPHDR;
   if (GetSymbol()==HeaderBegin) {
      LastError = XML_ERR_BADHDR;
      GetSymbol();
      if (Sy!=Identifier) return FALSE;
      if (String_Compare(LastToken,"xml")) return FALSE;
      for (;;) {
         GetSymbol();
         if (Sy==HeaderEnd) {
            return TRUE;
         } else if (Sy==Identifier) {
            GetSymbol();
            if (Sy!=Equals) break;
            GetSymbol();
            if (Sy!=StringLit) break;
         }
      }
   }
   return FALSE;
}

/*................................................*/

static UINT
Hash(CPBYTE s, UINT len)
{
   UINT i,c,h=0xFFFF;
   for (i=0; i<len; i++) {
      c = s[i];
      h = (CRCtable[h>>8] ^ (h<<8) ^ c) & 0xFFFF;
   }
   return ((h & (HASH_TABLE_ENTRIES-1)) ^ (h>>HASH_BITS));
}

/*................................................*/

static PSTR
StoreSymbol(PXMLINF pXML, PNODE pNewNode, CPBYTE symbol)
// This is a neat trick (if I say so myself) I came up with a long time ago when
// writing a compiler. If you can arrange that symbol names are unique (only stored
// once, though they can be referenced by multiple objects), then that means
// that there's a one-to-one mapping between a string and its pointer, hence you
// can test strings for (in)equality by comparing pointers instead of calling a
// relatively slow string compare function. And, the same trick works for Unicode
// or ANSI strings, though I don't take advantage of that here.
{
   UINT slen = String_Length(symbol);
   UINT h = Hash(symbol,slen);       // use hash table to quickly look up a symbol
   PNODE pNode = pXML->HashTable[h];

   // A hashing scheme maps many string values to a common value, hence we now need
   // to check for an actual string match. If we find a match then we also found the
   // pointer which identifies that string.
   for (; pNode; pNode=pNode->pNextHash) { // run along collision list for this hash code
      if (String_Compare(pNode->pszIdent,symbol)==0) { // if got matching string..
         return pNode->pszIdent; // then return the pointer equivalent of the string.
      }
   }

   // symbol was not found, so we need to add a new entry to the collision list for
   // this hash code. This is why we need the pNewNode argument.
   pNewNode->pNextHash = pXML->HashTable[h];
   pXML->HashTable[h] = pNewNode;
   return AllocMem(pXML,symbol,slen+1);
}

/*................................................*/

static PNODE
FindSymbol(PXMLINF pXML, CPBYTE symbol)
// Find the first tree node that matches a given symbol string. "First" in this
// context means first in the hash table collision list, not necessarily the first
// occurrence of the string in the XML file (in fact the collision list will typically
// list strings in LIFO order).
{
   UINT h = Hash(symbol,String_Length(symbol));
   PNODE pNode = pXML->HashTable[h];
   for (; pNode; pNode=pNode->pNextHash) {
      if (String_Compare(pNode->pszIdent,symbol)==0) {
         return pNode; // symbol stored, return a node pointer.
      }
   }
   return NULL; // symbol not stored.
}

/*................................................*/

static void
ReadBodyData(PXMLINF pXML, PNODE pEle)
// TODO: Detect and convert &... escape codes.
{
   UINT c,i=0,iLastNonWhiteSpace=0;
   
   /* skip over leading whitespace */
   do c=getc(); while (c<=' ');

   while (c<0x100 && c!='<') {
      LastToken[i++] = (BYTE)c;
      if (c>32) iLastNonWhiteSpace = i;
      c = getc();
   }

   // truncate at last non-whitespace
   i = iLastNonWhiteSpace;
   if (i) {
      LastToken[i] = (BYTE)0; // NUL terminate the data.
      pEle->pValue = StoreSymbol(pXML,pEle,LastToken);
   }
   if (c=='<') PushBack(c);
}

/*................................................*/

static PNODE
Element(PXMLINF pXML, PNODE pParent)
{
   LastError = XML_ERR_EXPOPENTAG;
   if (Sy==OpenTagBegin) {
      LastError = XML_ERR_EXPIDENT;
      if (GetSymbol()==Identifier) {
         PNODE pEle = AllocMem(pXML,NULL,sizeof(NODE));
         PNODE pPrev = NULL;

         pEle->flags    = NODE_TYPE_ELEMENT;
         pEle->pParent  = pParent;
         pEle->pszIdent = StoreSymbol(pXML,pEle,LastToken);

         // parse the list of attributes associated with this xml element
         GetSymbol();
         while (Sy==Identifier) {
            PNODE pAttr = AllocMem(pXML,NULL,sizeof(NODE));
            pAttr->flags = NODE_TYPE_ATTRIBUTE;
            pAttr->pszIdent = StoreSymbol(pXML,pAttr,LastToken);
            pAttr->pParent = pEle;

            if (pPrev) pPrev->pNext = pAttr;
            else pEle->pAttr = pAttr;
            pPrev = pAttr;

            LastError = XML_ERR_EXPEQUALS;
            if (GetSymbol()!=Equals) return NULL;

            LastError = XML_ERR_EXPSTRLIT;
            if (GetSymbol()!=StringLit) return NULL;
            pAttr->pValue = AllocMem(pXML,LastToken,String_Length(LastToken)+1);

            GetSymbol();
         }

         if (Sy==CloseTag) {
            // normal tag close, means that now we have optional body data and possible child tags.
            ReadBodyData(pXML,pEle);
            pPrev = NULL;
            for (;;) {
               GetSymbol();
               if (Sy==OpenTagBegin) { // a child element
                  PNODE pChild = Element(pXML,pEle);
                  if (!pChild) return NULL;
                  if (pPrev) pPrev->pNext = pChild;
                  else pEle->pChild = pChild;
                  pPrev = pChild;
               } else {
                  break;
               }
            }
            if (Sy!=CloseTagBegin) {
               LastError = XML_ERR_EXPCLOSETAG;
               return NULL;
            } else {
               LastError = XML_ERR_EXPIDENT;
               if (GetSymbol()!=Identifier) return NULL;
               if (String_Compare(LastToken,pEle->pszIdent)) {
                  LastError = XML_ERR_IDMISMATCH;
                  return NULL;
               }
               LastError = XML_ERR_EXPCLOSETAG;
               if (GetSymbol()!=CloseTag) return NULL;
            }
         } else if (Sy!=CloseEmptyTag) {
            LastError = XML_ERR_EXPCLOSETAG;
            return NULL;
         }

         return pEle;
      }
   }
   return NULL;
}

/*................................................*/

static XMLDOC
Parse(void)
{
   if (Header()) {
      PXMLINF pXML = Mem_Alloc(MEMF_ZEROINIT, sizeof(XMLINFO));
      pXML->dcache = AllocMemBlock(pXML,DATACACHE_SIZE); // allocate an initial memory block
      GetSymbol();
      pXML->pChild = Element(pXML,(PNODE)pXML);
      if (pXML->pChild) {
         return (XMLDOC)pXML;
      }
      FreeMemBlocks(pXML);
   }
   return NULL;
}

/*................................................*/

PUBLIC XMLDOC
XML_Open(CPFN xmlfn)
{
   fin = File_OpenRead(xmlfn);
   if (fin == NULLFILE) Error(XML_ERR_OPENING);
   else {
      UINT i,j,crc;
      XMLDOC xml;
      
      /* init CRC table used by hash function */
      for (i=0; i<256; i++) {
         crc = (i<<8);
         for (j=0; j<8; j++) {
            if (crc & 0x8000) {
               crc = ((crc<<1) ^ 0x1021);
            } else {
               crc <<= 1;
            }
         }
         CRCtable[i] = (WORD)crc;
      }

      Column=1; LineNo=1; idx=0; BytesRead=0; prevc_sp=0;
      pBuff = Mem_Alloc(0,BUFFSIZE+MAXTOKLEN);
      LastToken = pBuff+BUFFSIZE;
      xml = Parse();
      File_Close(fin);
      return xml;
   }
   return NULL;
}

/*.....................................................*/

PUBLIC XMLDOC
XML_Close(XMLDOC hXML)
{
   if (hXML) FreeMemBlocks((PXMLINF)hXML);
   return NULL;
}

/*.....................................................*/

PUBLIC XMLELE
XML_FindElement(XMLDOC hXML, CPCHAR pszPath)
{
   if (hXML) {
      PSTR path[MAX_DEPTH];
      UINT d,depth=0;
      PNODE pNode=NULL,pTest;

      while (*pszPath) { // get exemplar string pointers for each element of the path
         pNode = FindSymbol((PXMLINF)hXML,pszPath);
         if (!pNode) return NULL; // if any substring doesn't exist then the path can't be valid.
         path[depth++] = pNode->pszIdent;
         pszPath += (String_Length(pszPath)+1);
      }

      // the above loop found a leaf node pNode which is the first (in the hash table collision
      // list) to match the leaf part of the path. We can now walk the tree to the root checking
      // for a name match at each level. If we don't find one then we step to the next node in
      // the collision list and try again.
      for (; pNode; pNode=pNode->pNextHash) { // for every element or attribute with a matching hash code

         if (NODE_TYPE(pNode)!=NODE_TYPE_ELEMENT) continue; // skip anything that isn't an element name (eg. attribute names).

         // got a matching element name. Walk the tree to the root, comparing string pointers for equality.
         d=depth; pTest=pNode;
         do {
            d--;
            if (path[d]!=pTest->pszIdent) break;
            pTest = pTest->pParent;
         } while (d);
         if (d==0 && pTest==(PNODE)hXML) {
            // gotcha
            return (XMLELE)pNode;
         }
      }
   }
   return NULL;
}

/*.....................................................*/

PUBLIC XMLELE
XML_FirstChild(XMLELE hEle)
{
   if (hEle) {
      PNODE pNode = (PNODE)hEle;
      if (NODE_TYPE(pNode)==NODE_TYPE_HEADER) {
         return (XMLELE)(((PXMLINF)pNode)->pChild);
      } else {
         return (XMLELE)(pNode->pChild);
      }
   }
   return NULL;
}

/*.....................................................*/

PUBLIC XMLELE
XML_NextChild(XMLELE hEle)
{
   if (hEle) {
      PNODE pNode = (PNODE)hEle;
      if (NODE_TYPE(pNode)!=NODE_TYPE_HEADER) {
         return (XMLELE)(pNode->pNext);
      }
   }
   return NULL;
}

/*.....................................................*/

PUBLIC PSTR
XML_Attribute(XMLDOC hXML, XMLELE hEle, CPSTR pszAttrName)
{
   if (hXML && hEle && pszAttrName) {
      PNODE pNode = FindSymbol((PXMLINF)hXML,pszAttrName); // find an exemplar string pointer.
      if (pNode) {
         PSTR psz = pNode->pszIdent; // use the pointer for comparisons.
         pNode = ((PNODE)hEle)->pAttr;
         for (; pNode; pNode=pNode->pNext) {
            if (pNode->pszIdent==psz) { // gotcha
               return (PSTR)(pNode->pValue);
            }
         }
      }
   }
   return NULL;
}

/*.....................................................*/

/* end of module xml.c */


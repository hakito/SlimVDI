/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "filename.h"

/*.....................................................*/

static BOOL
IsPathSeparator(FNCHAR c)
{
   return (c=='\\' || c=='/');
}

/*.....................................................*/

static void
CopyChars(PFN fnDest, CPFN fnSrc, int len)
{
   if (fnDest!=fnSrc) {
      for (; len; len--) *fnDest++ = *fnSrc++;
   }
}

/*.....................................................*/

PUBLIC int
Filename_Length(CPFN fn)
{
   int L;
   for (L=0; *fn; fn++) L++;
   return L;
}

/*.....................................................*/

PUBLIC int
Filename_Compare(CPFN s1, CPFN s2)
{
   int d,c1,c2;
   do {
      c1 = *s1++;
      if (c1>='a' && c1<='z') c1-=32;
      c2 = *s2++;
      if (c2>='a' && c2<='z') c2-=32;
      d = c1-c2;
      if (d!=0) return d;
   } while (c1);
   return 0;
}

/*.....................................................*/

PUBLIC void
Filename_SplitPath(CPFN path, PFN head, PFN tail)
/* Find the index of the rightmost path separator in the string "path", assuming the
 * index to be zero if a separator is not present - a path separator found at the
 * very end of the path string is not counted. Then return the characters to the
 * left of this selected index in the "head" substring, and the chars to the right of
 * the index in the "tail" substring. Note that "path" may legitimately be the same
 * app variable as "head" or "tail", so a local copy of "path" is required for
 * processing.
 */
{
   int L = Filename_Length(path);

   /* if the path section includes a drive spec then copy it to the head variable,
    * and remove it from the local path spec.
    */
   if (L>=2 && path[1]==':') {
      if (head) { head[0]=path[0]; head[1]=':'; head += 2; }
      path+=2;
      L-=2;
   }

   /* Remove redundant trailing path separators */
   while (L && IsPathSeparator(path[L-1])) L--;

   /* if the resulting path is blank then it must indicate the root directory -
    * we handle that as a special case.
    */
   if (L==0) {
      if (head) { head[0]='\\'; head[1]=0; }
      if (tail) tail[0]=0;
   } else {
      int i,j;

      /* now we are ready to search for the rightmost path separator */
      for (i=L; i && !IsPathSeparator(path[i-1]); i--);

      // at this point i is either 0, or the index of the first char beyond that last path separator.

      if (head) {
         j = i;
         if (j) CopyChars(head,path,j);
         if (j>1) j--; // delete last path separator, provided it doesn't indicate root folder.
         head[j] = (FNCHAR)0;
      }
      if (tail) {
         if (L>i) CopyChars(tail,path+i,L-i);
         tail[L-i] = (FNCHAR)0;
      }
   }
}

/*.....................................................*/

PUBLIC void
Filename_SplitTail(CPFN path, PFN tail)
{
   Filename_SplitPath(path,NULL,tail);
}

/*.....................................................*/

PUBLIC void
Filename_MakePath(PFN path, CPFN head, CPFN tail)
{
   int L = Filename_Length(head);
   int lenTail = Filename_Length(tail);
   if (L) CopyChars(path,head,L);
   if (lenTail) {
      if (L && head[L-1]!='\\' && tail[0]!='\\') path[L++] = '\\';
      if (IsPathSeparator(tail[0])) { tail++; lenTail--; }
      CopyChars(path+L,tail,lenTail);
      L += lenTail;
   }
   path[L] = (FNCHAR)0;
}

/*.....................................................*/

static UINT
ExtensionPos(CPFN s, int i)
// Returns position of first '.' to the left of position i in string s.
{
   if (i) {
      FNCHAR c;
      do {
         c = s[--i];
      } while (i && !IsPathSeparator(c) && (c!=':') && (c!='.'));
      if (c=='.') return i;
   }
   return 0xFFFFFFFF;
}

/*................................................*/

PUBLIC void
Filename_AddExtension(PFN s, CPFN ext)
{
   int L,extlen;
   if (!ext || !ext[0]) return;
   L = Filename_Length(s);
   extlen = Filename_Length(ext);
   if (ext[0]!='.') s[L++] = '.';
   CopyChars(s+L,ext,extlen);
   s[L+extlen] = (FNCHAR)0;
}

/*.....................................................*/

PUBLIC void
Filename_GetExtension(CPFN fn, PFN ext)
{
   UINT L = Filename_Length(fn);
   UINT p = ExtensionPos(fn,L);
   if (p==0xFFFFFFFF) ext[0]=0;
   else {
      p++;
      if ((L-p)>31) p = L; // a string that long probably isn't an extension.
      if (L>p) CopyChars(ext,fn+p,L-p);
      ext[L-p] = (FNCHAR)0;
   }
}

/*.....................................................*/

PUBLIC void
Filename_RemoveExtension(PFN s)
{
   UINT p = ExtensionPos(s,Filename_Length(s));
   if (p!=0xFFFFFFFF) s[p] = (FNCHAR)0;
}

/*.....................................................*/

PUBLIC void
Filename_ChangeExtension(PFN s, CPFN ext)
{
   Filename_RemoveExtension(s);
   Filename_AddExtension(s,ext);
}

/*.....................................................*/

PUBLIC BOOL
Filename_IsExtension(CPFN s, CPFN ext)
{
   FNCHAR curr_ext[32];
   Filename_GetExtension(s,curr_ext);
   return (Filename_Compare(curr_ext,ext)==0);
}

/*.....................................................*/

PUBLIC int
Filename_Copy(PFN fnDest, CPFN fnSrc, int MaxLen)
{
   int len = Filename_Length(fnSrc)+1;
   if (len>MaxLen) len = MaxLen;
   CopyChars(fnDest, fnSrc, len);
   return len;
}

/*.............................................*/

/* end of module filename.c */


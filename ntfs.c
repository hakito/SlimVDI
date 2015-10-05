/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#include "djwarning.h"
#include "djtypes.h"
#include "ntfs.h"
#include "vddr.h"
#include "ntfs_struct.h"
#include "memfile.h"
#include "mem.h"
#include "djfile.h"
#include "djstring.h"
#include "partinfo.h"
#include "cow.h"

#define CLUSTERS_PER_BUFF 16

typedef struct {
   CLASS(FSYS) Base;
   HVDDR hVDIsrc;
   NTFS_BOOT_SECTOR boots;
   UINT ClusterSize;
   UINT BitmapBytes;
   BYTE *cluster; // buffer for reading clusters.
   UINT *Bitmap;
} NTFSVOLINF, *PNTFSVOL;

static BYTE raw_boot_sector[512];

/*.....................................................*/
#if 0
static void
DumpData(PSTR fn, void *data, UINT len)
{
   FILE f = File_Create(fn,DJFILE_FLAG_OVERWRITE);
   File_WrBin(f,data,len);
   File_Close(f);
}
#endif
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

static BOOL
UnicodeNameMatch(WORD *name1, WORD *name2, UINT len)
// Can't use lstrcmpW because name1 may not be NUL terminated.
// This function requires that name2 *is* NUL terminated.
// This comparison is case sensitive.
{
   WORD c1,c2;
   for (; len; len--) {
      c1 = *name1++;
      c2 = *name2++;
      if (c1!=c2) return FALSE;
      if (c1==0) return TRUE; // since c1==c2 is TRUE at this point, if c1==NUL then so is c2.
   }
   return (len==0 && name2[len]==0);
}

/*.....................................................*/

static BOOL
ReadClusters(PNTFSVOL pNTFS, PVOID dest, HUGE LCN, UINT nClusters)
{
   HUGE LBA;
   hugeop_shl(LBA, LCN, pNTFS->boots.SectorsPerClusterShift);
   hugeop_add(LBA, LBA, pNTFS->boots.BootSectorLBA);
   return pNTFS->hVDIsrc->ReadSectors(pNTFS->hVDIsrc, dest, LBA, (nClusters<<pNTFS->boots.SectorsPerClusterShift));
}

/*.....................................................*/

static void
UnpackBootSector(NTFS_BOOT_SECTOR *boots, BYTE *buffer, HUGE iLBA, HUGE cLBA)
{
   Mem_Zero(boots,sizeof(NTFS_BOOT_SECTOR));
   Mem_Copy(boots->SystemID,buffer+3,8);
   boots->BytesPerSector    = MAKEWORD(buffer[11],buffer[12]);
   boots->SectorsPerCluster = buffer[0x0D];
   boots->SectorsPerClusterShift = PowerOfTwo(boots->SectorsPerCluster);
   boots->MediaDescriptor   = buffer[0x15];
   boots->SectorsPerTrack   = MAKEWORD(buffer[0x18],buffer[0x19]);
   boots->NumberOfHeads     = MAKEWORD(buffer[0x1A],buffer[0x1B]);
   Mem_Copy(&boots->MediaType,buffer+0x24,4);
   Mem_Copy(&boots->SectorsInVolume,buffer+0x28,8);
   Mem_Copy(&boots->LCN_MFT,buffer+0x30,8);
   Mem_Copy(&boots->LCN_MFTMirr,buffer+0x38,8);
   Mem_Copy(&boots->BytesPerMFTRec,buffer+0x40,4);
   if (boots->BytesPerMFTRec & 0x80) {
      boots->BytesPerMFTRec = (1<<(256-boots->BytesPerMFTRec));
   } else {
      boots->BytesPerMFTRec = (boots->BytesPerMFTRec<<boots->SectorsPerClusterShift)*boots->BytesPerSector;
   }
   Mem_Copy(&boots->BytesPerIndexRec,buffer+0x44,4);
   if (boots->BytesPerIndexRec & 0x80) {
      boots->BytesPerIndexRec = (1<<(256-boots->BytesPerIndexRec));
   } else {
      boots->BytesPerIndexRec = (boots->BytesPerIndexRec<<boots->SectorsPerClusterShift)*boots->BytesPerSector;
   }
   Mem_Copy(&boots->VolumeSerialNumber,buffer+0x48,8);
   boots->BootSectorLBA = iLBA;
   boots->TruePartitionSectors = cLBA;
   
   // Calculate LBA of first sector beyond the area we map. Note that we use the
   // SectorsInVolume field of the boot sector for this, not the cLBA argument.
   // This is because SectorsInVolume is rounded down to an integral number of
   // clusters, and since any residue may not be included in the cluster bitmap
   // we have no way of knowing if something is stored there or not.
   hugeop_add(boots->LastSectorLBA, boots->BootSectorLBA, boots->SectorsInVolume);
}

/*.....................................................*/

static BOOL
DoMstFixups(PNTFS_FILE_RECORD pFile)
// The OS writes a magic (update sequence) number into the last two bytes of every sector
// of the file record, storing the original values in an array elsewhere. This function
// restores the affected bytes to their correct values.
{
   UINT UpSeqOff = pFile->UpdateSeqOffset;
   UINT UpSeqLen = pFile->UpdateSeqLength;
   if (UpSeqLen>=2) {
      WORD *seq = (WORD*)(((PSTR)pFile) + UpSeqOff);
      WORD *pMagic,magic=seq[0];
      UINT i;

      // do the fixups. We check the validity of the sequence number as we go.
      pMagic = ((WORD*)pFile) + 255; // point at last two bytes in each sector.
      for (i=1; i<UpSeqLen; i++) {
         if (*pMagic != magic) return FALSE;
         *pMagic = seq[i];
         pMagic += 256;
      }

      pFile->UpdateSeqLength = 0; // mark the in-memory record as already having been fixed up.
   }
   return TRUE;
}

/*.....................................................*/

static PMFT_ATTRIBUTE
MFTFindAttribute(PNTFS_FILE_RECORD pFile, UINT idAttr)
{
   PMFT_ATTRIBUTE pAttr = (MFT_ATTRIBUTE*)(((PSTR)pFile) + pFile->FirstAttrOffset);
   do {
      if (pAttr->type == idAttr) return pAttr;
      pAttr = (MFT_ATTRIBUTE*)(((PSTR)pAttr)+pAttr->len);
   } while (pAttr->type != 0xFFFFFFFF);
   return NULL;
}

/*.....................................................*/

static PMFT_ATTRIBUTE
MFTFindNamedAttribute(PNTFS_FILE_RECORD pFile, UINT idAttr, WORD *name)
{
   PMFT_ATTRIBUTE pAttr = (MFT_ATTRIBUTE*)(((PSTR)pFile) + pFile->FirstAttrOffset);
   do {
      if (pAttr->type == idAttr) {
         if (!name || !name[0]) {
            if (pAttr->NameLength==0) return pAttr;
         } else if (pAttr->NameOffset) {
            WORD *pwName = (WORD*)(((PBYTE)pAttr)+pAttr->NameOffset);
            if (UnicodeNameMatch(pwName, name, pAttr->NameLength)) return pAttr;
         }
      }
      pAttr = (MFT_ATTRIBUTE*)(((PSTR)pAttr)+pAttr->len);
   } while (pAttr->type != 0xFFFFFFFF);
   return NULL;
}

/*.....................................................*/

static PNTFS_FILE_RECORD
MFTFindFile(PNTFSVOL pNTFS, WORD *name, BYTE *buffer, UINT bufferlen)
{
   BYTE *buffer_end = buffer + bufferlen;
   PNTFS_FILE_RECORD pFile;
   PMFT_ATTRIBUTE pAttr;

   while (buffer<buffer_end) {
      pFile = (PNTFS_FILE_RECORD)buffer;
      DoMstFixups(pFile);
      pAttr = MFTFindAttribute(pFile,MFT_ATTR_FILENAME);
      if (pAttr) {
         MFT_FILENAME *pfn = (MFT_FILENAME*)(((PSTR)(&pAttr->u.res.bPadding))+1+pAttr->NameLength*2);
         if (UnicodeNameMatch(pfn->Name,name,pfn->FileNameLen)) {
            return pFile;
         }
      }
      buffer += pNTFS->boots.BytesPerMFTRec;
   }
   return NULL;
}

/*.....................................................*/

static void
GetRunList(PNTFSVOL pNTFS, PNTFS_FILE_RECORD pFile, PMFT_ATTRIBUTE pAttr, UINT *pRunList)
/* Reads the runlist data from an MFT file record and returns it
 * as a list of UINT pairs <Cluster Number><Length in Clusters>,
 * end of list is indicated by a cluster number of 0xFFFFFFFF.
 *
 * See "NTFS_Structs.h" for an explanation of runlists and their encoding.
 */
{
   DoMstFixups(pFile);
   if (!pAttr) pAttr = MFTFindAttribute(pFile,MFT_ATTR_DATA);
   if (pAttr && pAttr->bNonResident) {
      BYTE *pRun = ((BYTE*)pAttr) + pAttr->u.nonres.DataRunOffset;
      BYTE *pDataEnd = ((BYTE*)pAttr)+pAttr->len;
      BYTE olb,Os,Ls;
      int  offset,shift;
      UINT length;
      HUGE LCN = 0;
      while (pRun < pDataEnd) {
         olb = *pRun++;
         if (olb==0) break;
         Ls = (BYTE)(olb & 0xF); // size of length field
         Os = (BYTE)(olb>>4);    // size of offset field

         length = 0;
         if (Ls) Mem_Copy(&length,pRun,Ls);
         offset = 0;
         if (Os) {
            Mem_Copy(&offset,pRun+Ls,Os);
            shift = ((4-Os)<<3);
            if (shift>0) offset = ((offset<<shift)>>shift); // sign extend the offset.
            hugeop_adduint(LCN,LCN,offset);
         }
         *pRunList++ = LO32(LCN);
         *pRunList++ = length;

         pRun += (Ls+Os);
      }
   }
   *pRunList++ = 0xFFFFFFFF;
}

/*.....................................................*/

static PVOID
DoReadFile(PNTFSVOL pNTFS, PNTFS_FILE_RECORD pFile, UINT *filelen)
// The only file I actually read from the NTFS volume is "$Bitmap", which is assumed to
// be contiguous, or at least not heavily fragmented, since it should have been created at max
// size when the volume was first formatted. Hence this function is not intended to be a complete
// solution to the problem of reading any possible NTFS file.
{
   PMFT_ATTRIBUTE pAttr;
   DoMstFixups(pFile);
   pAttr = MFTFindAttribute(pFile,MFT_ATTR_DATA);
   if (pAttr && pAttr->bNonResident) {
      HMEMFILE f = MemFile.Create(0);
      PBYTE buff = Mem_Alloc(0,pNTFS->ClusterSize*CLUSTERS_PER_BUFF);
      BYTE *pRun = ((BYTE*)pAttr) + pAttr->u.nonres.DataRunOffset;
      BYTE *pDataEnd = ((BYTE*)pAttr)+pAttr->len;
      BYTE olb,Os,Ls;
      int  offset,shift;
      UINT i,length,StartVCN = LO32(pAttr->u.nonres.StartVCN); // for simplicity, assume I can't have more than 2^31-1 clusters in one file.
      HUGE LCN;

      if (StartVCN) {
         Mem_Zero(buff,pNTFS->ClusterSize);
         for (i=0; i<StartVCN; i++) {
            MemFile.WrBin(f,buff,pNTFS->ClusterSize);
         }
      }

      LCN = 0;
      while (pRun < pDataEnd) {
         olb = *pRun++;
         if (olb==0) break;
         Ls = (BYTE)(olb & 0xF); // size of length field
         Os = (BYTE)(olb>>4);    // size of offset field
         length = 0;
         if (Ls) Mem_Copy(&length,pRun,Ls);
         if (Os) {
            HUGE absLCN;
            UINT ClustersToRead;

            offset = 0;
            Mem_Copy(&offset,pRun+Ls,Os);
            shift = ((4-Os)<<3);
            if (shift>0) offset = ((offset<<shift)>>shift); // sign extend the offset.
            hugeop_adduint(LCN,LCN,offset);

            // length clusters at offset LCN.
            absLCN=LCN;
            i = 0;
            while (i<length) {
               ClustersToRead = length-i;
               if (ClustersToRead>CLUSTERS_PER_BUFF) ClustersToRead = CLUSTERS_PER_BUFF;
               ReadClusters(pNTFS,buff,absLCN,ClustersToRead);
               MemFile.WrBin(f,buff,ClustersToRead*pNTFS->ClusterSize);
               hugeop_adduint(absLCN,absLCN,ClustersToRead);
               i += ClustersToRead;
            }
         } else {
            // a run of zeroed clusters (sparse file).
            Mem_Zero(buff,pNTFS->ClusterSize);
            for (i=0; i<length; i++) {
               MemFile.WrBin(f,buff,pNTFS->ClusterSize);
            }
         }
         pRun += (Ls+Os);
      }

      i=0; MemFile.WrBin(f,&i,4); // add 4 four bytes of padding on end (allows dword lookahead without buffer overrun).
      Mem_Free(buff);
      buff = MemFile.Extract(f,(int*)filelen);
      MemFile.Close(f);
      return buff;
   }
   return NULL;
}

/*.....................................................*/

PUBLIC BOOL
NTFS_IsNTFSVolume(HVDDR hVDI, HUGE iLBA)
{
   if (hVDI->ReadSectors(hVDI, raw_boot_sector, iLBA, 1)) {
      return (Mem_Compare(raw_boot_sector+3,"NTFS    ",8)==0);
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC HFSYS
NTFS_OpenVolume(HVDDR hVDI, HUGE iLBA, HUGE cLBA, UINT cSectorSize)
{
   if (NTFS_IsNTFSVolume(hVDI,iLBA)) { // this also reads the boot sector into the static "raw_boot_sector" buffer.
      PNTFSVOL pNTFS = Mem_Alloc(MEMF_ZEROINIT, sizeof(NTFSVOLINF));
      if (pNTFS) {
         pNTFS->Base.CloseVolume = NTFS_CloseVolume;
         pNTFS->Base.IsBlockUsed = NTFS_IsBlockUsed;
         pNTFS->hVDIsrc = hVDI;
         UnpackBootSector(&pNTFS->boots, raw_boot_sector, iLBA, cLBA);
         pNTFS->ClusterSize = pNTFS->boots.BytesPerSector*pNTFS->boots.SectorsPerCluster;
         pNTFS->cluster     = Mem_Alloc(0,pNTFS->ClusterSize*16);
         if (pNTFS->cluster) {
            PNTFS_FILE_RECORD pFile;
            ReadClusters(pNTFS, pNTFS->cluster, pNTFS->boots.LCN_MFT, 16);
            pFile = MFTFindFile(pNTFS, L"$Bitmap", pNTFS->cluster, pNTFS->ClusterSize*16);
//          DumpData("c:\\dj2\\MFT_original.bin",pNTFS->cluster,9*1024);
            if (pFile) {
               pNTFS->Bitmap = DoReadFile(pNTFS, pFile, &pNTFS->BitmapBytes);
//             DumpData("c:\\dj2\\Bitmap_after.bin",pNTFS->Bitmap,pNTFS->BitmapBytes-4);
               if (pNTFS->Bitmap) {
                  return (HFSYS)pNTFS;
               }
            }
            pNTFS->cluster = Mem_Free(pNTFS->cluster);
         }
      }
      Mem_Free(pNTFS);
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC HFSYS
NTFS_CloseVolume(HFSYS hNTFS)
{
   if (hNTFS) {
      PNTFSVOL pNTFS = (PNTFSVOL)hNTFS;
      Mem_Free(pNTFS->cluster);
      Mem_Free(pNTFS->Bitmap);
      Mem_Free(pNTFS);
   }
   return NULL;
}

/*.....................................................*/

static BOOL
SomeClustersUsed(PNTFSVOL pNTFS, UINT i, UINT nClusters)
// Return TRUE if any cluster LCN in the range from i to i+nClusters-1
// is in use.
{
   UINT *Bitmap = pNTFS->Bitmap + (i>>5);
   UINT nBits = (i & 0x1F); // count of unwanted lsbs
   UINT mask = (*Bitmap++) & (~((1<<nBits)-1)); // zero bit range from 0..[startbit-1] for first UINT only
   nClusters += nBits; // let main loop treat mask.bit0 onwards as valid
   for (; nClusters>=32; nClusters-=32) {
      if (mask) return TRUE;
      mask = (*Bitmap++);
   }
   if (nClusters) {
      if (mask & ((1<<nClusters)-1)) return TRUE;
   }
   return FALSE;
}

/*.....................................................*/

PUBLIC int
NTFS_IsBlockUsed(HFSYS hNTFS, UINT iBlock, UINT SectorsPerBlockShift)
{
   if (hNTFS) {
      PNTFSVOL pNTFS = (PNTFSVOL)hNTFS;
      UINT SectorsPerBlock = (1<<SectorsPerBlockShift);
      HUGE LBAstart, LBAend, htemp;

      LBAstart = MAKEHUGE(iBlock,0);

      hugeop_shl(LBAstart, LBAstart, SectorsPerBlockShift);
      if (hugeop_compare(LBAstart,pNTFS->boots.BootSectorLBA)>=0) { // if LBAstart>=BootSectorLBA ...
         LBAend = LBAstart;
         hugeop_adduint(LBAend, LBAend, SectorsPerBlock);
         if (hugeop_compare(LBAend,pNTFS->boots.LastSectorLBA)<=0) { // if LBAend<LastSectorLBA ...
            UINT iFirstCluster,iLastCluster;

            // convert start/end LBA to volume relative form.
            hugeop_sub(LBAstart,LBAstart,pNTFS->boots.BootSectorLBA);
            hugeop_sub(LBAend,LBAend,pNTFS->boots.BootSectorLBA);
            hugeop_shr(htemp, LBAstart, pNTFS->boots.SectorsPerClusterShift);
            iFirstCluster = LO32(htemp);
            hugeop_shr(htemp, LBAend, pNTFS->boots.SectorsPerClusterShift);
            iLastCluster = LO32(htemp);

            // we need to take account of the possibility that block boundaries are not
            // aligned with clusters (the LBAstart shift is safe).
            if (LO32(LBAend) & ((1<<(pNTFS->boots.SectorsPerClusterShift))-1)) iLastCluster++;

            if (SomeClustersUsed(pNTFS,iFirstCluster,iLastCluster-iFirstCluster)) return FSYS_BLOCK_USED;
            return FSYS_BLOCK_UNUSED;
         }
      }
   }
   return FSYS_BLOCK_OUTSIDE;
}

/*.....................................................*/

static void
MarkRunUnused(PNTFSVOL pNTFS, UINT i, UINT nClusters)
// Set a run of $Bitmap bits to 0 (unused).
{
   UINT *Bitmap = pNTFS->Bitmap;
   for (; nClusters; nClusters--,i++) {
      Bitmap[i>>5] &= ~(1<<(i & 0x1F));
   }
}

/*.....................................................*/

static void
MarkRunUsed(PNTFSVOL pNTFS, UINT i, UINT nClusters)
// Set a run of $Bitmap bits to 1 (in use).
{
   UINT *Bitmap = pNTFS->Bitmap;
   for (; nClusters; nClusters--,i++) {
      Bitmap[i>>5] |= (1<<(i & 0x1F));
   }
}

/*.....................................................*/

static UINT
FindFreeSpace(PNTFSVOL pNTFS, UINT iFirstCluster, UINT iLastCluster, UINT length)
// Find a contiguous run of free clusters to use for a file of the given length.
{
   if (length && length<iLastCluster) {
      UINT i=iFirstCluster, *Bitmap = pNTFS->Bitmap;
      UINT j,nClusters;
      iLastCluster -= length;
      while (i<iLastCluster) {
         for (j=i,nClusters=length; nClusters; nClusters--,j++) {
            if (Bitmap[j>>5] & (1<<(j & 0x1F))) { // if cluster in use
               i=j+1; break; // restart search 1 step beyond used cluster
            }
         }
         if (nClusters==0) return i;
      }
   }
   return 0;
}

/*.....................................................*/

static UINT
BinLength(HUGE n) // Returns the number of bytes needed to store the SI64 number 'n'
{
   UINT L=0,lsb=0;
   if (n==0) return 0;
   do {
      lsb = (BYTE)n; // keep the byte we're about to lose.
      n >>= 8; // I assume arithmetic right shift, i.e. (-x) shifts down to (-1), positive numbers shift to zero.
      L++;
   } while (n!=0 && n!=(-1)); // exit loop if only sign bits left.
   if (n) lsb ^= 0x80;         // if top bit doesn't match sign then n needs an extra byte of storage.
   return (L + (lsb>>7));
}

/*.....................................................*/

static void
SetRunList(PNTFSVOL pNTFS, PNTFS_FILE_RECORD pFile, UINT *pRunList, PMFT_ATTRIBUTE pDataAttr)
{
   UINT cLen,old_cLen,LCN,prevLCN,deltaLCN,runlen,Os,Ls;
   BYTE *pRun,EncodedRun[64];

   // encode the runs
   prevLCN = cLen = 0;
   while (*pRunList!=0xFFFFFFFF) {
      LCN = *pRunList++;
      runlen = *pRunList++;
      deltaLCN = LCN-prevLCN;
      prevLCN = LCN;
      Os = BinLength(deltaLCN);
      Ls = BinLength(runlen);
      EncodedRun[cLen] = (BYTE)(Ls + (Os<<4));
      Mem_Copy(EncodedRun+cLen+1, &runlen, Ls);
      Mem_Copy(EncodedRun+cLen+1+Ls, &deltaLCN, Os);
      cLen += (1+Ls+Os);
   }
   EncodedRun[cLen++] = (BYTE)0; // terminator
   while (cLen & 3) EncodedRun[cLen++] = (BYTE)0; // pad attribute length to a dword boundary.

   // get length in bytes of old run list
   old_cLen = 0;
   if (!pDataAttr) pDataAttr = MFTFindAttribute(pFile,MFT_ATTR_DATA);
   pRun = ((BYTE*)pDataAttr) + pDataAttr->u.nonres.DataRunOffset;
   if (pDataAttr) { // always true
      BYTE *pDataEnd = ((BYTE*)pDataAttr)+pDataAttr->len;
      BYTE olb;
      while (pRun < pDataEnd) {
         olb = *pRun++;
         old_cLen++;
         if (olb==0) break;
         Ls = (BYTE)(olb & 0xF);
         Os = (BYTE)(olb>>4);
         old_cLen += (Ls+Os);
         pRun += (Ls+Os);
      }
   }
   old_cLen = (old_cLen+3) & ~3;

   // make space for the new run list (if size has changed)
   pRun = ((BYTE*)pDataAttr) + pDataAttr->u.nonres.DataRunOffset;
   Ls = (UINT)((((BYTE*)pFile) + pNTFS->boots.BytesPerMFTRec) - pRun);
   if (old_cLen < cLen) { // new runlist needs more space
      UINT delta = cLen-old_cLen; // in fact it needs this many extra bytes
      Mem_Move(pRun+delta,pRun,Ls);
      pDataAttr->len += delta;
   } else if (cLen < old_cLen) { // new runlist needs less space
      UINT delta = old_cLen-cLen; // in fact it needs this many fewer bytes
      Mem_Move(pRun,pRun+delta,Ls-delta);
      pDataAttr->len -= delta;
   }

   // store new run list
   Mem_Move(pRun,EncodedRun,cLen);
}

/*.....................................................*/

static void
RestoreFixups(PNTFSVOL pNTFS, PNTFS_FILE_RECORD pFile)
{
   BYTE *pb = (BYTE*)pFile;
   UINT n = pNTFS->boots.BytesPerMFTRec>>9;
   WORD *pUpdateSeq = (WORD*)(pb + pFile->UpdateSeqOffset);
   WORD SeqNo = *pUpdateSeq++;
   pFile->UpdateSeqLength = (WORD)(n+1);
   pb += 510;
   for (; n; n--) {
      *pUpdateSeq++ = *((WORD*)pb);
      *((WORD*)pb) = SeqNo;
      pb += 512;
   }
}

/*.....................................................*/

static void
WriteFileRecord(PNTFSVOL pNTFS, PNTFS_FILE_RECORD pFile, HCOW cow)
{
   HUGE LBA;
   UINT offsetFromMftStart = ((UINT)(((BYTE*)pFile) - pNTFS->cluster))>>9; // offset in sectors.

   RestoreFixups(pNTFS,pFile);

   LBA = pNTFS->boots.BootSectorLBA + (pNTFS->boots.LCN_MFT<<pNTFS->boots.SectorsPerClusterShift) + offsetFromMftStart;
   cow->WriteSectors(cow,pFile,LBA,pNTFS->boots.BytesPerMFTRec>>9);

   if (offsetFromMftStart < pNTFS->boots.SectorsPerCluster) { // MFTMirr only backs up as many MFT records as fit in one cluster.
      LBA = pNTFS->boots.BootSectorLBA + (pNTFS->boots.LCN_MFTMirr<<pNTFS->boots.SectorsPerClusterShift) + offsetFromMftStart;
      cow->WriteSectors(cow,pFile,LBA,pNTFS->boots.BytesPerMFTRec>>9);
   }
}

/*.....................................................*/

PUBLIC UINT
NTFS_GrowPartition(HVDDR hVDI, UINT iLBA, UINT OldSectors, UINT NewSectors, UINT cHeads)
{
   HFSYS hNTFS = NTFS_OpenVolume(hVDI, iLBA, OldSectors, 512);
   if (hNTFS) {
      HCOW cow = (HCOW)hVDI;
      PNTFSVOL pNTFS = (PNTFSVOL)hNTFS;
      PNTFS_FILE_RECORD pFile;
      HUGE SectorsInVolume;
      
      // patch boot sector and write to standard and backup locations.
      raw_boot_sector[26] = (BYTE)cHeads;
      raw_boot_sector[27] = (BYTE)0;
      SectorsInVolume = ((NewSectors-1)>>pNTFS->boots.SectorsPerClusterShift)<<pNTFS->boots.SectorsPerClusterShift;
      Mem_Copy(raw_boot_sector+0x28,&SectorsInVolume,8);
      cow->WriteSectors(cow,raw_boot_sector,iLBA,1);
      cow->WriteSectors(cow,raw_boot_sector,iLBA+NewSectors-1,1);
      NewSectors--;
      
      pFile = MFTFindFile(pNTFS, L"$Bitmap", pNTFS->cluster, pNTFS->ClusterSize*16);
      if (pFile) {
         UINT nClusters = (OldSectors>>pNTFS->boots.SectorsPerClusterShift);
         UINT i,MFT_Zone_Clusters,bitmap_bytes,bitmap_clusters,bitmap_sectors,iNewBitmapLCN,RunList[32];
         MFT_FILENAME *pfn;
         PMFT_ATTRIBUTE pAttr;

         // NTFS tries to avoid allocating clusters from inside the "MFT zone" to anything except the MFT.
         // We will try to keep to the same rules.
         // The MFT Zone is a contiguous 12.5% or 1/8 of the volume, starting whereever the MFT starts.
         MFT_Zone_Clusters = ((nClusters+7)>>3);

         // the last qword of the old bitmap may have addressed out of range clusters which will
         // now be in-range. Make sure these are no longer marked as used.
         if (nClusters & 0x3F) MarkRunUnused(pNTFS,nClusters,64-(nClusters & 0x3F));

         // Free up the clusters previously occupied by the bitmap file.
         GetRunList(pNTFS,pFile,NULL,RunList);
         for (i=0; RunList[i]!=0xFFFFFFFF; i+=2) {
            MarkRunUnused(pNTFS,RunList[i],RunList[i+1]);
         }

         // size in clusters of expanded volume
         nClusters = (NewSectors>>pNTFS->boots.SectorsPerClusterShift);

         // logical size in bytes of new bitmap file (this must be a multiple of 8 bytes).
         bitmap_bytes = (nClusters>>6)*8;
         if (nClusters & 0x3F) bitmap_bytes+=8;

         // allocated size in clusters of new bitmap file
         bitmap_clusters = (bitmap_bytes+(pNTFS->ClusterSize-1))/pNTFS->ClusterSize;

         // find filename attribute of $Bitmap file and change the file sizes.
         pAttr = MFTFindAttribute(pFile,MFT_ATTR_FILENAME);
         pfn = (MFT_FILENAME*)(((PSTR)(&pAttr->u.res.bPadding))+1+pAttr->NameLength*2);
         pfn->FileSizeReal = bitmap_bytes;
         pfn->FileSizeAlloc = bitmap_clusters*pNTFS->ClusterSize;

         // ditto for the data attribute.
         pAttr = MFTFindAttribute(pFile,MFT_ATTR_DATA);
         pAttr->u.nonres.AttrSizeReal  = bitmap_bytes;
         pAttr->u.nonres.AttrSizeAlloc = bitmap_clusters*pNTFS->ClusterSize;
         pAttr->u.nonres.InitDataSize  = bitmap_bytes;
         pAttr->u.nonres.StartVCN      = 0;
         pAttr->u.nonres.LastVCN       = (bitmap_clusters-1);

         bitmap_sectors = ((bitmap_bytes+511) & ~511)>>9; // round up to sector size (granularity of disk write).

         // expand the existing memory copy of the bitmap file.
         pNTFS->Bitmap = Mem_ReAlloc(pNTFS->Bitmap,MEMF_ZEROINIT,(bitmap_sectors<<9)+4);

         // the last qword of the new bitmap may address out of range clusters. Make sure
         // these are marked as used.
         if (nClusters & 0x3F) MarkRunUsed(pNTFS,nClusters,64-(nClusters & 0x3F));

         // allocate new contiguous clusters for the expanded bitmap file, and mark those clusters as used.
         // Try to avoid MFT Zone for first attempt.
         iNewBitmapLCN = FindFreeSpace(pNTFS,((UINT)(pNTFS->boots.LCN_MFT))+MFT_Zone_Clusters,nClusters,bitmap_clusters);
         if (iNewBitmapLCN==0) iNewBitmapLCN = FindFreeSpace(pNTFS,1,nClusters,bitmap_clusters);
         if (iNewBitmapLCN) {
            MarkRunUsed(pNTFS,iNewBitmapLCN,bitmap_clusters);

            // Update the runlist for the $Bitmap file to reference the new cluster run.
            RunList[0] = iNewBitmapLCN;
            RunList[1] = bitmap_clusters;
            RunList[2] = 0xFFFFFFFF;
            SetRunList(pNTFS,pFile,RunList,NULL);
            WriteFileRecord(pNTFS,pFile,cow);

            // finally, write the bitmap file data itself.
//          DumpData("c:\\dj2\\Bitmap_enlarged_b4write.bin",pNTFS->Bitmap,bitmap_sectors<<9);
            cow->WriteSectors(cow,pNTFS->Bitmap,iLBA+(iNewBitmapLCN<<pNTFS->boots.SectorsPerClusterShift),bitmap_sectors);
            OldSectors = NewSectors;
         }

         // Correct the $BadClus$Bad stream size (it should equal the volume size).
         pFile = MFTFindFile(pNTFS, L"$BadClus", pNTFS->cluster, pNTFS->ClusterSize*16);
         if (pFile) {
            // find filename attribute of $BadClus file and change the file sizes.
            pAttr = MFTFindNamedAttribute(pFile,MFT_ATTR_DATA,L"$Bad");
            if (pAttr) {
               HUGE fsize = nClusters;

               pAttr->u.nonres.AttrSizeAlloc = pAttr->u.nonres.AttrSizeReal = fsize*pNTFS->ClusterSize;
               pAttr->u.nonres.InitDataSize  = 0;
               pAttr->u.nonres.StartVCN      = 0;
               pAttr->u.nonres.LastVCN       = (nClusters-1);

               RunList[0] = 0;
               RunList[1] = nClusters;
               RunList[2] = 0xFFFFFFFF;
               SetRunList(pNTFS,pFile,RunList,pAttr);

               WriteFileRecord(pNTFS,pFile,cow);
            }
         }
      }
      NTFS_CloseVolume(hNTFS);
   }
   return OldSectors;
}

/*.....................................................*/

/* end of ntfs.c */


/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef EXT2_STRUCT_H
#define EXT2_STRUCT_H

#include "djtypes.h"

#define EXT2_SUPER_MAGIC               0xEF53
#define EXT4_FEATURE_INCOMPAT_EXTENTS  0x0040

#pragma pack(push,1)

// superblock structure
typedef struct {
   UINT s_inodes_count;
   UINT s_blocks_count;      // ** This is total number of blocks (clusters) in system. Note 32bit limit.
   UINT s_r_blocks_count;    // reserved (for super user) blocks count.
   UINT s_free_blocks_count;
   UINT s_free_inodes_count;
   UINT s_first_data_block;  // id of block containing the superblock. Not much use, since you already have the superblock to see this. 
   UINT s_log_block_size;    // ** block size == (1024<<s_log_block_size) - so a block cannot be less than 1024 bytes.
   int  s_log_frag_size;     // fragment(?) size == (1024<<s_log_block_size) note that shift can be neg, so <1024 is allowed.
   UINT s_blocks_per_group;  // ** blocks per block group. What about if s_blocks_count isn't an integral multiple of this?
   UINT s_frags_per_group;   // I assume fragments have something to do with efficient handling of small files. Fragments don't seem to be supported in Linux anyway.
   UINT s_inodes_per_group;
   UINT s_mtime;
   UINT s_wtime;
   WORD s_mnt_count;
   WORD s_max_mnt_count;
   WORD s_magic;             // Should match EXT2_SUPER_MAGIC
   WORD s_state;
   WORD s_errors;
   WORD s_minor_rev_level;
   UINT s_lastcheck;
   UINT s_checkinterval;
   UINT s_creator_os;
   UINT s_rev_level;
   WORD s_def_resuid;
   WORD s_def_resgid;

   // EXT2_DYNAMIC_REV Specific --
   UINT s_first_ino;
   WORD s_inode_size;
   WORD s_block_group_nr;
   UINT s_feature_compat;
   UINT s_feature_incompat;
   UINT s_feature_ro_compat;
   S_UUID s_uuid;
   BYTE s_volume_name[16];
   BYTE s_last_mounted[64];
   UINT s_algo_bitmap;

   // Performance Hints --
   BYTE s_prealloc_blocks;
   BYTE s_prealloc_dir_blocks;
   WORD pad; // (quad alignment)

   // -- Journaling Support --
   S_UUID s_journal_uuid;
   UINT s_journal_inum;
   UINT s_journal_dev;
   UINT s_last_orphan;

   // -- Directory Indexing Support --
   UINT s_hash_seed[4];
   BYTE s_def_hash_version[4]; // hash version in low byte only.

   // -- Other options --
   UINT s_default_mount_options;
   UINT s_first_meta_bg;
   BYTE reserved[760]; // Reserved for future filesystem revisions
} SUPERBLK, *PSUPERBLK;

// block group descriptor structure
typedef struct {              // Block Group Descriptor
   UINT bg_block_bitmap;      // ** absolute block-id of first block of block bitmap for this block group.
   UINT bg_inode_bitmap;
   UINT bg_inode_table;
   WORD bg_free_blocks_count; // ? implies 16 bit limit on blocks per block group??
   WORD bg_free_inodes_count; // ? ditto for inodes.
   WORD bg_used_dirs_count;
   WORD bg_pad;               // pad to quad boundary.
   WORD bg_reserved[6];       // pad to 32 bytes.
} BGD, *PBGD;

// unix style datestamp == seconds since 1st Jan, 1970.
typedef UINT XTIME;

// inode table structure (Linux variant)
typedef struct {  // 128 bytes
   WORD i_mode;   // file type and access mode flags.
   WORD i_uid;    // low word of user id.
   UINT i_size;   // low 32 bits of file in bytes.
   XTIME i_atime; // last access time, seconds since 
   XTIME i_ctime; // creation "
   XTIME i_mtime; // modify "
   XTIME i_dtime; // inode deletion "
   WORD i_gid;    // id of which posix group has access (low word).
   WORD i_links_count; // number of references to this inode.
   UINT i_blocks; // number of blocks reserved to contain inode data.
   UINT i_flags;  // processing flags.
   UINT i_osd1;   // reserved.
   UINT i_block[15]; // block numbers of blocks containing file data, [13]=double indirect link, [14]=triple indirect link.
   UINT i_generation; // file version
   UINT i_file_acl; // block number of extended attribites block.
   UINT i_size_hi; // for regular files, the high 32 bits of the file size.
   UINT i_faddr;   // location of file fragment (not used in Linux)
   UINT reserved;
   WORD i_uid_hi;  // high word of user id.
   WORD i_hid_hi;  // high word of posix group id.
   UINT reserved2;
} INODE_TABLE;

#pragma pack(pop)

#endif


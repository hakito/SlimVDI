/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef IDS_H
#define IDS_H

/*======================================================================*/
/* string identifiers                                                   */
/*======================================================================*/

/* widely useful strings */
#define IDS_NULL                  0
#define IDS_OK                    1   /* = "Ok" */
#define IDS_NONE                  2   /* = "None" */
#define IDS_ERROR                 3   /* = "Error" */
#define IDS_UNKERROR              4   /* = "Unknown Error" */
#define IDS_NOYES                 5   /* = "NY" */

/* dialog box template names */
#define IDS_DLG_CLONEVDI          20                   /* = "DLG_CLONEVDI" */
#define IDS_DLG_ALT_PROGRESS      (IDS_DLG_CLONEVDI+1) /* = "DLG_ALT_PROGRESS" */
#define IDS_DLG_PARTINFO          (IDS_DLG_CLONEVDI+2) /* = "DLG_PARTINFO" */
#define IDS_DLG_VDI_HDR_INFO      (IDS_DLG_CLONEVDI+3) /* = "DLG_VDI_HDR_INFO" */
#define IDS_DLG_VHD_HDR_INFO      (IDS_DLG_CLONEVDI+4) /* = "DLG_VHD_HDR_INFO" */
#define IDS_DLG_HDD_HDR_INFO      (IDS_DLG_CLONEVDI+5) /* = "DLG_HDD_HDR_INFO" */
#define IDS_DLG_VMDK_HDR_INFO     (IDS_DLG_CLONEVDI+6) /* = "DLG_VMDK_HDR_INFO" */
#define IDS_DLG_WRITE_SECTORS     (IDS_DLG_CLONEVDI+7) /* = "DLG_WRITE_SECTORS" */
#define IDS_DLG_ALT_SECTOR_VIEWER (IDS_DLG_CLONEVDI+8) /* = "DLG_ALT_SECTOR_VIEWER" */

/* strings from CloneVDI.c */
#define IDS_CLONEVDI 100
#define IDS_SELECT_SOURCE  (IDS_CLONEVDI+0) /* = "Select source file..." */
#define IDS_SRCFILEFILTER  (IDS_CLONEVDI+1) /* = "Virtual drive files\0*.vdi;*.vhd;*.vmdk;*.raw;*.img\0\0" */
#define IDS_SELECT_DEST    (IDS_CLONEVDI+2) /* = "Select destination filename..." */
#define IDS_DSTFILEFILTER  (IDS_CLONEVDI+3) /* = "Virtual drive files\0*.vdi\0\0" */
#define IDS_UNKNOWNFS      (IDS_CLONEVDI+4) /* = "Unknown(%02lx)" */
#define IDS_PLSSELSRC      (IDS_CLONEVDI+5) /* = "Please select a source virtual disk" */
#define IDS_NOSRC          (IDS_CLONEVDI+6) /* = "Cannot proceed - source file does not exist!" */
#define IDS_ERRAPPWND      (IDS_CLONEVDI+7) /* = "Could not create app window (error x%x)" */

/* strings from Clone.c */
#define IDS_CLONE (IDS_CLONEVDI+50)
#define IDS_LOMEM           (IDS_CLONE+0)  /* = "Low memory! Could not allocate copy buffer!" */
#define IDS_FINDINGUSEDBLKS (IDS_CLONE+1)  /* = "Finding used blocks in partitions - please wait..." */
#define IDS_CLONINGVHD      (IDS_CLONE+2)  /* = "Cloning Virtual Hard Disk..." */
#define IDS_CLONEWAIT       (IDS_CLONE+3)  /* = "Cloning virtual disk - please wait..." */
#define IDS_USERABORT       (IDS_CLONE+4)  /* = "Aborted by user" */
#define IDS_ORIGINAL        (IDS_CLONE+5)  /* = "Original " */
#define IDS_SNAPSHOT        (IDS_CLONE+6)  /* = "Source is a difference image. Sorry, this tool cannot clone these (for now)" */
#define IDS_SIZEERR2        (IDS_CLONE+7)  /* = "The new drive size must be a number followed by 'GB' or 'MB' to indicate the size range" */
#define IDS_SIZEERR3        (IDS_CLONE+8)  /* = "Bad number format, or number too large, in new drive size field" */
#define IDS_SIZEERR4        (IDS_CLONE+9)  /* = "The new drive size must be at least as large as the old drive size" */
#define IDS_SIZEERR5        (IDS_CLONE+10) /* = "This utility cannot create virtual drives larger than 2047.00 GB" */
#define IDS_BADNUM          (IDS_CLONE+11) /* = "Bad number in new drive size field" */
#define IDS_ERRRENAME       (IDS_CLONE+12) /* = "Could not rename old VDI - a backup file of the intended name already exists! Therefore the clone will keep its temp filename of ""%s""" */

/* strings from cmdline.c */
#define IDS_CMDLINE (IDS_CLONE+50)
#define IDS_USAGE00         (IDS_CMDLINE+0)
#define IDS_USAGE01         (IDS_CMDLINE+1)
#define IDS_USAGE02         (IDS_CMDLINE+2)
#define IDS_USAGE03         (IDS_CMDLINE+3)
#define IDS_USAGE04         (IDS_CMDLINE+4)
#define IDS_USAGE05         (IDS_CMDLINE+5)
#define IDS_USAGE06         (IDS_CMDLINE+6)
#define IDS_USAGE07         (IDS_CMDLINE+7)
#define IDS_USAGE08         (IDS_CMDLINE+8)
#define IDS_USAGE09         (IDS_CMDLINE+9)
#define IDS_USAGE10         (IDS_CMDLINE+10)
#define IDS_USAGE11         (IDS_CMDLINE+11)
#define IDS_USAGE12         (IDS_CMDLINE+12)
#define IDS_USAGE13         (IDS_CMDLINE+13)
#define IDS_USAGE14         (IDS_CMDLINE+14)
#define IDS_USAGE15         (IDS_CMDLINE+15)
#define IDS_USAGE16         (IDS_CMDLINE+16)
#define IDS_USAGE17         (IDS_CMDLINE+17)
#define IDS_USAGE18         (IDS_CMDLINE+18)
#define IDS_USAGE19         (IDS_CMDLINE+19)
#define IDS_USAGE_LAST      IDS_USAGE19
#define IDS_BANNER0         (IDS_CMDLINE+20)  /* = "\015\012CloneVDI %lu.%02lx\015\012" */
#define IDS_BANNER1         (IDS_CMDLINE+21)  /* = "Copyright (C) %lu, Don Milne\015\012" */
#define IDS_BANNER2         (IDS_CMDLINE+22)  /* = "Feedback to: mpack on forums.virtualbox.org\r\n" */
#define IDS_ARGERR          (IDS_CMDLINE+23)  /* = "Error in argument %lu: %s.\015\012" */
#define IDS_OPTTWICE        (IDS_CMDLINE+24)  /* = "Error in argument %lu: %s option specified twice.\015\012" */
#define IDS_NOFILEN         (IDS_CMDLINE+25)  /* = "output option specified, no filename provided" */
#define IDS_NODSIZE         (IDS_CMDLINE+26)  /* = "enlarge option specified, no disk size provided" */
#define IDS_UNKOPT          (IDS_CMDLINE+27)  /* = "unknown option." */
#define IDS_INVOPT          (IDS_CMDLINE+28)  /* = "Invalid option format (embedded space?)" */
#define IDS_SRCTWICE        (IDS_CMDLINE+29)  /* = "Source name given twice? Dest file should be specified using --output <fn> option" */
#define IDS_NEEDSRC         (IDS_CMDLINE+30)  /* = "Source filename is missing" */
#define IDS_VOPTOUTPUT      (IDS_CMDLINE+31)  /* = "output" */
#define IDS_VOPTKEEPUUID    (IDS_CMDLINE+32)  /* = "keepuuid" */
#define IDS_VOPTENLARGE     (IDS_CMDLINE+33)  /* = "enlarge" */
#define IDS_VOPTCOMPACT     (IDS_CMDLINE+34)  /* = "compact" */
#define IDS_VOPTHELP        (IDS_CMDLINE+35)  /* = "help" */
#define IDS_CHAROPT         (IDS_CMDLINE+36)  /* = "okech"  */

/* strings from env.c */
#define IDS_ENV (IDS_CMDLINE+50)
#define IDS_CLONEOF         (IDS_ENV+0)       /* = "Clone of " */

/* SectorViewer.c */
#define IDS_SVIEWER (IDS_ENV+20)
#define IDS_SVOFFSET        (IDS_SVIEWER+0)   /* = "Offset = %lu" */
#define IDS_SVSECTOR        (IDS_SVIEWER+1)   /* = "Sector %lu of %lu" */
#define IDS_SVDESTFN        (IDS_SVIEWER+2)   /* = "Select destination filename..." */
#define IDS_SVBINFLT        (IDS_SVIEWER+3)   /* = "Binary files\000*.bin\000\000" */
#define IDS_SVPEVALSS       (IDS_SVIEWER+4)   /* = "Please enter a valid start sector" */
#define IDS_SVPEVALSW       (IDS_SVIEWER+5)   /* = "Please enter a valid number of sectors to write" */
#define IDS_SVNOSPACE       (IDS_SVIEWER+6)   /* = "Not enough free space in target folder" */
#define IDS_SVINVPATH       (IDS_SVIEWER+7)   /* = "Invalid path" */
#define IDS_SVACCDEN        (IDS_SVIEWER+8)   /* = "Write access was denied" */
#define IDS_SVWRPROT        (IDS_SVIEWER+10)  /* = "Target drive is write protected" */
#define IDS_SVOVERWR        (IDS_SVIEWER+11)  /* = "Destination already exists. Are you sure you want to overwrite it?" */
#define IDS_SVEXISTS        (IDS_SVIEWER+12)  /* = "File Exists" */
#define IDS_SVLOWMEM        (IDS_SVIEWER+13)  /* = "Low memory! Could not allocate copy buffer!" */
#define IDS_SVWRSECT        (IDS_SVIEWER+14)  /* = "Writing sectors - please wait..." */
#define IDS_SVPRCAPT        (IDS_SVIEWER+15)  /* = "Writing sectors..." */
#define IDS_SVWRERR         (IDS_SVIEWER+16)  /* = "Write error!" */
#define IDS_SVABORT         (IDS_SVIEWER+17)  /* = "Aborted by user" */

/* ShowHeader.c */
#define IDS_SHOWHDR (IDS_SVIEWER+40)
#define IDS_DYNAMIC         (IDS_SHOWHDR+0)   /* = "Dynamic" */
#define IDS_FIXED           (IDS_SHOWHDR+1)   /* = "Fixed" */
#define IDS_UNDO            (IDS_SHOWHDR+2)   /* = "Undo" */
#define IDS_DIFFERENCING    (IDS_SHOWHDR+3)   /* = "Differencing" */
#define IDS_SET             (IDS_SHOWHDR+4)   /* = "Set" */
#define IDS_NOTSET          (IDS_SHOWHDR+5)   /* = "Not set" */
#define IDS_NOTSPEC         (IDS_SHOWHDR+6)   /* = "Not specified" */
#define IDS_ACCTYPE0        (IDS_SHOWHDR+7)   /* = "?" */
#define IDS_ACCTYPE1        (IDS_SHOWHDR+8)   /* = "RW" */
#define IDS_ACCTYPE2        (IDS_SHOWHDR+9)   /* = "RdOnly" */
#define IDS_ACCTYPE3        (IDS_SHOWHDR+10)  /* = "None" */
#define IDS_EXTTYPE0        (IDS_SHOWHDR+11)  /* = "?" */
#define IDS_EXTTYPE1        (IDS_SHOWHDR+12)  /* = "Flat" */
#define IDS_EXTTYPE2        (IDS_SHOWHDR+13)  /* = "Sparse" */
#define IDS_EXTTYPE3        (IDS_SHOWHDR+14)  /* = "Zero" */

/* generic error messages for VxxxR modules */
#define IDS_VDXR            (IDS_SHOWHDR+40)
#define IDS_VDNOEXIST       (IDS_VDXR+0)      /* = "The source file does not exist" */
#define IDS_VDERRSHARE      (IDS_VDXR+1)      /* = "Source file already in use (is VirtualBox running?)" */
#define IDS_VDERRREAD       (IDS_VDXR+2)      /* = "Got OS error %lu when reading from source file" */
#define IDS_VDNOTVDI        (IDS_VDXR+3)      /* = "Source file is not a recognized VDI file format" */
#define IDS_VDINVHANDLE     (IDS_VDXR+4)      /* = "Invalid handle passed to VDx source object" */
#define IDS_VDNOMEM         (IDS_VDXR+5)      /* = "Not enough memory to map source file" */
#define IDS_VDSEEKRANGE     (IDS_VDXR+6)      /* = "App attempted to seek beyond end of drive!" */
#define IDS_VDERRFMT        (IDS_VDXR+7)      /* = "Source has strange format which is incompatible with this tool" */
#define IDS_VDERRSEEK       (IDS_VDXR+8)      /* = "Got OS error %lu when seeking inside source file" */
#define IDS_VDBLKMAP        (IDS_VDXR+9)      /* = "Source file corrupt - block map references blocks beyond end of file" */
#define IDS_VDNOTVHD        (IDS_VDXR+10)     /* = "Source file is not a recognized VHD file format" */
#define IDS_VDNOTVMDK       (IDS_VDXR+11)     /* = "Source file is not a recognized VMDK file format" */
#define IDS_VDNODESC        (IDS_VDXR+12)     /* = "VMDK incomplete - disk descriptor not found" */
#define IDS_VDINVDESC       (IDS_VDXR+13)     /* = "Invalid VMDK disk descriptor" */
#define IDS_VDBADVER        (IDS_VDXR+14)     /* = "VMDK version number was not recognized" */
#define IDS_VDMISSING       (IDS_VDXR+15)     /* = "Required parameters are missing from the VMDK descriptor" */
#define IDS_VDERRCOMP       (IDS_VDXR+16)     /* = "Compressed VMDKs are not currently supported" */
#define IDS_VDNOEXTENT      (IDS_VDXR+17)     /* = "A descriptor referenced extent file is missing (renamed?)" */
#define IDS_VDNOTHDD        (IDS_VDXR+18)     /* = "Source file is not a recognized HDD file format" */

/* vdiw.c */
#define IDS_VDIW            (IDS_VDXR+40)
#define IDS_VWERRPATH       (IDS_VDIW+0)      /* = "Destination path does not exist" */
#define IDS_VWEXISTS        (IDS_VDIW+1)      /* = "Destination file already exists" */
#define IDS_VWNOSPACE       (IDS_VDIW+2)      /* = "Not enough free space on destination drive" */
#define IDS_VWWRPROT        (IDS_VDIW+3)      /* = "Dest drive is write protected" */
#define IDS_VWACCDEN        (IDS_VDIW+4)      /* = "Access to dest drive was denied" */
#define IDS_VWNOMEM         (IDS_VDIW+5)      /* = "Not enough memory" */
#define IDS_VWWRERR         (IDS_VDIW+6)      /* = "Drive write error" */
#define IDS_VWBLOCK         (IDS_VDIW+7)      /* = "Attempted block write past end of virtual disk" */
#define IDS_VWEXISTSQ       (IDS_VDIW+8)      /* = "Destination already exists. Are you sure you want to overwrite it?" */
#define IDS_VWEXISTSC       (IDS_VDIW+9)      /* = "File Exists" */

#endif


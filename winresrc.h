/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef RCDEFS_H
#define RCDEFS_H

/* STANDARD WINDOWS SYMBOLS FOR A GENERIC RESOURCE COMPILER - ADD TO THESE AS REQUIRED */
/* I created this include file after suffering repeated problems with resource
 * compilers erroring out with the standard "windows.h" include. The idea is that
 * this include will have no more than is needed to get a typical resource script to
 * compile. Note that this header is included by the resource file, not by the
 * project directly.
 */

/*
 * Dialog Box Command IDs
 */
#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7
#define IDCLOSE             8
#define IDHELP              9


/*
 * Virtual Key Codes
 */
#define VK_LBUTTON        0x01
#define VK_RBUTTON        0x02
#define VK_CANCEL         0x03
#define VK_MBUTTON        0x04    /* NOT contiguous with L & RBUTTON */

#define VK_BACK           0x08
#define VK_TAB            0x09

#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D

#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14

#define VK_ESCAPE         0x1B

#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29
#define VK_PRINT          0x2A
#define VK_EXECUTE        0x2B
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F

/* VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
/* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */

#define VK_LWIN           0x5B
#define VK_RWIN           0x5C
#define VK_APPS           0x5D

#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87
#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91


/*
 * Window Styles
 */
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_VSCROLL          0x00200000L
#define WS_HSCROLL          0x00100000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_GROUP            0x00020000L
#define WS_TABSTOP          0x00010000L

#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L

#define WS_TILED            WS_OVERLAPPED
#define WS_ICONIC           WS_MINIMIZE
#define WS_SIZEBOX          WS_THICKFRAME

/*
 * Common Window Styles
 */
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX)
#define WS_TILEDWINDOW      WS_OVERLAPPEDWINDOW
#define WS_POPUPWINDOW      (WS_POPUP|WS_BORDER|WS_SYSMENU)
#define WS_CHILDWINDOW      (WS_CHILD)

/*
 * Extended Window Styles
 */
#define WS_EX_DLGMODALFRAME     0x00000001L
#define WS_EX_NOPARENTNOTIFY    0x00000004L
#define WS_EX_TOPMOST           0x00000008L
#define WS_EX_ACCEPTFILES       0x00000010L
#define WS_EX_TRANSPARENT       0x00000020L
#define WS_EX_MDICHILD          0x00000040L
#define WS_EX_TOOLWINDOW        0x00000080L
#define WS_EX_WINDOWEDGE        0x00000100L
#define WS_EX_CLIENTEDGE        0x00000200L
#define WS_EX_CONTEXTHELP       0x00000400L

#define WS_EX_RIGHT             0x00001000L
#define WS_EX_LEFT              0x00000000L
#define WS_EX_RTLREADING        0x00002000L
#define WS_EX_LTRREADING        0x00000000L
#define WS_EX_LEFTSCROLLBAR     0x00004000L
#define WS_EX_RIGHTSCROLLBAR    0x00000000L

#define WS_EX_CONTROLPARENT     0x00010000L
#define WS_EX_STATICEDGE        0x00020000L
#define WS_EX_APPWINDOW         0x00040000L

#define WS_EX_OVERLAPPEDWINDOW  (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW     (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)




/*
 * Dialog Styles
 */
#define DS_ABSALIGN         0x01L
#define DS_SYSMODAL         0x02L
#define DS_LOCALEDIT        0x20L   /* Edit items get Local storage. */
#define DS_SETFONT          0x40L   /* User specified font for Dlg controls */
#define DS_MODALFRAME       0x80L   /* Can be combined with WS_CAPTION  */
#define DS_NOIDLEMSG        0x100L  /* WM_ENTERIDLE message will not be sent */
#define DS_SETFOREGROUND    0x200L  /* not in win3.1 */

#define DS_3DLOOK           0x0004L
#define DS_FIXEDSYS         0x0008L
#define DS_NOFAILCREATE     0x0010L
#define DS_CONTROL          0x0400L
#define DS_CENTER           0x0800L
#define DS_CENTERMOUSE      0x1000L
#define DS_CONTEXTHELP      0x2000L


/*
 * Button Control Styles
 */
#define BS_PUSHBUTTON       0x00000000L
#define BS_DEFPUSHBUTTON    0x00000001L
#define BS_CHECKBOX         0x00000002L
#define BS_AUTOCHECKBOX     0x00000003L
#define BS_RADIOBUTTON      0x00000004L
#define BS_3STATE           0x00000005L
#define BS_AUTO3STATE       0x00000006L
#define BS_GROUPBOX         0x00000007L
#define BS_USERBUTTON       0x00000008L
#define BS_AUTORADIOBUTTON  0x00000009L
#define BS_OWNERDRAW        0x0000000BL
#define BS_LEFTTEXT         0x00000020L
#define BS_TEXT             0x00000000L
#define BS_ICON             0x00000040L
#define BS_BITMAP           0x00000080L
#define BS_LEFT             0x00000100L
#define BS_RIGHT            0x00000200L
#define BS_CENTER           0x00000300L
#define BS_TOP              0x00000400L
#define BS_BOTTOM           0x00000800L
#define BS_VCENTER          0x00000C00L
#define BS_PUSHLIKE         0x00001000L
#define BS_MULTILINE        0x00002000L
#define BS_NOTIFY           0x00004000L
#define BS_FLAT             0x00008000L
#define BS_RIGHTBUTTON      BS_LEFTTEXT


/*
 * Listbox Styles
 */
#define LBS_NOTIFY            0x0001L
#define LBS_SORT              0x0002L
#define LBS_NOREDRAW          0x0004L
#define LBS_MULTIPLESEL       0x0008L
#define LBS_OWNERDRAWFIXED    0x0010L
#define LBS_OWNERDRAWVARIABLE 0x0020L
#define LBS_HASSTRINGS        0x0040L
#define LBS_USETABSTOPS       0x0080L
#define LBS_NOINTEGRALHEIGHT  0x0100L
#define LBS_MULTICOLUMN       0x0200L
#define LBS_WANTKEYBOARDINPUT 0x0400L
#define LBS_EXTENDEDSEL       0x0800L
#define LBS_DISABLENOSCROLL   0x1000L
#define LBS_NODATA            0x2000L
#define LBS_NOSEL             0x4000L
#define LBS_STANDARD          (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)


/*
 * Combo Box styles
 */
#define CBS_SIMPLE            0x0001L
#define CBS_DROPDOWN          0x0002L
#define CBS_DROPDOWNLIST      0x0003L
#define CBS_OWNERDRAWFIXED    0x0010L
#define CBS_OWNERDRAWVARIABLE 0x0020L
#define CBS_AUTOHSCROLL       0x0040L
#define CBS_OEMCONVERT        0x0080L
#define CBS_SORT              0x0100L
#define CBS_HASSTRINGS        0x0200L
#define CBS_NOINTEGRALHEIGHT  0x0400L
#define CBS_DISABLENOSCROLL   0x0800L
#define CBS_UPPERCASE           0x2000L
#define CBS_LOWERCASE           0x4000L


/*
 * Scroll Bar Styles
 */
#define SBS_HORZ                    0x0000L
#define SBS_VERT                    0x0001L
#define SBS_TOPALIGN                0x0002L
#define SBS_LEFTALIGN               0x0002L
#define SBS_BOTTOMALIGN             0x0004L
#define SBS_RIGHTALIGN              0x0004L
#define SBS_SIZEBOXTOPLEFTALIGN     0x0002L
#define SBS_SIZEBOXBOTTOMRIGHTALIGN 0x0004L
#define SBS_SIZEBOX                 0x0008L
#define SBS_SIZEGRIP                0x0010L


/*
 * Static Control Constants
 */
#define SS_LEFT             0x00000000L
#define SS_CENTER           0x00000001L
#define SS_RIGHT            0x00000002L
#define SS_ICON             0x00000003L
#define SS_BLACKRECT        0x00000004L
#define SS_GRAYRECT         0x00000005L
#define SS_WHITERECT        0x00000006L
#define SS_BLACKFRAME       0x00000007L
#define SS_GRAYFRAME        0x00000008L
#define SS_WHITEFRAME       0x00000009L
#define SS_USERITEM         0x0000000AL
#define SS_SIMPLE           0x0000000BL
#define SS_LEFTNOWORDWRAP   0x0000000CL
#define SS_BITMAP           0x0000000EL
#define SS_OWNERDRAW        0x0000000DL
#define SS_ENHMETAFILE      0x0000000FL
#define SS_ETCHEDHORZ       0x00000010L
#define SS_ETCHEDVERT       0x00000011L
#define SS_ETCHEDFRAME      0x00000012L
#define SS_TYPEMASK         0x0000001FL
#define SS_NOPREFIX         0x00000080L /* Don't do "&" character translation */
#define SS_NOTIFY           0x00000100L
#define SS_CENTERIMAGE      0x00000200L
#define SS_RIGHTJUST        0x00000400L
#define SS_REALSIZEIMAGE    0x00000800L
#define SS_SUNKEN           0x00001000L


/*
 * Edit Control Styles
 */
#define ES_LEFT             0x0000L
#define ES_CENTER           0x0001L
#define ES_RIGHT            0x0002L
#define ES_MULTILINE        0x0004L
#define ES_UPPERCASE        0x0008L
#define ES_LOWERCASE        0x0010L
#define ES_PASSWORD         0x0020L
#define ES_AUTOVSCROLL      0x0040L
#define ES_AUTOHSCROLL      0x0080L
#define ES_NOHIDESEL        0x0100L
#define ES_OEMCONVERT       0x0400L
#define ES_READONLY         0x0800L
#define ES_WANTRETURN       0x1000L
#define ES_NUMBER           0x2000L

/* ====================================================================================== */
/* ====================================================================================== */
/* LANGUAGE constants */
#define LANG_NEUTRAL                     0x00
#define LANG_ARABIC                      0x01
#define LANG_AFRIKAANS                   0x36
#define LANG_ALBANIAN                    0x1c
#define LANG_BASQUE                      0x2d
#define LANG_BULGARIAN                   0x02
#define LANG_BYELORUSSIAN                0x23
#define LANG_CATALAN                     0x03
#define LANG_CHINESE                     0x04
#define LANG_CROATIAN                    0x1a
#define LANG_CZECH                       0x05
#define LANG_DANISH                      0x06
#define LANG_DUTCH                       0x13
#define LANG_ENGLISH                     0x09
#define LANG_ESTONIAN                    0x25
#define LANG_FAEROESE                    0x38
#define LANG_FARSI                       0x29
#define LANG_FINNISH                     0x0b
#define LANG_FRENCH                      0x0c
#define LANG_GERMAN                      0x07
#define LANG_GREEK                       0x08
#define LANG_HEBREW                      0x0D
#define LANG_HUNGARIAN                   0x0e
#define LANG_ICELANDIC                   0x0f
#define LANG_INDONESIAN                  0x21
#define LANG_ITALIAN                     0x10
#define LANG_JAPANESE                    0x11
#define LANG_KOREAN                      0x12
#define LANG_LATVIAN                     0x26
#define LANG_LITHUANIAN                  0x27
#define LANG_NORWEGIAN                   0x14
#define LANG_POLISH                      0x15
#define LANG_PORTUGUESE                  0x16
#define LANG_ROMANIAN                    0x18
#define LANG_RUSSIAN                     0x19
#define LANG_SLOVAK                      0x1b
#define LANG_SLOVENIAN                   0x24
#define LANG_SORBIAN                     0x2e
#define LANG_SPANISH                     0x0a
#define LANG_SWEDISH                     0x1d
#define LANG_THAI                        0x1e
#define LANG_TURKISH                     0x1f
#define LANG_UKRAINIAN                   0x22

/*
 *  Sublanguage IDs.
 *
 *  The name immediately following SUBLANG_ dictates which primary
 *  language ID that sublanguage ID can be combined with to form a
 *  valid language ID.
 */
#define SUBLANG_NEUTRAL                  0x00    /* language neutral */
#define SUBLANG_DEFAULT                  0x01    /* user default */
#define SUBLANG_SYS_DEFAULT              0x02    /* system default */

#define SUBLANG_ARABIC                   0x01    /* Arabic (Saudi Arabia) */
#define SUBLANG_ARABIC_IRAQ              0x02    /* Arabic (Iraq) */
#define SUBLANG_ARABIC_EGYPT             0x03    /* Arabic (Egypt) */
#define SUBLANG_ARABIC_LIBYA             0x04    /* Arabic (Libya) */
#define SUBLANG_ARABIC_ALGERIA           0x05    /* Arabic (Algeria) */
#define SUBLANG_ARABIC_MOROCCO           0x06    /* Arabic (Morocco) */
#define SUBLANG_ARABIC_TUNISIA           0x07    /* Arabic (Tunisia) */
#define SUBLANG_ARABIC_OMAN              0x08    /* Arabic (Oman) */
#define SUBLANG_ARABIC_YEMEN             0x09    /* Arabic (Yemen) */
#define SUBLANG_ARABIC_SYRIA             0x10    /* Arabic (Syria) */
#define SUBLANG_ARABIC_JORDAN            0x11    /* Arabic (Jordan) */
#define SUBLANG_ARABIC_LEBANON           0x12    /* Arabic (Lebanon) */
#define SUBLANG_ARABIC_KUWAIT            0x13    /* Arabic (Kuwait) */
#define SUBLANG_ARABIC_UAE               0x14    /* Arabic (United Arab Emirates) */
#define SUBLANG_ARABIC_BAHRAIN           0x15    /* Arabic (Bahrain) */
#define SUBLANG_ARABIC_QATAR             0x16    /* Arabic (Qatar) */
#define SUBLANG_CHINESE_TRADITIONAL      0x01    /* Chinese (Taiwan) */
#define SUBLANG_CHINESE_SIMPLIFIED       0x02    /* Chinese (PR China) */
#define SUBLANG_CHINESE_HONGKONG         0x03    /* Chinese (Hong Kong) */
#define SUBLANG_CHINESE_SINGAPORE        0x04    /* Chinese (Singapore) */
#define SUBLANG_DUTCH                    0x01    /* Dutch */
#define SUBLANG_DUTCH_BELGIAN            0x02    /* Dutch (Belgian) */
#define SUBLANG_ENGLISH_US               0x01    /* English (USA) */
#define SUBLANG_ENGLISH_UK               0x02    /* English (UK) */
#define SUBLANG_ENGLISH_AUS              0x03    /* English (Australian) */
#define SUBLANG_ENGLISH_CAN              0x04    /* English (Canadian) */
#define SUBLANG_ENGLISH_NZ               0x05    /* English (New Zealand) */
#define SUBLANG_ENGLISH_EIRE             0x06    /* English (Irish) */
#define SUBLANG_ENGLISH_SAFRICA          0x07    /* English (South Africa) */
#define SUBLANG_ENGLISH_JAMAICA          0x08    /* English (Jamaica) */
#define SUBLANG_ENGLISH_CARRIBEAN        0x09    /* English (Carribean) */
#define SUBLANG_FRENCH                   0x01    /* French */
#define SUBLANG_FRENCH_BELGIAN           0x02    /* French (Belgian) */
#define SUBLANG_FRENCH_CANADIAN          0x03    /* French (Canadian) */
#define SUBLANG_FRENCH_SWISS             0x04    /* French (Swiss) */
#define SUBLANG_FRENCH_LUXEMBOURG        0x05    /* French (Luxembourg) */
#define SUBLANG_GERMAN                   0x01    /* German */
#define SUBLANG_GERMAN_SWISS             0x02    /* German (Swiss) */
#define SUBLANG_GERMAN_AUSTRIAN          0x03    /* German (Austrian) */
#define SUBLANG_GERMAN_LUXEMBOURG        0x04    /* German (Luxembourg) */
#define SUBLANG_GERMAN_LIECHTENSTEIN     0x05    /* German (Liechtenstein) */
#define SUBLANG_ITALIAN                  0x01    /* Italian */
#define SUBLANG_ITALIAN_SWISS            0x02    /* Italian (Swiss) */
#define SUBLANG_KOREAN                   0x01    /* Korean */
#define SUBLANG_KOREAN_JOHAB             0x02    /* Korean (Johab) */
#define SUBLANG_NORWEGIAN_BOKMAL         0x01    /* Norwegian (Bokmal) */
#define SUBLANG_NORWEGIAN_NYNORSK        0x02    /* Norwegian (Nynorsk) */
#define SUBLANG_PORTUGUESE               0x02    /* Portuguese */
#define SUBLANG_PORTUGUESE_BRAZILIAN     0x01    /* Portuguese (Brazilian) */
#define SUBLANG_SPANISH                  0x01    /* Spanish (Castilian) */
#define SUBLANG_SPANISH_MEXICAN          0x02    /* Spanish (Mexican) */
#define SUBLANG_SPANISH_MODERN           0x03    /* Spanish (Modern) */
#define SUBLANG_SPANISH_GUATEMALA        0x04    /* Spanish (Guatemala) */
#define SUBLANG_SPANISH_COSTARICA        0x05    /* Spanish (Costa Rica) */
#define SUBLANG_SPANISH_PANAMA           0x06    /* Spanish (Panama) */
#define SUBLANG_SPANISH_DOMINICAN        0x07    /* Spanish (Dominican Republic) */
#define SUBLANG_SPANISH_VENEZUELA        0x08    /* Spanish (Venezuela) */
#define SUBLANG_SPANISH_COLOMBIA         0x09    /* Spanish (Colombia) */
#define SUBLANG_SPANISH_PERU             0x10    /* Spanish (Peru) */
#define SUBLANG_SPANISH_ARGENTINA        0x11    /* Spanish (Argentina) */
#define SUBLANG_SPANISH_ECUADOR          0x12    /* Spanish (Ecuador) */
#define SUBLANG_SPANISH_CHILE            0x13    /* Spanish (Chile) */
#define SUBLANG_SPANISH_URUGUAY          0x14    /* Spanish (Uruguay) */
#define SUBLANG_SPANISH_PARAGUAY         0x15    /* Spanish (Paraguay) */
#define SUBLANG_SPANISH_BOLIVIA          0x16    /* Spanish (Bolivia) */

/* ========================= VERSIONINFO SYMBOLS =============================== */

/* define VS_FILE_INFO          RT_VERSION */
#define VS_VERSION_INFO         1
#define VS_USER_DEFINED         100

/* ----- VS_VERSION.dwFileFlags ----- */
#define VS_FFI_SIGNATURE        0xFEEF04BDL
#define VS_FFI_STRUCVERSION     0x00010000L
#define VS_FFI_FILEFLAGSMASK    0x0000003FL

/* ----- VS_VERSION.dwFileFlags ----- */
#define VS_FF_DEBUG             0x00000001L
#define VS_FF_PRERELEASE        0x00000002L
#define VS_FF_PATCHED           0x00000004L
#define VS_FF_PRIVATEBUILD      0x00000008L
#define VS_FF_INFOINFERRED      0x00000010L
#define VS_FF_SPECIALBUILD      0x00000020L

/* ----- VS_VERSION.dwFileOS ----- */
#define VOS_UNKNOWN             0x00000000L
#define VOS_DOS                 0x00010000L
#define VOS_OS216               0x00020000L
#define VOS_OS232               0x00030000L
#define VOS_NT                  0x00040000L

#define VOS__BASE               0x00000000L
#define VOS__WINDOWS16          0x00000001L
#define VOS__PM16               0x00000002L
#define VOS__PM32               0x00000003L
#define VOS__WINDOWS32          0x00000004L

#define VOS_DOS_WINDOWS16       0x00010001L
#define VOS_DOS_WINDOWS32       0x00010004L
#define VOS_OS216_PM16          0x00020002L
#define VOS_OS232_PM32          0x00030003L
#define VOS_NT_WINDOWS32        0x00040004L

/* ----- VS_VERSION.dwFileType ----- */
#define VFT_UNKNOWN             0x00000000L
#define VFT_APP                 0x00000001L
#define VFT_DLL                 0x00000002L
#define VFT_DRV                 0x00000003L
#define VFT_FONT                0x00000004L
#define VFT_VXD                 0x00000005L
#define VFT_STATIC_LIB          0x00000007L

/* ----- VS_VERSION.dwFileSubtype for VFT_WINDOWS_DRV ----- */
#define VFT2_UNKNOWN            0x00000000L
#define VFT2_DRV_PRINTER        0x00000001L
#define VFT2_DRV_KEYBOARD       0x00000002L
#define VFT2_DRV_LANGUAGE       0x00000003L
#define VFT2_DRV_DISPLAY        0x00000004L
#define VFT2_DRV_MOUSE          0x00000005L
#define VFT2_DRV_NETWORK        0x00000006L
#define VFT2_DRV_SYSTEM         0x00000007L
#define VFT2_DRV_INSTALLABLE    0x00000008L
#define VFT2_DRV_SOUND          0x00000009L
#define VFT2_DRV_COMM           0x0000000AL
#define VFT2_DRV_INPUTMETHOD    0x0000000BL

/* ----- VS_VERSION.dwFileSubtype for VFT_WINDOWS_FONT ----- */
#define VFT2_FONT_RASTER        0x00000001L
#define VFT2_FONT_VECTOR        0x00000002L
#define VFT2_FONT_TRUETYPE      0x00000003L

/* ----- VerFindFile() flags ----- */
#define VFFF_ISSHAREDFILE       0x0001

#define VFF_CURNEDEST           0x0001
#define VFF_FILEINUSE           0x0002
#define VFF_BUFFTOOSMALL        0x0004

/* ----- VerInstallFile() flags ----- */
#define VIFF_FORCEINSTALL       0x0001
#define VIFF_DONTDELETEOLD      0x0002

#define VIF_TEMPFILE            0x00000001L
#define VIF_MISMATCH            0x00000002L
#define VIF_SRCOLD              0x00000004L

#define VIF_DIFFLANG            0x00000008L
#define VIF_DIFFCODEPG          0x00000010L
#define VIF_DIFFTYPE            0x00000020L

#define VIF_WRITEPROT           0x00000040L
#define VIF_FILEINUSE           0x00000080L
#define VIF_OUTOFSPACE          0x00000100L
#define VIF_ACCESSVIOLATION     0x00000200L
#define VIF_SHARINGVIOLATION    0x00000400L
#define VIF_CANNOTCREATE        0x00000800L
#define VIF_CANNOTDELETE        0x00001000L
#define VIF_CANNOTRENAME        0x00002000L
#define VIF_CANNOTDELETECUR     0x00004000L
#define VIF_OUTOFMEMORY         0x00008000L

#define VIF_CANNOTREADSRC       0x00010000L
#define VIF_CANNOTREADDST       0x00020000L

#define VIF_BUFFTOOSMALL        0x00040000L

/* ====================================================================================== */
/* ==================== ADDITIONAL STYLES FOR COMMON CONTROLS =========================== */
/* ====================================================================================== */

#define HDS_HORZ                0x0000
#define HDS_BUTTONS             0x0002
#define HDS_HOTTRACK            0x0004
#define HDS_HIDDEN              0x0008
#define HDS_DRAGDROP            0x0040
#define HDS_FULLDRAG            0x0080

#define RBS_TOOLTIPS        0x0100
#define RBS_VARHEIGHT       0x0200
#define RBS_BANDBORDERS     0x0400
#define RBS_FIXEDORDER      0x0800
#define RBS_REGISTERDROP    0x1000
#define RBS_AUTOSIZE        0x2000
#define RBS_VERTICALGRIPPER 0x4000  // this always has the vertical gripper (default for horizontal mode)
#define RBS_DBLCLKTOGGLE    0x8000

#define TTS_ALWAYSTIP           0x01
#define TTS_NOPREFIX            0x02

#define SBARS_SIZEGRIP          0x0100

#define TBS_AUTOTICKS           0x0001
#define TBS_VERT                0x0002
#define TBS_HORZ                0x0000
#define TBS_TOP                 0x0004
#define TBS_BOTTOM              0x0000
#define TBS_LEFT                0x0004
#define TBS_RIGHT               0x0000
#define TBS_BOTH                0x0008
#define TBS_NOTICKS             0x0010
#define TBS_ENABLESELRANGE      0x0020
#define TBS_FIXEDLENGTH         0x0040
#define TBS_NOTHUMB             0x0080
#define TBS_TOOLTIPS            0x0100

#define UDS_WRAP                0x0001
#define UDS_SETBUDDYINT         0x0002
#define UDS_ALIGNRIGHT          0x0004
#define UDS_ALIGNLEFT           0x0008
#define UDS_AUTOBUDDY           0x0010
#define UDS_ARROWKEYS           0x0020
#define UDS_HORZ                0x0040
#define UDS_NOTHOUSANDS         0x0080
#define UDS_HOTTRACK            0x0100

#define PBS_SMOOTH              0x01
#define PBS_VERTICAL            0x04

//====== COMMON CONTROL STYLES ================================================

#define CCS_TOP                 0x00000001L
#define CCS_NOMOVEY             0x00000002L
#define CCS_BOTTOM              0x00000003L
#define CCS_NORESIZE            0x00000004L
#define CCS_NOPARENTALIGN       0x00000008L
#define CCS_ADJUSTABLE          0x00000020L
#define CCS_NODIVIDER           0x00000040L
#define CCS_VERT                0x00000080L
#define CCS_LEFT                (CCS_VERT | CCS_TOP)
#define CCS_RIGHT               (CCS_VERT | CCS_BOTTOM)
#define CCS_NOMOVEX             (CCS_VERT | CCS_NOMOVEY)

#define LVS_ICON                0x0000
#define LVS_REPORT              0x0001
#define LVS_SMALLICON           0x0002
#define LVS_LIST                0x0003
#define LVS_TYPEMASK            0x0003
#define LVS_SINGLESEL           0x0004
#define LVS_SHOWSELALWAYS       0x0008
#define LVS_SORTASCENDING       0x0010
#define LVS_SORTDESCENDING      0x0020
#define LVS_SHAREIMAGELISTS     0x0040
#define LVS_NOLABELWRAP         0x0080
#define LVS_AUTOARRANGE         0x0100
#define LVS_EDITLABELS          0x0200
#define LVS_OWNERDATA           0x1000
#define LVS_NOSCROLL            0x2000
#define LVS_TYPESTYLEMASK       0xfc00
#define LVS_ALIGNTOP            0x0000
#define LVS_ALIGNLEFT           0x0800
#define LVS_ALIGNMASK           0x0c00
#define LVS_OWNERDRAWFIXED      0x0400
#define LVS_NOCOLUMNHEADER      0x4000
#define LVS_NOSORTHEADER        0x8000


#define TVS_HASBUTTONS          0x0001
#define TVS_HASLINES            0x0002
#define TVS_LINESATROOT         0x0004
#define TVS_EDITLABELS          0x0008
#define TVS_DISABLEDRAGDROP     0x0010
#define TVS_SHOWSELALWAYS       0x0020
#define TVS_RTLREADING          0x0040
#define TVS_NOTOOLTIPS          0x0080
#define TVS_CHECKBOXES          0x0100
#define TVS_TRACKSELECT         0x0200
#define TVS_SINGLEEXPAND        0x0400
#define TVS_INFOTIP             0x0800
#define TVS_FULLROWSELECT       0x1000
#define TVS_NOSCROLL            0x2000
#define TVS_NONEVENHEIGHT       0x4000

#define TCS_SCROLLOPPOSITE      0x0001   // assumes multiline tab
#define TCS_BOTTOM              0x0002
#define TCS_RIGHT               0x0002
#define TCS_MULTISELECT         0x0004  // allow multi-select in button mode
#define TCS_FLATBUTTONS         0x0008
#define TCS_FORCEICONLEFT       0x0010
#define TCS_FORCELABELLEFT      0x0020
#define TCS_HOTTRACK            0x0040
#define TCS_VERTICAL            0x0080
#define TCS_TABS                0x0000
#define TCS_BUTTONS             0x0100
#define TCS_SINGLELINE          0x0000
#define TCS_MULTILINE           0x0200
#define TCS_RIGHTJUSTIFY        0x0000
#define TCS_FIXEDWIDTH          0x0400
#define TCS_RAGGEDRIGHT         0x0800
#define TCS_FOCUSONBUTTONDOWN   0x1000
#define TCS_OWNERDRAWFIXED      0x2000
#define TCS_TOOLTIPS            0x4000
#define TCS_FOCUSNEVER          0x8000

#define ACS_CENTER              0x0001
#define ACS_TRANSPARENT         0x0002
#define ACS_AUTOPLAY            0x0004
#define ACS_TIMER               0x0008  // don't use threads... use timers

#define MCS_DAYSTATE        0x0001
#define MCS_MULTISELECT     0x0002
#define MCS_WEEKNUMBERS     0x0004
#define MCS_NOTODAYCIRCLE   0x0008
#define MCS_NOTODAY         0x0010

#define DTS_UPDOWN          0x0001 // use UPDOWN instead of MONTHCAL
#define DTS_SHOWNONE        0x0002 // allow a NONE selection
#define DTS_SHORTDATEFORMAT 0x0000 // use the short date format (app must forward WM_WININICHANGE messages)
#define DTS_LONGDATEFORMAT  0x0004 // use the long date format (app must forward WM_WININICHANGE messages)
#define DTS_TIMEFORMAT      0x0009 // use the time format (app must forward WM_WININICHANGE messages)
#define DTS_APPCANPARSE     0x0010 // allow user entered strings (app MUST respond to DTN_USERSTRING)
#define DTS_RIGHTALIGN      0x0020 // right-align popup instead of left-align it

#define PGS_VERT                0x00000000
#define PGS_HORZ                0x00000001
#define PGS_AUTOSCROLL          0x00000002
#define PGS_DRAGNDROP           0x00000004

#define NFS_EDIT                0x0001
#define NFS_STATIC              0x0002
#define NFS_LISTCOMBO           0x0004
#define NFS_BUTTON              0x0008
#define NFS_ALL                 0x0010

#endif




/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef HEXVIEW_H
#define HEXVIEW_H

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*                       H E X  V I E W  C O N T R O L                           */
/* ---------------------------------------------------------------------------   */
/*  Control intended to be used in dialog boxes which displays a hex view of     */
/*  binary data.                                                                 */
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/* --------------------------------------------------------------------------- */
/*    Messages which the parent window can send to the hexview control.    */
/* --------------------------------------------------------------------------- */
#define HVM_GETBUFFPTR    (WM_USER+0)
// Returns a pointer to the internal buffer used by the control. The internal
// buffer defaults to 512 bytes. The application should be aware that this
// buffer will be destroyed when the control is destroyed.

#define HVM_SETBUFFSIZE   (WM_USER+1)
// Can be used to set a new buffer size. wParam = new buffer size in bytes.
// lParam is a repaint flag (1=repaint).
// Returns a new buffer pointer on success, NULL indicates failure (unlikely
// since the only possible failure is running out of memory).
// IMPORTANT NOTE: any previously stored buffer pointer will be invalidated
// by this function.

#define HVM_REPAINT       (WM_USER+2)
// Usually called after the application has changed the buffer contents. This
// is an alternative to calling InvalidateRect and waiting for a WM_PAINT.

#define HVM_GETOFFSET     (WM_USER+3)
// Returns the current buffer offset, i.e. the caret index.

#define HVM_SETOFFSET     (WM_USER+4)
// Sets the buffer offset to the value of wParam. lParam has a repaint flag.
// The function result is the new buffer offset, which should match the requested
// offset unless it was out of range. The control does NOT send an HVN_OFFSET_CHANGED
// notification when the buffer offset is changed in this way.
//
// Note that the control is not repainted (regardless of the lParam state) if
// wParam was out of range or if wParam matched the previous buffer offset.
//

#define HVM_GETDIMENSIONS (WM_USER+5)
// The control returns its preferred dimensions, which the application can use
// to ensure that the entire control is visible (scroll bars are not used).
// LOWORD(rslt) is the width of a line (which should ideally match the width of
// the controls client area), HIWORD(rslt) is the height of one line. This should
// be multiplied by the expected number of lines (i.e. ROUND(BuffSize/32)) to get
// the ideal height of the controls client area.
//

/* --------------------------------------------------------------------------- */
/*    The hexview control also sends notifications in WM_COMMAND messages. The */
/*    notification code is in HIWORD(wParam). LOWORD(wParam) contains the      */
/*    control ID, lParam contains the control HWND.                            */
/* --------------------------------------------------------------------------- */
#define HVN_OFFSET_CHANGED 1
// This message is sent when arrow keys or a mouse click are used to change
// the location of the text caret within the control. wParam contains the new
// byte offset in the buffer. The application can use this to update a
// status field.

#define HVN_PAGEUP         2
#define HVN_PAGEDOWN       3
// User hit the PageUp or PageDown key while the HexView control had the
// focus.

#define HVN_LINEUP         4
#define HVN_LINEDOWN       5
// User tried to scroll off the top of the buffer (LINEUP) or off the bottom
// of the buffer (LINEDOWN).
//

BOOL HexView_RegisterClass(HINSTANCE hInst);

#endif


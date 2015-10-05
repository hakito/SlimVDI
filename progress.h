/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef PROGRESS_H
#define PROGRESS_H

typedef struct {
   PSTR pszFn;         // application inits this before calling Begin() or SetFilename().
   PSTR pszMsg;        // application inits this before calling Begin() or SetMessageText().
   PSTR pszCaption;    // application inits this before calling Begin().
   double BytesDone;   // application updates this as it makes progress, before calling UpdateStats().
   double BytesTotal;  // application inits this before calling Begin()
   double StartTime;   // progress module updates this (app should ignore).
   BOOL bUserCancel;   // Either the app or the progress module can set this.
   int  old_pcent;     // app should ignore this.
   BOOL bPrintToConsole;
} ProgInfo, *PPROGINF;

/*

void Progress.Begin(HINSTANCE hInstRes, HWND hWndOwner, PPROGINF pProg);
 * Creates and displays a progress dialog, using a template called DLG_ALT_PROGRESS
 * taken from module hInstRes.
 *
 
void Progress.ResetTime(PPROGINF pProg);
 * The module records a new start time, all duration and ETA calculations will
 * then be relative to this (this call is only needed if there is a long delay
 * between calling Begin() and the first call to UpdateStats() which you don't
 * want to including in the timing.
 *

void Progress.SetMessageText(PPROGINF pProg);
void Progress.SetFilename(PPROGINF pProg);
void Progress.UpdateStats(PPROGINF pProg);
void Progress.End(PPROGINF pProg);

*/

typedef struct {
   void (*Begin)          (HINSTANCE hInstRes, HWND hWndOwner, PPROGINF pProg);
   void (*ResetTime)      (PPROGINF pProg);
   void (*SetMessageText) (PPROGINF pProg);
   void (*SetFilename)    (PPROGINF pProg);
   void (*UpdateStats)    (PPROGINF pProg);
   void (*End)            (PPROGINF pProg);
} Progress_DEF;

extern Progress_DEF Progress;

#endif


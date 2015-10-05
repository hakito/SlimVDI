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
#include "progress.h"
#include "thermo.h"
#include "env.h"
#include "ids.h"
#include "djfile.h"

#define IDD_THERMO        200
#define IDD_FILENAME      201
#define IDD_MBYTES_DONE   202
#define IDD_MBYTES_LEFT   203
#define IDD_TIME_ELAPSED  204
#define IDD_TIME_LEFT     205
#define IDD_DATA_RATE     206
#define IDD_MESSAGE       300

static HWND hWndProgress;
static PSTR pszDLG_ALT_PROGRESS;

/*................................................................*/

BOOL pascal
ProgressDlgProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_INITDIALOG: {
         PPROGINF pProg = (PPROGINF)lParam;
         SetWindowLong(hDlg,DWL_USER,lParam);
         SetDlgItemText(hDlg,IDD_FILENAME,"");
         SetDlgItemText(hDlg,IDD_MESSAGE,pProg->pszMsg);
         break;
     }

      case WM_COMMAND: /* Note that the dialog doesn't necessarily have these buttons */
         if (LOWORD(wParam)==IDCANCEL) {
            PPROGINF pProg = (PPROGINF)GetWindowLong(hDlg,DWL_USER);
            if (pProg) {
               pProg->bUserCancel = TRUE;
            }
         }
         break;

      default:
         return FALSE;
   }
   return TRUE;
}

/*..........................................................*/

static double near
GetCurrentTimeInSeconds(void)
{
   SYSTEMTIME st;
   GetSystemTime(&st);
   return st.wHour*3600.0 + st.wMinute*60.0 + st.wSecond*1.0 + st.wMilliseconds*(1e-3);
}

/*..........................................................*/

static double near
CalcElapsedTimeInSeconds(PPROGINF pProg)
{
   double s = GetCurrentTimeInSeconds();
   if (s <= pProg->StartTime) {
      s += 86400; // # of seconds in one day.
   }
   return s - pProg->StartTime;
}

/*..........................................................*/

static UINT
YieldCPU(void)
{
   UINT gotmsg = 0;
   MSG msg;
   if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
      gotmsg = 1;
      TranslateMessage (&msg);
      DispatchMessage (&msg);
   }
   return gotmsg;
}

/*.....................................................*/

static void
Progress_Begin(HINSTANCE hInstRes, HWND hWndOwner, PPROGINF pProg)
{
   pProg->StartTime = GetCurrentTimeInSeconds();
   pProg->bUserCancel = FALSE;

   if (!pProg->bPrintToConsole && !hWndProgress) {
      while (YieldCPU());
      hWndProgress = CreateDialogParam(hInstRes,RSTR(DLG_ALT_PROGRESS),hWndOwner,ProgressDlgProc,(LPARAM)pProg);
      if (hWndProgress) SetWindowText(hWndProgress, pProg->pszCaption);
      while (YieldCPU());
   }
}

/*................................................................*/

static void
Progress_ResetTime(PPROGINF pProg)
{
   pProg->StartTime = GetCurrentTimeInSeconds();
}

/*................................................................*/

static void
Progress_SetMessageText(PPROGINF pProg)
{
   if (hWndProgress) {
      SetDlgItemText(hWndProgress,IDD_MESSAGE,pProg->pszMsg);
      while (YieldCPU());
   }
}

/*................................................................*/

static void
Progress_SetFilename(PPROGINF pProg)
{
   if (hWndProgress) {
      SetDlgItemText(hWndProgress,IDD_FILENAME,pProg->pszFn);
      while (YieldCPU());
   }
}

/*................................................................*/

#define HOURS_SCALE 2.77777777777777777778e-4
#define MINS_SCALE  0.0166666666666666666

static void near
ShowTime(HWND hWnd, UINT idCtl, double t)
{
   char sz[32];
   if (t>7200) { // if >= 2 hours.
      int hours,mins;
      hours = (int)(t * HOURS_SCALE);
      mins = (int)((t-hours*3600.0) * MINS_SCALE);
      if (mins>=60) { mins=0; hours++; } // rounding sometimes gives us a nonsense such as 1:60.
      wsprintf(sz,"%luHr %lumin",hours,mins);
   } else {
      int mins,secs;
      mins = (int)(t * MINS_SCALE);
      secs = (int)(t - mins*60 + 0.5);
      if (secs>=60) { secs=0; mins++; }
      wsprintf(sz,"%lumin %lusec",mins,secs);
   }
   SetDlgItemText(hWnd,idCtl,sz);
}

/*................................................................*/

static void
Progress_UpdateStats(PPROGINF pProg)
{
   if (pProg->bPrintToConsole) {
      int pcent = (pProg->BytesTotal <= 1 ? 100 : (int)((pProg->BytesDone * 100.0 / pProg->BytesTotal) + 0.5));
      int deltaPcent = pcent - pProg->old_pcent;
      if (deltaPcent > 0) {
         FILE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
         if (stdout) {
            char sz[128];
            for (int i = 0; i < deltaPcent; ++i)
               sz[i] = '.';
            sz[deltaPcent] = 0;

            File_WrBin(stdout, sz, deltaPcent);
         }
      }
      pProg->old_pcent = pcent;
      return;
   }

   if (hWndProgress) {
      double BytesLeft = pProg->BytesTotal-pProg->BytesDone;
      char sz[64];
      int pcent;

      if (pProg->BytesTotal>1) pcent = (int)((pProg->BytesDone*100.0 / pProg->BytesTotal) + 0.5);
      else pcent = 100;
      Env_DoubleToString(sz,pProg->BytesDone*1e-6,0);
      SetDlgItemText(hWndProgress,IDD_MBYTES_DONE,sz);
      Env_DoubleToString(sz,BytesLeft*1e-6,0);
      SetDlgItemText(hWndProgress,IDD_MBYTES_LEFT,sz);

      if (pProg->BytesDone>1) {
         double t = CalcElapsedTimeInSeconds(pProg);

         SendMessage(GetDlgItem(hWndProgress,IDD_THERMO),THM_SETPCENTAGE,pcent,pcent);

         if (t>0) {
            ShowTime(hWndProgress,IDD_TIME_ELAPSED,t);
            t = pProg->BytesDone / t; // convert to a data rate.
            if (t>1000000) {
               Env_DoubleToString(sz,t*1e-6,2);
               lstrcat(sz,"Mb/s");
            } else {
               Env_DoubleToString(sz,t*1e-3,2);
               lstrcat(sz,"Kb/s");
            }
            SetDlgItemText(hWndProgress,IDD_DATA_RATE,sz);
            ShowTime(hWndProgress,IDD_TIME_LEFT,BytesLeft / t); // show time remaining.
         }
      } else {
         SetDlgItemText(hWndProgress,IDD_TIME_ELAPSED,"");
         SetDlgItemText(hWndProgress,IDD_TIME_LEFT,"");
         SetDlgItemText(hWndProgress,IDD_DATA_RATE,"");
         SendMessage(GetDlgItem(hWndProgress,IDD_THERMO),THM_SETPCENTAGE,pcent,pcent);
      }

      while (YieldCPU());
   }
}

/*................................................................*/

static void
Progress_End(PPROGINF pProg)
{
   if (pProg->bPrintToConsole) {
      FILE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
      if (stdout) File_WrBin(stdout, "\r\n", 2);
      return;
   }

   if (hWndProgress) {
      DestroyWindow(hWndProgress);
      hWndProgress = 0;
      while (YieldCPU());
   }
}

/*................................................................*/

Progress_DEF Progress = {
   Progress_Begin,
   Progress_ResetTime,
   Progress_SetMessageText,
   Progress_SetFilename,
   Progress_UpdateStats,
   Progress_End
};

/*................................................................*/

/* end of progress.c */


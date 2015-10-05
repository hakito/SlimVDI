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
#include "hexview.h"
#include "mem.h"

#define GWL_PHEXVIEW    0
#define WNDEXTRABYTES   4

#define X_PAD           2
#define Y_PAD           2
#define COLS            32

typedef struct {
   UINT    idCtl;
   UINT    iCaret;
   UINT    BuffSize;
   HDC     hMemDC;
   HBITMAP hBm;
   HFONT   hCreateFont;
   HFONT   hFont;
   BYTE    buffer[512]; // actually variable length, to end of record.
} HexViewInfo, *PHV;

static PSTR szHexViewClass = "SSCCHexView";

static HPEN   hPenGutter;
static HBRUSH hBrushRow;

/*...........................................*/

static void
GetCharSpacing(HDC hDC, SIZE *step)
{
   GetTextExtentPoint32(hDC,"8",1,step);
}

/*...........................................*/

static void
PaintControl(HWND hWnd, HDC hDCin)
{
   HDC hDC;
   PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
   HFONT hOldFont;
   HPEN  hOldPen;
   SIZE step;
   UINT i,j,k,iCaret=p->iCaret;
   INT  spacing[3];
   char sz[16];
   RECT r,rcClip;

   GetClientRect(hWnd,&r);

   if (!p->hMemDC) {
      p->hMemDC = CreateCompatibleDC(hDCin);
      p->hBm = CreateCompatibleBitmap(hDCin,r.right,r.bottom);
      p->hBm = SelectObject(p->hMemDC, p->hBm);
   }
   hDC = p->hMemDC;
   hOldFont = SelectObject(hDC,p->hFont);
   hOldPen = SelectObject(hDC,hPenGutter);

   GetCharSpacing(hDC,&step);
   spacing[0] = spacing[1] = spacing[2] = (INT)(step.cx);
   SetBkMode(hDC, TRANSPARENT);
   SetTextColor(hDC, 0);
   
   FillRect(hDC,&r,GetStockObject(WHITE_BRUSH));

   if (GetFocus()!=hWnd) iCaret = 0xFFFFFFFF;
   else {
      rcClip.top    = (iCaret/COLS)*(step.cy+Y_PAD);
      rcClip.bottom = rcClip.top + (step.cy+Y_PAD);
      rcClip.left   = 0;
      rcClip.right  = r.right;
//    FillRect(hDC,&rcClip,(HBRUSH)(COLOR_BTNFACE+1));
      FillRect(hDC,&rcClip,hBrushRow);
   }

   MoveToEx(hDC,step.cx*3+2,0,NULL);
   LineTo(hDC,step.cx*3+2,r.bottom);

   i = step.cx*3+6 + COLS*(step.cx*2+X_PAD) + 1;
   MoveToEx(hDC,i,0,NULL);
   LineTo(hDC,i,r.bottom);

   i += 2;
   MoveToEx(hDC,i,0,NULL);
   LineTo(hDC,i,r.bottom);

   i += (2 + step.cx*COLS + 1);
   MoveToEx(hDC,i,0,NULL);
   LineTo(hDC,i,r.bottom);

   rcClip.top = 0;
   for (i=0; i<p->BuffSize; i+=COLS) {
      wsprintf(sz,"%03lx",i);
      rcClip.bottom = rcClip.top+step.cy;
//    if (iCaret>=i && iCaret<(i+COLS)) {
//       COLORREF bkclr = SetBkColor(hDC, RGB(192,192,192));
//       rcClip.left = 0;
//       rcClip.right = step.cx*3+1;
//       ExtTextOut(hDC,rcClip.left,rcClip.top,ETO_OPAQUE,&rcClip,sz,3,spacing);
//       SetBkColor(hDC, bkclr);
//    } else {
         ExtTextOut(hDC,0,rcClip.top,0,&r,sz,3,spacing);
//    }
      k = i+COLS;
      if (k>p->BuffSize) k = p->BuffSize;
      rcClip.left   = step.cx*3+6;
      for (j=i; j<k; j++) {
         wsprintf(sz,"%02lx",p->buffer[j]);
         rcClip.right = rcClip.left + step.cx*2;
         if (j==iCaret) {
            COLORREF bkclr = SetBkColor(hDC, 0);
            COLORREF tclr = SetTextColor(hDC,RGB(255,255,255));
            ExtTextOut(hDC,rcClip.left,rcClip.top,ETO_OPAQUE,&rcClip,sz,2,spacing);
            SetBkColor(hDC, bkclr);
            SetTextColor(hDC,tclr);
         } else {
            ExtTextOut(hDC,rcClip.left,rcClip.top,0,&rcClip,sz,2,spacing);
         }
         rcClip.left += step.cx*2+X_PAD;
      }
      rcClip.left += 5;
      sz[1] = (char)0;
      for (j=i; j<k; j++) {
         sz[0] = p->buffer[j];
         if (sz[0]<' ') sz[0] = ' ';
         rcClip.right = rcClip.left + step.cx;
         if (j==iCaret) {
            COLORREF bkclr = SetBkColor(hDC, 0);
            COLORREF tclr = SetTextColor(hDC,RGB(255,255,255));
            ExtTextOut(hDC,rcClip.left,rcClip.top,ETO_OPAQUE,&rcClip,sz,1,spacing);
            SetBkColor(hDC, bkclr);
            SetTextColor(hDC,tclr);
         } else if (sz[0]!=' ') {
            ExtTextOut(hDC,rcClip.left,rcClip.top,0,&rcClip,sz,1,spacing);
         }
         rcClip.left += step.cx;
      }
      rcClip.top += (step.cy+Y_PAD);
   }

   SelectObject(hDC,hOldFont);
   SelectObject(hDC,hOldPen);

   BitBlt(hDCin,0,0,r.right,r.bottom,p->hMemDC,0,0,SRCCOPY);
}

/*...........................................*/

static void
HandleMouseClick(HWND hWnd, int mx, int my)
{
   PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
   SIZE step;
   int i,xLeft,rows = ((p->BuffSize+(COLS-1))/COLS);
   UINT iCaret = p->iCaret;
   HDC hDC = GetDC(hWnd);
   HFONT hFont = SelectObject(hDC,p->hFont);
   GetCharSpacing(hDC,&step);
   SelectObject(hDC,hFont);
   ReleaseDC(hWnd,hDC);

   my /= (step.cy+Y_PAD);
   if (my>rows) return; // off bottom edge

   i = (step.cx*3+6);
   if (mx<i) return; // in left margin
   xLeft = i;

   i += (COLS*(step.cx*2+X_PAD));
   if (mx<i) {
      // in middle pane
      mx = (mx-xLeft) / (step.cx*2+X_PAD);
      iCaret = my*COLS+mx;
   } else {
      xLeft = i+5;
      if (mx<xLeft) return; // in central gutter
      i += (5+COLS*step.cx);
      if (mx<i) {
         // in right pane
         mx = (mx-xLeft) / (step.cx);
         iCaret = my*COLS+mx;
      }
   }
   if (iCaret!=p->iCaret && iCaret<p->BuffSize) {
      p->iCaret = iCaret;
   }
}

/*...........................................*/

static void
RepaintControl(HWND hWnd)
{
   HDC hDC = GetDC(hWnd);
   PaintControl(hWnd,hDC);
   ReleaseDC(hWnd,hDC);
}

/*...........................................*/

LRESULT pascal
HexViewWndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_CREATE: {
         PHV p = Mem_Alloc(MEMF_ZEROINIT, sizeof(HexViewInfo));
         HDC hDC = GetDC(0);
         int nHeight = -MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72);
         ReleaseDC(0,hDC);
         p->hCreateFont = CreateFont(nHeight,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,
                   CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,"Courier New");
         p->hFont = p->hCreateFont;
         p->BuffSize = 512;
         p->idCtl = GetWindowLong(hWnd, GWL_ID);
         SetWindowLong(hWnd, GWL_PHEXVIEW, (LONG)p);
         if (!hPenGutter) {
            hPenGutter = CreatePen(PS_SOLID,1,RGB(192,192,192));
            hBrushRow  = CreateSolidBrush(RGB(208,228,246));
         }
         break;
      }
      case WM_SETFONT: {
//       PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
//       p->hFont = (HFONT)wParam;
//       if (lParam) InvalidateRect(hWnd, NULL, FALSE);
         break;
      }
      case WM_GETFONT: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         return (LRESULT)(p->hFont);
      }
      case WM_GETDLGCODE:
         return DLGC_WANTARROWS;
      case WM_ERASEBKGND:
         return 1;
      case WM_PRINTCLIENT:
         PaintControl(hWnd, (HDC)wParam);
         break;
      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hDC = (wParam ? (HDC)wParam : BeginPaint(hWnd,&ps));
         PaintControl(hWnd, hDC);
         if (wParam) ValidateRect(hWnd,NULL);
         else EndPaint(hWnd, &ps);
         break;
      }
      case WM_LBUTTONDOWN: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         UINT oldCaret = p->iCaret;
         HandleMouseClick(hWnd,LOWORD(lParam),HIWORD(lParam));
         if (p->iCaret != oldCaret) SendMessage(GetParent(hWnd),WM_COMMAND,MAKEWPARAM(p->idCtl,HVN_OFFSET_CHANGED),(LPARAM)hWnd);
         if (GetFocus()==hWnd) RepaintControl(hWnd);
         else SetFocus(hWnd);
         break;
      }
      case WM_KEYDOWN: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         UINT oldCaret = p->iCaret;
         if (wParam == VK_LEFT) {
            if (oldCaret) p->iCaret--;
         } else if (wParam==VK_RIGHT) {
            if ((oldCaret+1) < p->BuffSize) p->iCaret++;
         } else if (wParam==VK_UP) {
            if (oldCaret>=COLS) p->iCaret -= COLS;
            else SendMessage(GetParent(hWnd),WM_COMMAND, MAKEWPARAM(p->idCtl,HVN_LINEUP),(LPARAM)hWnd);
         } else if (wParam==VK_DOWN) {
            if ((oldCaret+COLS) < p->BuffSize) p->iCaret += COLS;
            else SendMessage(GetParent(hWnd),WM_COMMAND, MAKEWPARAM(p->idCtl,HVN_LINEDOWN),(LPARAM)hWnd);
         } else if (wParam==VK_PRIOR) {
            SendMessage(GetParent(hWnd),WM_COMMAND, MAKEWPARAM(p->idCtl,HVN_PAGEUP),(LPARAM)hWnd);
            return 0;
         } else if (wParam==VK_NEXT) {
            SendMessage(GetParent(hWnd),WM_COMMAND, MAKEWPARAM(p->idCtl,HVN_PAGEDOWN),(LPARAM)hWnd);
            return 0;
         }
         if (p->iCaret != oldCaret) {
            RepaintControl(hWnd);
            SendMessage(GetParent(hWnd),WM_COMMAND,MAKEWPARAM(p->idCtl,HVN_OFFSET_CHANGED),(LPARAM)hWnd);
         }
         break;
      }
      case WM_SETFOCUS:
      case WM_KILLFOCUS:
      case HVM_REPAINT:
         RepaintControl(hWnd);
         break;

      /* ctrl-specific messages */
      case HVM_GETBUFFPTR: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         return (LRESULT)(p->buffer);
      }
      case HVM_SETBUFFSIZE: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         p = Mem_ReAlloc(p,MEMF_ZEROINIT,sizeof(HexViewInfo)-512+wParam);
         p->BuffSize = (UINT)wParam;
         if (p->iCaret>p->BuffSize) p->iCaret = 0;
         if (lParam) RepaintControl(hWnd);
         // Can be used to set a new buffer size. wParam = new buffer size in bytes.
         // Returns TRUE on success.
         // IMPORTANT NOTE: any previously stored buffer pointer will be invalidated
         // by this function.
         break;
      }
      case HVM_GETOFFSET: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         return (LRESULT)(p->iCaret);
      }
      case HVM_SETOFFSET: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         if (wParam<p->BuffSize && wParam!=p->iCaret) {
            p->iCaret = (UINT)wParam;
            if (lParam) RepaintControl(hWnd);
         }
         return (LRESULT)(p->iCaret);
      }
      case HVM_GETDIMENSIONS: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         HDC hDC = GetDC(0);
         HFONT hFont = SelectObject(hDC,p->hFont);
         int w,h;
         SIZE step;
         GetCharSpacing(hDC,&step);
         SelectObject(hDC,hFont);
         ReleaseDC(0,hDC);
         h = step.cy+Y_PAD;
         w = step.cx*(3 + 3*COLS) + COLS*X_PAD + 16;
         return MAKELRESULT(w,h);
      }

      case WM_NCDESTROY: {
         PHV p = (PHV)GetWindowLong(hWnd,GWL_PHEXVIEW);
         p->hBm = SelectObject(p->hMemDC, p->hBm);
         DeleteObject(p->hBm);
         DeleteDC(p->hMemDC);
         if (p->hCreateFont) DeleteObject(p->hCreateFont);
         Mem_Free(p);
         break;
      }

      default:
         return DefWindowProc(hWnd,iMsg,wParam,lParam);
   }
   return 0;
}

/*................................................*/

PUBLIC BOOL
HexView_RegisterClass(HINSTANCE hInstance)
{
   WNDCLASS wndclass;

   wndclass.style         = CS_HREDRAW|CS_VREDRAW;
   wndclass.cbClsExtra    = 0;
   wndclass.cbWndExtra    = WNDEXTRABYTES;
   wndclass.hInstance     = hInstance;
   wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndclass.lpfnWndProc   = (WNDPROC)HexViewWndProc;
   wndclass.hIcon         = NULL;
   wndclass.hbrBackground = NULL;
   wndclass.lpszMenuName  = NULL;
   wndclass.lpszClassName = szHexViewClass;

   if (!RegisterClass(&wndclass)) {
      return(FALSE);
   }
   return TRUE;
}

/*...........................................*/

/* end of hexview.c */


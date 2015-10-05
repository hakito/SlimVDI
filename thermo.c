/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/* Implements a "thermometer" control, as used by the progress dialog. */

#include "djwarning.h"
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <math.h>
#include "thermo.h"

#define THREE_D_LOOK  1

#define GWL_PCENTAGE  0   /* Most recent % figure set by app */
#define GWL_NUMTOSHOW 4   /* Number to show in control */
#define GWL_FONT      8   /* HFONT set by WM_SETFONT message */
#define GWL_HBITMAP   12  /* HBITMAP used to paint the control, preventing flicker */
#define GWL_HMEMDC    16  /* Memory HDC used to paint the control, preventing flicker */
#define WNDEXTRABYTES 20

static PSTR szThermoClass = "SSCCThermo";

#if THREE_D_LOOK

//define THERMO_COLOR  RGB(0,253,240)
//define THERMO_COLOR  RGB(220,0,0)
#define THERMO_COLOR  RGB(0,255,0)

typedef struct {
   double x,y,z;
} VECT, *PVECT;

/* Normalized eye vector */
#define EX 0
#define EY -1
#define EZ 0

/* Normalized light source vector */
#define LX -0.5774
#define LY -0.5774
#define LZ 0.5774

#else

#define THERMO_COLOR  RGB(0,0,255)

#endif

/*...........................................*/

#if THREE_D_LOOK

static COLORREF near
ColorIntensity(PVECT uN, COLORREF C, int spec_thresh, int spec_thresh2)
{
   VECT N,R;
   double rM,LdotN,f;

   /* Normalise vector */
   rM = 1/sqrt(uN->x*uN->x + uN->y*uN->y + uN->z*uN->z); /* reciprocal of magnitude */
   N.x=uN->x*rM; N.y=uN->y*rM; N.z=uN->z*rM;

   /* LdotN - angle between light vector and surface normal */
   LdotN = (LX*N.x + LY*N.y + LZ*N.z);

   /* Calculate reflection vector */
   R.x = 2 * N.x * LdotN - LX;
   R.y = 2 * N.y * LdotN - LY;
   R.z = 2 * N.z * LdotN - LZ;

   /* Calculate fraction of reflected light going along eye vector,
    *  = R dot EyeVector
    */
   f = 0.63+0.37*(R.x*EX + R.y*EY + R.z*EZ);
   if (uN->z>=spec_thresh && uN->z<spec_thresh2) f *= 1.8; // add specular highlight.
   if (f<0) f=0; else if (f>1) f=1;
   return RGB((int)(GetRValue(C)*f),
              (int)(GetGValue(C)*f),
              (int)(GetBValue(C)*f));
}

#endif

/*............................................................*/

#if THREE_D_LOOK

static void near
Render_3DBar(HDC hDC, int xmin, int xmax, int bar_height, COLORREF BaseColor)
{
   int    x,z,zmin,zmax,spec_thresh,spec_thresh2;
   double radius,rad2;
   COLORREF pix;
   VECT   N;

   radius = (bar_height>>1);
   rad2   = 1.0 / (radius*radius);
   zmax   = (bar_height>>1);
   spec_thresh  = 2;
   spec_thresh2 = zmax;
   zmin   = -zmax;
   N.x    = 0;

   for (z=zmax; z>zmin; z--) {
      N.z = z;
      N.y = (-(radius * sqrt(1 - z*z*rad2)));
      pix = ColorIntensity(&N,BaseColor,spec_thresh,spec_thresh2);
      for (x=xmin; x<xmax; x++) {
         SetPixel(hDC,x,zmax-z,pix);
      }
   }
}

#endif

/*............................................................*/

#if THREE_D_LOOK

static void
PaintThermo(HWND hWnd, HDC hDC)
{
   RECT   r,fr;
   int    bluebit,slen;
   char   s[80],szControl[80];
   DWORD  PcentComplete;
   HFONT  hFont,hOldFont=NULL;
   SIZE   tsize;
   DWORD  NumToShow;
   HDC    hMemDC = (HDC)GetWindowLong(hWnd, GWL_HMEMDC);

   PcentComplete = GetWindowLong(hWnd, GWL_PCENTAGE);
   NumToShow     = GetWindowLong(hWnd, GWL_NUMTOSHOW);
   hFont         = (HFONT)GetWindowLong(hWnd, GWL_FONT);
   if (!hFont) hFont = GetStockObject(DEFAULT_GUI_FONT);
   GetWindowText(hWnd, szControl, 80);

   GetClientRect(hWnd, &r);
   bluebit = (r.right*PcentComplete)/100;
   if (bluebit>r.right) bluebit=r.right;

   if (!hMemDC) {
      HBITMAP hbm;
      hMemDC = CreateCompatibleDC(hDC);
      hbm = CreateCompatibleBitmap(hDC,r.right,r.bottom);
      hbm = SelectObject(hMemDC, hbm);
      SetWindowLong(hWnd, GWL_HBITMAP, (LONG)hbm);
      SetWindowLong(hWnd, GWL_HMEMDC, (LONG)hMemDC);
   }

   if (!szControl[0]) s[0]=0;
   else wsprintf (s,szControl,NumToShow);
   slen = lstrlen(s);

   if (slen && hFont) hOldFont=SelectObject(hDC, hFont);

   GetTextExtentPoint32(hDC,s,slen,&tsize);

   if (bluebit>0) {
      Render_3DBar(hMemDC,0,bluebit,r.bottom,THERMO_COLOR);

      SetTextColor(hMemDC,RGB(255,255,255)); // text is white against a blue background
      SetBkMode(hMemDC,TRANSPARENT);

      fr.left=0; fr.top=0;
      fr.right = bluebit;
      fr.bottom = r.bottom;
      ExtTextOut(hMemDC, (r.right-tsize.cx)>>1,(r.bottom-tsize.cy)>>1,
                 ETO_CLIPPED,&fr,s,slen,NULL);
   }

   if (bluebit<r.right) {
//    Render_3DBar(hMemDC,bluebit,r.right,r.bottom,RGB(250,250,250));
      SetBkColor(hMemDC,RGB(255,255,255));
      SetTextColor(hMemDC,0);  // text is black gainst a white background
      fr.left=bluebit; fr.top=0;
      fr.right = r.right;
      fr.bottom = r.bottom;
      ExtTextOut(hMemDC, (r.right-tsize.cx)>>1,(r.bottom-tsize.cy)>>1,
                 ETO_OPAQUE | ETO_CLIPPED,&fr,s,slen,NULL);
   }

   BitBlt(hDC,0,0,r.right,r.bottom,hMemDC,0,0,SRCCOPY);

   if (slen && hFont) SelectObject(hDC, hOldFont);
}

#endif

/*...........................................*/

#if !THREE_D_LOOK

static void
PaintThermo(HWND hWnd, HDC hDC)
{
   RECT   r,fr;
   int    bluebit,slen;
   char   s[80],szControl[80];
   DWORD  PcentComplete;
   HFONT  hFont,hOldFont;
   SIZE   tsize;
   DWORD  NumToShow;

   PcentComplete = GetWindowLong(hWnd, GWL_PCENTAGE);
   NumToShow     = GetWindowLong(hWnd, GWL_NUMTOSHOW);
   hFont         = (HFONT)GetWindowLong(hWnd, GWL_FONT);
   GetWindowText(hWnd, szControl, 80);

   GetClientRect(hWnd, &r);
   bluebit = (r.right*PcentComplete)/100;
   if (bluebit>r.right) bluebit=r.right;

   if (!szControl[0]) s[0]=0;
   else wsprintf (s,szControl,NumToShow);
   slen = lstrlen(s);

   if (slen && hFont) hOldFont=SelectObject(hDC, hFont);

   GetTextExtentPoint32(hDC,s,slen,&tsize);

   if (bluebit>0) {
      SetBkColor(hDC,THERMO_COLOR);
      SetTextColor(hDC,RGB(255,255,255)); // text is white against a blue background
      fr.left=0; fr.top=0;
      fr.right = bluebit;
      fr.bottom = r.bottom;
      ExtTextOut(hDC, (r.right-tsize.cx)>>1,(r.bottom-tsize.cy)>>1,
                 ETO_OPAQUE | ETO_CLIPPED,&fr,s,slen,NULL);
   }

   if (bluebit<r.right) {
      SetBkColor(hDC,RGB(255,255,255));
      SetTextColor(hDC,THERMO_COLOR);  // text is blue (or same as thermo) against a white background
      fr.left=bluebit; fr.top=0;
      fr.right = r.right;
      fr.bottom = r.bottom;
      ExtTextOut(hDC, (r.right-tsize.cx)>>1,(r.bottom-tsize.cy)>>1,
                 ETO_OPAQUE | ETO_CLIPPED,&fr,s,slen,NULL);
   }

   if (slen && hFont) SelectObject(hDC, hOldFont);
}

#endif

/*...........................................*/

LRESULT pascal
ThermoWndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   switch (iMsg) {
      case WM_CREATE:
         break;
      case WM_SETFONT:
         SetWindowLong(hWnd, GWL_FONT, wParam);
         if (lParam) InvalidateRect(hWnd, NULL, FALSE);
         break;
      case WM_GETFONT:
         return (LRESULT)GetWindowLong(hWnd, GWL_FONT);
      case WM_GETDLGCODE:
         return 0;
      case WM_ERASEBKGND:
         return 1;
      case WM_PRINTCLIENT:
         PaintThermo(hWnd, (HDC)wParam);
         break;
      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hDC = (wParam ? (HDC)wParam : BeginPaint(hWnd,&ps));
         PaintThermo(hWnd, hDC);
         if (wParam) ValidateRect(hWnd,NULL);
         else EndPaint(hWnd, &ps);
         break;
      }

      /* ctrl-specific messages */
      case THM_SETPCENTAGE: {
         HDC hDC;
         SetWindowLong(hWnd,GWL_PCENTAGE,wParam);
         SetWindowLong(hWnd,GWL_NUMTOSHOW,lParam);
         hDC = GetDC(hWnd);
         PaintThermo(hWnd, hDC);
         ReleaseDC(hWnd, hDC);
         break;
      }

      case WM_DESTROY: {
         HDC hMemDC = (HDC)GetWindowLong(hWnd, GWL_HMEMDC);
         if (hMemDC) {
            HBITMAP hOldBm = (HBITMAP)GetWindowLong(hWnd, GWL_HBITMAP);
            DeleteObject(SelectObject(hMemDC, hOldBm));
            DeleteDC(hMemDC);
         }
         break;
      }

      default:
         return DefWindowProc(hWnd,iMsg,wParam,lParam);
   }
   return 0;
}

/*................................................*/

static BOOL
Thermo_RegisterClass(HINSTANCE hInstance)
{
   WNDCLASS wndclass;

   wndclass.style         = CS_HREDRAW|CS_VREDRAW;
   wndclass.cbClsExtra    = 0;
   wndclass.cbWndExtra    = WNDEXTRABYTES;
   wndclass.hInstance     = hInstance;
   wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndclass.lpfnWndProc   = (WNDPROC)ThermoWndProc;
   wndclass.hIcon         = NULL;
   wndclass.hbrBackground = NULL;
   wndclass.lpszMenuName  = NULL;
   wndclass.lpszClassName = szThermoClass;

   if (!RegisterClass(&wndclass)) return(FALSE);
   return TRUE;
}

/*...........................................*/

Thermo_DEF Thermo = {
   Thermo_RegisterClass
};

/*...........................................*/

/* end of thermc.c */


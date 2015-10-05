/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef THERMO_H
#define THERMO_H

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*                T H E R M O M E T E R  C O N T R O L S                         */
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/* --------------------------------------------------------------------------- */
/*    Messages which the parent window can send to the thermometer control.    */
/* --------------------------------------------------------------------------- */

#define THM_SETPCENTAGE (WM_USER+0)
/* nPcent   = WPARAM
 * nNumShow = LPARAM
 *
 * The fill level of the thermometer control is set by a 'pcentage complete' value
 * passed in WPARAM. The number to actually be shown on the text of the control
 * is passed in LPARAM. The number will be formatted using a 'wsprintf' format
 * control string stored in the caption text for the control - if the caption
 * is a null string then no text is displayed in the thermometer control.
 */

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/* implement thermometer control *

BOOL Thermo.RegisterClass(HINSTANCE hInstDLL);
 * Register the thermometer control class.
 *
 */

typedef struct {
   BOOL (*RegisterClass)    (HINSTANCE hInstDLL);
} Thermo_DEF;

extern Thermo_DEF Thermo;

#endif


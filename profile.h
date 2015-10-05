/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef PROFILE_H
#define PROFILE_H

void Profile_Init(HINSTANCE hInstApp, PSTR Basename);
void Profile_GetString(PSTR szOptionName, PSTR szValue, DWORD cbMax, PSTR szDefault);
void Profile_SetString(PSTR szOptionName, PSTR szValue);
int  Profile_GetOption(PSTR szOptionName);

#endif

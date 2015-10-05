/*================================================================================*/
/* Copyright (C) 2011, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

/*================================================================================*/
/* The Media Registry is a table in which filenames can be looked up using UUID,  */
/* and perhaps vice versa too.                                                    */
/*                                                                                */
/* VirtualBox v4 introduced a new folder arrangement and a distributed media      */
/* registry, i.e. parts of the media registry are stored in each VM settings xml  */
/* and also (for legacy reasons) still in VirtualBox.xml.  I dislike the idea of  */
/* trying to access this complexity from CloneVDI, so I've tried to come up with  */
/* something much simpler and more robust: I'll simply build an implied media     */
/* registry by scanning all the virtual disk files in a folder. I'll use various  */
/* tricks and logic to guess which folders to look in.                            */
/*================================================================================*/


#ifndef MEDIAREG_H
#define MEDIAREG_H

#include "djtypes.h"

void MediaReg_Discard(void);
/* Discard (reset) the current media registry, if any.
 */

BOOL MediaReg_ScanFolder(CPFN pfn);
/* Given a filename, I scan all the filenames in the folder which includes that file,
 * looking for virtual disk files. If found I extract their UUID and add them to an
 * internal media registry. If the folder name is "Snapshots" then I also scan
 * the parent folder.
 */

PFN MediaReg_FilenameFromUUID(S_UUID *pUUID);
/* Given a UUID, search the media registry and return the associated filename. Returns
 * NULL if the UUID was not found.
 */

#endif

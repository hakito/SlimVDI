/*================================================================================*/
/* Copyright (C) 2010, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef DJXML_H
#define DJXML_H

/*================================================================================*/
/*               Simple XML Input File Parsing and Data Extraction                */
/*================================================================================*/

typedef struct {UINT dummy;} *XMLDOC;
typedef struct {UINT dummy;} *XMLELE;

#include "djtypes.h"

UINT XML_GetLastError(void);
PSTR XML_GetLastErrorString(UINT nErr);

XMLDOC XML_Open(CPFN xmlfn);
/* Creates a container inside which will be stored data extracted from an XML
 * file. The xmlfn argument can optionally specify a first xml file to parse and
 * store in the container. Further XML documents can (if necessary) be loaded into
 * the same container using XML_Load(). The container basically implements the
 * tree structure implied by the XML, in a form which can be queried efficiently.
 *
 * This function returns a handle to the new container, or NULL on failure.
 * XML_GetLastError() can be called to retrieve the cause of any failure.
 */

XMLDOC XML_Close(XMLDOC hXML);
/* Closes this XML container.
 */

XMLELE XML_FindElement(XMLDOC hXML, CPCHAR path);
/* Returns a reference to an XML tree node, selected by its path. The path consists
 * of the (case sensitive) name of every node on the trail from the root of the tree,
 * terminating at the desired node. If the named element node exists then this function
 * returns a reference to it.
 *
 * Notes:
 *  o The path separator is NUL, with a double NUL to terminate the list,
 *    eg. "VirtualBox\0Global\0MediaRegistry\0HardDisks\0\0" is a valid
 *    search path.
 *  o Element names are case sensitive.
 *  o Paths only check element names, attribute names are not part of 
 *    any path.
 *  o XML itself does not require node names to be unique, so if a conflict
 *    exists then an arbitrary branch will be taken. This function is only
 *    useful therefore if a particular doctype mandates that a node name will
 *    be unique (at least among sibling nodes).
 */

XMLELE XML_FirstChild(XMLELE hEle);
/* Returns a handle to the first child of the selected tree node. You can
 * cast an XMLDOC reference to a XMLELE reference to get the root child, or you
 * can also get a node reference by using FindElement(). Returns NULL if hEle
 * has no children.
 */

XMLELE XML_NextChild(XMLELE hEle);
/* Having finished with the first child of a parent you may then use this
 * function to fetch the next child of the same parent. hEle is the sibling you
 * just finished with. Returns NULL if hEle is the last child.
 */

PSTR XML_Attribute(XMLDOC hXML, XMLELE hEle, CPSTR pszAttrName);
/* Retrieves the value of a named attribute of the given node, or NULL if
 * the selected node has no such attribute.
 */

#endif

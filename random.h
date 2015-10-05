/*================================================================================*/
/* Copyright (C) 2009, Don Milne.                                                 */
/* All rights reserved.                                                           */
/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
/*================================================================================*/

#ifndef RANDOM_H
#define RANDOM_H

#include "djtypes.h"

void Random_Randomize(UINT seed);
/* Seeds the random number generator. The seed can come from any source (eg. the
 * system clock, or you can pass a fixed value if you want a repeatable "random"
 * sequence for debug purposes.
 */

UINT Random_Integer(UINT range);
/* Returns a random integer, modulo "Range".
 */

#endif


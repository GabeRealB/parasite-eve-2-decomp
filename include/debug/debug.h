#ifndef DEBUG_DEBUG_H
#define DEBUG_DEBUG_H

/* The debug family's interface to the resident code. */

#include "common.h"

/// Enemy names in Shift-JIS, one per enemy kind, after an empty first entry.
/// `Gp_GetItemText` returns entry `id - 0x500` for an id of 0x500 or more, so
/// those ids name enemies while this package is loaded.
extern char* Gp_ItemTextHi[];

#endif /* DEBUG_DEBUG_H */

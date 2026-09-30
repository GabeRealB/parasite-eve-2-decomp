#ifndef GAMEPLAY_PRIVATE_MODEL_OBJECTS_H
#define GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/coord.h"

/// Bit 31 of `GfxCoord::composeStamp` records the last visiting pass's parity.
///
/// Every visit stores the composition pass counter's low bit (0 even, 1 odd),
/// including visits that reuse `workm` without changing the low 31-bit rebuild stamp.
enum { GRAPHICS_COORD_VISIT_PARITY_BIT = 0x80000000 };

extern const CVECTOR gGpColorGrey;

extern const CVECTOR gGpColorWhite;

/// Composes a coordinate and its ancestors relative to the excluded `root`.
///
/// `stamp` is the current pass counter's low 31 bits; `parity` is its low bit.
/// Ancestors are visited first. A node rebuilds when its rebuild stamp is older
/// than its parent's, or when its stamp is clear at the chain's end; an unchanged
/// node keeps its rebuild stamp. Every visit records the current parity in bit 31.
/// `NULL` excludes no ancestor. Parents must form a live, acyclic chain ending
/// at the supplied root, and callers clear `composeStamp` after local changes.
void _gpUpdateCoordTree(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root);

#endif // GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

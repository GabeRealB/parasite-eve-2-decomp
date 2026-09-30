#ifndef GAMEPLAY_PRIVATE_MODEL_OBJECTS_H
#define GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

/// Bit 31 of `GfxCoord::composeStamp` records the last visiting pass's parity.
///
/// Every visit stores the composition pass counter's low bit (0 even, 1 odd),
/// including visits that reuse `workm` without changing the low 31-bit rebuild stamp.
enum { GRAPHICS_COORD_VISIT_PARITY_BIT = 0x80000000 };

extern const CVECTOR gGpColorGrey;

extern const CVECTOR gGpColorWhite;

#endif // GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

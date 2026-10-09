#ifndef GAMEPLAY_PRIVATE_MODEL_OBJECTS_H
#define GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

/// Bit 31 of `GfxCoord::composeStamp` records the last visiting pass's parity.
///
/// Every visit stores the composition pass counter's low bit (0 even, 1 odd),
/// including visits that reuse `workm` without changing the low 31-bit rebuild stamp.
enum { GRAPHICS_COORD_VISIT_PARITY_BIT = 0x80000000 };

/// Neutral RGB material and grey reference for model-lighting colour blends.
///
/// Each 8-bit channel is 128, the texture-modulation unity level; code is zero.
/// Copied before GTE material lighting and used as the blend's grey endpoint.
extern const CVECTOR gModelLightingNeutralColor;

/// Maximum-channel white material for lit environment and offset layers.
///
/// Each 8-bit channel is 255 and the command byte is zero. Consumers copy it
/// before GTE lighting; this is distinct from modulation unity at 128.
extern const CVECTOR gModelLightingWhiteColor;

#endif // GAMEPLAY_PRIVATE_MODEL_OBJECTS_H

#ifndef GAMEPLAY_PRIVATE_PLANAR_REFLECTION_H
#define GAMEPLAY_PRIVATE_PLANAR_REFLECTION_H

#include "types.h"

/// Shared stage/area selector latched by player-reflection task initialization.
///
/// Stores the packed location masked by GAME_LOCATION_STAGE_AREA_MASK: stage
/// in bits 24..31 and area in bits 16..23. Later dispatches retain this location
/// despite live location changes. Initializing another reflection task replaces
/// the selector for every dispatcher. Storage persists while gameplay is loaded.
extern u32 gPlanarReflectionLocationKey;

#endif // GAMEPLAY_PRIVATE_PLANAR_REFLECTION_H

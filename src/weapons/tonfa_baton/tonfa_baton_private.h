#ifndef SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H
#define SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H

#include "types.h"

#include "main/coord.h"

/// The eight-segment swing trails, one array per end of the baton. Every entry
/// is parented to `gGfxViewCoord`.
extern GfxCoord gBladeTrailBase[8];

extern GfxCoord gBladeTrailTip[8];

/// Primitive/blend selector for the trail, seeded by state 0 from
/// `Task::spawnArg1` and passed to `_bladeTrailDraw` every frame.
extern s16 D_tonfa_baton_8012C0EC;

#endif // SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H

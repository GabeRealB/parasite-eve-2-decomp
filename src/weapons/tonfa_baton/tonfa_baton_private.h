#ifndef SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H
#define SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H

#include "types.h"

#include "main/coord.h"

/// The eight-segment swing trails, one array per end of the baton. Every entry
/// is parented to `gGfxViewCoord`.
extern GpCoord D_tonfa_baton_8012BBEC[8];

extern GpCoord D_tonfa_baton_8012BE6C[8];

/// Primitive/blend selector for the trail, seeded by state 0 from
/// `Task::spawnArg1` and passed to `func_tonfa_baton_8011D6B0` every frame.
extern s16 D_tonfa_baton_8012C0EC;

#endif // SRC_WEAPONS_TONFA_BATON_TONFA_BATON_PRIVATE_H

#ifndef SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H
#define SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H

#include "types.h"

#include "gameplay/effects.h"

#include "main/coord.h"
#include "main/task_types.h"

/// The eight-segment beam trails, one array per end of the blade. Every entry
/// is parented to `gGfxViewCoord`.
extern GfxCoord gBladeTrailBase[8];

extern GfxCoord gBladeTrailTip[8];

/// The running beam task and its `EffectWork`, cached on entry to state 0 so
/// `gunbladeRequestChargeFlash` can reach them from outside the task. The work
/// pointer is cleared again when that `EffectWork` is released.
extern Task* D_gunblade_8012E244;

extern EffectWork* D_gunblade_8012E248;

/// Requests a deferred charge flash on the active gunblade trail.
///
/// ammunitionIndex is PlayerStatus::weaponSlotItem, consumed unchanged by the
/// flash effect (13..15 select its three drawn grades). With no live trail this
/// is a no-op. Otherwise the trail's pending counter is incremented; its update
/// spawns the flash only when that counter equals one starting at age nine.
/// Keep the cached trail task/work live and the gunblade overlay loaded.
void gunbladeRequestChargeFlash(s32 ammunitionIndex);

#endif // SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H
